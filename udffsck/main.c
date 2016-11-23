/*
 * main.c
 *
 * Copyright (c) 2016    Vojtech Vladyka <vojtch.vladyka@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ecma_167.h>
#include <libudffs.h>

#include "udf.h"
#include "options.h"
#include "udffsck.h"
#include "utils.h"

#define PVD 0x10

#define BLOCK_SIZE 2048

int is_udf(int fp) {
    struct volStructDesc vsd;
    struct beginningExtendedAreaDesc bea;
    struct volStructDesc nsr;
    struct terminatingExtendedAreaDesc tea;
    
    udf_lseek64(fp,  PVD*BLOCK_SIZE, SEEK_SET); // default block size is 2048B, so PVD will be there
    
    
    for(int i = 0; i<6; i++) {
        printf("[DBG] try #%d\n", i);
        
        //printf("[DBG] address: 0x%x\n", (unsigned int)ftell(fp));
        read(fp, &vsd, sizeof(vsd)); // read its contents to vsd structure
        
        printf("[DBG] vsd: type:%d, id:%s, v:%d\n", vsd.structType, vsd.stdIdent, vsd.structVersion);
        
        
        if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BEA01, 5)) {
            //It's Extended area descriptor, so it might be UDF, check next sector
            memcpy(&bea, &vsd, sizeof(bea)); // store it for later 
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BOOT2, 5)) {
            fprintf(stderr, "BOOT2 found, unsuported for now.\n");
            return(-1);
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CD001, 5)) { 
            //CD001 means there is ISO9660, we try search for UDF at sector 18
            //TODO do check for other parameters here
            udf_lseek64(fp, BLOCK_SIZE, SEEK_CUR);
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CDW02, 5)) {
            fprintf(stderr, "CDW02 found, unsuported for now.\n");
            return(-1);
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR01, 5)) {
            memcpy(&nsr, &vsd, sizeof(nsr));
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR02, 5)) {
            memcpy(&nsr, &vsd, sizeof(nsr));
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR03, 5)) {
            memcpy(&nsr, &vsd, sizeof(nsr));
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_TEA01, 5)) {
            //We found TEA01, so we can end recognition sequence
            memcpy(&tea, &vsd, sizeof(tea));
            break;
        } else {
            fprintf(stderr, "Unknown identifier: %s. Exiting\n", vsd.stdIdent);
            return(-1);
        }  
    }
    
    printf("bea: type:%d, id:%s, v:%d\n", bea.structType, bea.stdIdent, bea.structVersion);
    printf("nsr: type:%d, id:%s, v:%d\n", nsr.structType, nsr.stdIdent, nsr.structVersion);
    printf("tea: type:%d, id:%s, v:%d\n", tea.structType, tea.stdIdent, tea.structVersion);

    return 0;
}


int detect_blocksize(int fd, struct udf_disc *disc)
{
	int size;
	uint16_t bs;

	int blocks;
#ifdef BLKGETSIZE64
	uint64_t size64;
#endif
#ifdef BLKGETSIZE
	long size;
#endif
#ifdef FDGETPRM
	struct floppy_struct this_floppy;
#endif
	struct stat buf;
    
    
    printf("detect_blocksize\n");

#ifdef BLKGETSIZE64
	if (ioctl(fd, BLKGETSIZE64, &size64) >= 0)
		size = size64;
	//else
#endif
#ifdef BLKGETSIZE
	if (ioctl(fd, BLKGETSIZE, &size) >= 0)
		size = size;
	//else
#endif
#ifdef FDGETPRM
	if (ioctl(fd, FDGETPRM, &this_floppy) >= 0)
		size = this_floppy.size
	//else
#endif
	//if (fstat(fd, &buf) == 0 && S_ISREG(buf.st_mode))
    //    size = buf.st_size;
	//else
#ifdef BLKSSZGET
	if (ioctl(fd, BLKSSZGET, &size) != 0)
	    size=size;
    printf("Error: %s\n", strerror(errno));
    printf("Block size: %d\n", size);
    /*
	disc->blocksize = size;
	for (bs=512,disc->blocksize_bits=9; disc->blocksize_bits<13; disc->blocksize_bits++,bs<<=1)
	{
		if (disc->blocksize == bs)
			break;
	}
	if (disc->blocksize_bits == 13)
	{
		disc->blocksize = 2048;
		disc->blocksize_bits = 11;
	}
	disc->udf_lvd[0]->logicalBlockSize = cpu_to_le32(disc->blocksize);*/
#endif

    return 2048;
}

int main(int argc, char *argv[]) {
    char *path = NULL;
    int fd;
    int status = 0;
    int blocksize = 0;
    struct udf_disc disc = {0};
    
    parse_args(argc, argv, &path, &blocksize);	

    if(strlen(path) == 0 || path == NULL) {
        fprintf(stderr, "No file given. Exiting.\n");
        exit(-1);
    }
    if(!(blocksize == 512 | blocksize == 1024 | blocksize == 2048 | blocksize == 4096)) {
        fprintf(stderr, "Invalid blocksize. Posible blocksizes are 512, 1024, 2048 and 4096.\n");
        exit(-2);
    }

    printf("File to analyze: %s\n", path);

    if ((fd = open(path, O_RDONLY, 0660)) == -1) {
        fprintf(stderr, "Error opening %s %s:", path, strerror(errno));
        return errno;
    } 

    printf("FD: 0x%x\n", fd);
    //blocksize = detect_blocksize(fd, NULL);

    status = is_udf(fd); //this function is checking for UDF recognition sequence. This part uses 2048B sector size.
    if(status) exit(status);
    status = get_avdp(fd, &disc, blocksize, FIRST_AVDP); //load AVDP
    if(status) exit(status);
    //status = get_pvd(fd, &disc, blocksize); //load PVD
    printf("\nTrying to load VDS\n");
    status = get_vds(fd, &disc, blocksize, MAIN_VDS); //load main VDS
    if(status) exit(status);
    status = get_vds(fd, &disc, blocksize, RESERVE_VDS); //load reserve VDS
    if(status) exit(status);
    
    close(fd);

    print_disc(&disc);

    printf("All done\n");
    return status;
}
