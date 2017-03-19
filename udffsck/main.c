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
#define _POSIX_C_SOURCE 200808L

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

#include "options.h"
#include "udffsck.h"
#include "utils.h"

//#define PVD 0x10

#define BLOCK_SIZE 2048

#define FSD_PRESENT 
#define PRINT_DISC 
//#define PATH_TABLE

#define MAX_VERSION 3


int is_udf(uint8_t *dev, uint32_t sectorsize) {
    struct volStructDesc vsd;
    struct beginningExtendedAreaDesc bea;
    struct volStructDesc nsr;
    struct terminatingExtendedAreaDesc tea;
    uint32_t bsize = sectorsize>BLOCK_SIZE ? sectorsize : BLOCK_SIZE; //It is possible to have free sectors between descriptors, but there can't be more than one descriptor in sector. Since there is requirement to comply with 2kB sectors, this is only way.   

    for(int i = 0; i<6; i++) {
        printf("[DBG] try #%d at address 0x%x\n", i, 16*BLOCK_SIZE+i*bsize);

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

    if( (int)( (nsr.stdIdent)[4]-'0') > MAX_VERSION) {
        err("Medium is newer than supported version. We can verify medium up to NSR0%d\n", MAX_VERSION);
        return -1;
    }

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

/**
 * • 0 - No error
 * • 1 - Filesystem seq were fixed
 * • 2 - Filesystem seq were fixed, reboot is recomended
 * • 4 - Filesystem seq remained unfixed
 * • 8 - Program error
 * • 16 - Wrong input parameters
 * • 32 - Check was interrupted by user request
 * • 128 - Shared library error
 */
int main(int argc, char *argv[]) {
    char *path = NULL;
    int fd;
    FILE *fp;
    int status = 0;
    int blocksize = 0;
    struct udf_disc disc = {0};
    uint8_t *dev;
    //struct stat sb;
    off_t st_size;
    //metadata_err_map_t *seq;
    vds_sequence_t *seq; 

    int source = -1;

    parse_args(argc, argv, &path, &blocksize);	

    note("Verbose: %d, Autofix: %d, Interactive: %d\n", verbose, autofix, interactive);

    if(strlen(path) == 0 || path == NULL) {
        err("No file given. Exiting.\n");
        exit(16);
    }
    if(!(blocksize == 512 | blocksize == 1024 | blocksize == 2048 | blocksize == 4096)) {
        err("Invalid blocksize. Posible blocksizes are 512, 1024, 2048 and 4096.\n");
        exit(16);
    }

    msg("File to analyze: %s\n", path);


    int prot = PROT_READ;
    int flags = O_RDONLY;
    // If is there some request for corrections, we need read/write access to medium
    if(interactive || autofix) {
        prot |= PROT_WRITE;
        flags = O_RDWR;
        note("RW\n");
    }

    if ((fd = open(path, flags, 0660)) == -1) {
        fatal("Error opening %s: %s.", path, strerror(errno));
        exit(16);
    } 
    if ((fp = fopen(path, "r")) == NULL) {
        fatal("Error opening %s: %s.", path, strerror(errno));
        exit(16);
    } 

    note("FD: 0x%x\n", fd);
    //blocksize = detect_blocksize(fd, NULL);

    //stat(path, &sb);
    if(fseeko(fp, 0 , SEEK_END) != 0) {
        /* Handle error */
    } 
    st_size = ftello(fp);
    printf("Size: 0x%lx\n", (long)st_size);
    dev = (uint8_t *)mmap(NULL, st_size, prot, MAP_SHARED, fd, 0);
    if(dev == MAP_FAILED) {
        switch(errno) {
            case EACCES: printf("EACCES\n"); break;
            case EAGAIN: printf("EAGAIN\n"); break;
            case EBADF: printf("EBADF\n"); break;
            case EINVAL: printf("EINVAL\n"); break;
            case ENFILE: printf("ENFILE\n"); break;
            case ENODEV: printf("ENODEV\n"); break;
            case ENOMEM: printf("ENOMEM\n"); break;
            case EPERM: printf("EPERM\n"); break;
            case ETXTBSY: printf("ETXTBSY\n"); break;
            case EOVERFLOW: printf("EOVERFLOW\n"); break;
            default: printf("EUnknown\n"); break;
        }
    

        fatal("Error maping %s: %s.", path, strerror(errno));
        exit(16);
    }

    // Close FD. It is kept by mmap now.
    close(fd);
    // Unalloc path
    free(path);

    //------------- Detections -----------------------

    seq = calloc(1, sizeof(vds_sequence_t));
    //seq = calloc(1, sizeof(metadata_err_map_t));

    status = is_udf(dev, blocksize); //this function is checking for UDF recognition sequence. This part uses 2048B sector size.
    if(status < 0) {
        exit(status);
    } else if(status == 1) { //Unclosed or bridged medium 
        status = get_avdp(dev, &disc, blocksize, st_size, -1); //load AVDP
        source = FIRST_AVDP; // Unclosed medium have only one AVDP and that is saved at first position.
        if(status) {
            err("AVDP is broken. Aborting.\n");
            exit(4);
        }
    } else { //Normal medium
        seq->anchor[0].error = get_avdp(dev, &disc, blocksize, st_size, FIRST_AVDP); //try load FIRST AVDP
        seq->anchor[1].error = get_avdp(dev, &disc, blocksize, st_size, SECOND_AVDP); //load AVDP
        seq->anchor[2].error = get_avdp(dev, &disc, blocksize, st_size, THIRD_AVDP); //load AVDP

        if(seq->anchor[0].error == 0) {
            source = FIRST_AVDP;
        } else if(seq->anchor[1].error == 0) {
            source = SECOND_AVDP;
        } else if(seq->anchor[2].error == 0) {
            source = THIRD_AVDP;
        } else {
            err("All AVDP are broken. Aborting.\n");
            exit(4);
        }
    }


    note("\nTrying to load VDS\n");
    status = get_vds(dev, &disc, blocksize, source, MAIN_VDS, seq); //load main VDS
    if(status) exit(status);
    status = get_vds(dev, &disc, blocksize, source, RESERVE_VDS, seq); //load reserve VDS
    if(status) exit(status);


    status = get_lvid(dev, &disc, blocksize); //load LVID
    if(status) exit(status);

    verify_vds(&disc, seq, MAIN_VDS, seq);
    verify_vds(&disc, seq, RESERVE_VDS, seq);

#ifdef PRINT_DISC
    print_disc(&disc);
#endif

    // SBD is not necessarily present, decide how to select
    // SBD with EFE are seen at r2.6 implementation
#ifdef SBD_PRESENT //FIXME Unfinished
    status = get_sbd(dev, &disc, SBD_STRUCTURE_HERE);
#endif

#ifdef FSD_PRESENT
    // FSD is not necessarily pressent, decide how to select
    // Seen at r1.5 implementations
    uint32_t lbnlsn = 0;
    status = get_fsd(dev, &disc, blocksize, &lbnlsn);
    //if(status) exit(status);
    note("LBNLSN: %d\n", lbnlsn);
    status = get_file_structure(dev, &disc, lbnlsn);
    if(status) exit(status);
#endif

#ifdef PATH_TABLE //FIXME Remove it. Not needed.
    pathTableRec table[100]; 
    status = get_path_table(dev, blocksize, table);
    if(status) exit(status);
#endif

    //---------- Corrections --------------

    if(seq->anchor[0].error + seq->anchor[1].error + seq->anchor[2].error != 0) { //Something went wrong with AVDPs
        int target1 = -1;
        int target2 = -1;

        if(seq->anchor[0].error == 0) {
            source = FIRST_AVDP;
            if(seq->anchor[1].error != 0)
                target1 = SECOND_AVDP;
            if(seq->anchor[2].error != 0)
                target2 = THIRD_AVDP;
        } else if(seq->anchor[1].error == 0) {
            source = SECOND_AVDP;
            target1 = FIRST_AVDP;
            if(seq->anchor[2].error != 0)
                target2 = THIRD_AVDP;
        } else if(seq->anchor[2].error == 0) {
            source = THIRD_AVDP;
            target1 = FIRST_AVDP;
            target2 = SECOND_AVDP;
        } else {
            err("Unrecoverable AVDP failure. Aborting.\n");
            exit(0);
        }

        int fix_avdp = 0;
        if(interactive) {
            if(prompt("Found error at AVDP. Do you want to fix them? [Y/n]") != 0) {
                fix_avdp = 1;
            }
        }
        if(autofix)
            fix_avdp = 1;

        if(fix_avdp) {
            msg("Source: %d, Target1: %d, Target2: %d\n", source, target1, target2);
            if(target1 >= 0) {
                if(write_avdp(dev, &disc, blocksize, st_size, source, target1) != 0) {
                    fatal("AVDP recovery failed. Is medium writable?\n");
                } 
            } 
            if(target2 >= 0) {
                if(write_avdp(dev, &disc, blocksize, st_size, source, target2) != 0) {
                    fatal("AVDP recovery failed. Is medium writable?\n");
                }
            }
        }
    }


    print_metadata_sequence(seq);

    fix_vds(dev, &disc, blocksize, source, seq, interactive, autofix); 
    
    
    if(seq->lvid.error != 0) {
        //LVID is doomed.
        err("LVID is broken. Recovery is not possible.\n");
    } else {
        if(disc.udf_lvid->integrityType == LVID_INTEGRITY_TYPE_OPEN) {
            //There are some unfinished writes
            err("Opened integrity type. Some writes may be unfinished.\n");
        } else if(disc.udf_lvid->integrityType == LVID_INTEGRITY_TYPE_CLOSE) {
            //Evrything is closed. Continue.
        } else {
            //Unknown type. It is just wrong.
            err("Unknown integrity type: %d\n", disc.udf_lvid->integrityType);
        }
    }
   


    
    //---------------- Clean up -----------------

    note("Clean allocations\n");
    free(disc.udf_anchor[0]);
    free(disc.udf_anchor[1]);
    free(disc.udf_anchor[2]);
    
    free(disc.udf_pvd[0]);
    free(disc.udf_lvd[0]);
    free(disc.udf_usd[0]);
    free(disc.udf_iuvd[0]);
    free(disc.udf_pd[0]);
    free(disc.udf_td[0]);

    free(disc.udf_pvd[1]);
    free(disc.udf_lvd[1]);
    free(disc.udf_usd[1]);
    free(disc.udf_iuvd[1]);
    free(disc.udf_pd[1]);
    free(disc.udf_td[1]);

    free(disc.udf_lvid);
    free(disc.udf_fsd);

    free(seq);


    msg("All done\n");
    return status;
}
