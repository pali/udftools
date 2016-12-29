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
#include <sys/mman.h>

#include <ecma_167.h>
#include <ecma_119.h>
#include <libudffs.h>

#include "udf.h"
#include "options.h"
#include "udffsck.h"
#include "utils.h"

//#define PVD 0x10

#define BLOCK_SIZE 2048

#define FSD_PRESENT 
//#define PRINT_DISC 
//#define PATH_TABLE

int is_udf(uint8_t *dev, uint32_t sectorsize) {
    struct volStructDesc vsd;
    struct beginningExtendedAreaDesc bea;
    struct volStructDesc nsr;
    struct terminatingExtendedAreaDesc tea;
    uint32_t bsize = sectorsize>BLOCK_SIZE ? sectorsize : BLOCK_SIZE; //It is possible to have free sectors between descriptors, but there can't be more than one descriptor in sector. Since there is requirement to comply with 2kB sectors, this is only way.   
    
    for(int i = 0; i<6; i++) {
        printf("[DBG] try #%d\n", i);
        
        //printf("[DBG] address: 0x%x\n", (unsigned int)ftell(fp));
        //read(fp, &vsd, sizeof(vsd)); // read its contents to vsd structure
        memcpy(&vsd, dev+16*BLOCK_SIZE+i*bsize, sizeof(vsd));

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
            //udf_lseek64(fp, BLOCK_SIZE, SEEK_CUR);
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
        } else if(vsd.stdIdent[0] == '\0') {
            fprintf(stderr, "Giving up VRS, maybe unclosed or bridged disc.\n");
            return 1;
        } else {
            fprintf(stderr, "Unknown identifier: %s. Exiting\n", vsd.stdIdent);
            return -1;
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
    uint8_t *dev;
    struct stat sb;

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

    fstat(fd, &sb);
    dev = (uint8_t *)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

    close(fd);
    
    status = is_udf(dev, blocksize); //this function is checking for UDF recognition sequence. This part uses 2048B sector size.
    if(status < 0) {
        exit(status);
    } else if(status == 1) { 
        status = get_avdp(dev, &disc, blocksize, sb.st_size, -1); //load AVDP
        if(status) exit(status);
    } else {
        status = get_avdp(dev, &disc, blocksize, sb.st_size, FIRST_AVDP); //load AVDP
        if(status) exit(status);
    }

    status = get_avdp(dev, &disc, blocksize, sb.st_size, SECOND_AVDP); //load AVDP
    if(status) exit(status);
    status = get_avdp(dev, &disc, blocksize, sb.st_size, THIRD_AVDP); //load AVDP
    if(status) exit(status);

    printf("\nTrying to load VDS\n");
    status = get_vds(dev, &disc, blocksize, MAIN_VDS); //load main VDS
    if(status) exit(status);
    status = get_vds(dev, &disc, blocksize, RESERVE_VDS); //load reserve VDS
    if(status) exit(status);
    status = get_lvid(dev, &disc, blocksize); //load LVID
    if(status) exit(status);

    verify_vds(&disc, MAIN_VDS);
    verify_vds(&disc, RESERVE_VDS);
    // SBD is not necessarily present, decide how to select
    // SBD with EFE are seen at r2.6 implementation
#ifdef SBD_PRESENT //FIXME Unfinished
    status = get_sbd(dev, &disc, SBD_STRUCTURE_HERE);
#endif

#ifdef FSD_PRESENT
    // FSD is not necessarily pressent, decide how to select
    // Seen at r1.5 implementations
    status = get_fsd(dev, &disc, blocksize);
    if(status) exit(status);
    status = get_file_structure(dev, &disc);
    if(status) exit(status);
#endif

#ifdef PATH_TABLE
    pathTableRec table[100]; 
    status = get_path_table(dev, blocksize, table);
    if(status) exit(status);
#endif

#ifdef PRINT_DISC
    print_disc(&disc);
#endif

    
    printf("All done\n");
    return status;
}
