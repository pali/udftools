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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <limits.h>
#include <signal.h>

#include <ecma_167.h>
#include <ecma_119.h>
#include <libudffs.h>

#include "utils.h"
#include "options.h"
#include "udffsck.h"

//#define PVD 0x10

#define BLOCK_SIZE 2048

#define FSD_PRESENT 
#define PRINT_DISC 
//#define PATH_TABLE

#define MAX_VERSION 0x0201

void user_interrupt(int dummy) {
    warn("\nUser interrupted operation. Exiting.\n");
    exit(32);
}

void segv_interrupt(int dummy) {
    fatal("Unexpected error. Exiting.\n");
    exit(8);
}

int is_udf(uint8_t *dev, int *sectorsize, int force_sectorsize) {
    struct volStructDesc vsd;
    struct beginningExtendedAreaDesc bea;
    struct volStructDesc nsr;
    struct terminatingExtendedAreaDesc tea;
    int ssize = 512;
    int notFound = 0;
    int foundBEA = 0;


    for(int it=0; it<5; it++, ssize *= 2) {
        if(force_sectorsize) {
            ssize = *sectorsize;
            it = INT_MAX - 1; //End after this iteration
            dbg("Forced sectorsize\n");
        }
        
        dbg("Try sectorsize %d\n", ssize);

        for(int i = 0; i<6; i++) {
            dbg("try #%d at address 0x%x\n", i, 16*BLOCK_SIZE+i*ssize);

            //printf("[DBG] address: 0x%x\n", (unsigned int)ftell(fp));
            //read(fp, &vsd, sizeof(vsd)); // read its contents to vsd structure
            memcpy(&vsd, dev+16*BLOCK_SIZE+i*ssize, sizeof(vsd));

            dbg("vsd: type:%d, id:%s, v:%d\n", vsd.structType, vsd.stdIdent, vsd.structVersion);


            if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BEA01, 5)) {
                //It's Extended area descriptor, so it might be UDF, check next sector
                memcpy(&bea, &vsd, sizeof(bea)); // store it for later
                foundBEA = 1; 
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BOOT2, 5)) {
                err("BOOT2 found, unsuported for now.\n");
                return -1;
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CD001, 5)) { 
                //CD001 means there is ISO9660, we try search for UDF at sector 18
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CDW02, 5)) {
                err("CDW02 found, unsuported for now.\n");
                return -1;
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
                if(foundBEA) {
                    continue;
                }
                notFound = 1;
                break;
            } else {
                err("Unknown identifier: %s. Exiting\n", vsd.stdIdent);
                notFound = 1;
                break;
            }  
        }

        if(notFound) {
            notFound = 0;
            continue;
        }

        dbg("bea: type:%d, id:%s, v:%d\n", bea.structType, bea.stdIdent, bea.structVersion);
        dbg("nsr: type:%d, id:%s, v:%d\n", nsr.structType, nsr.stdIdent, nsr.structVersion);
        dbg("tea: type:%d, id:%s, v:%d\n", tea.structType, tea.stdIdent, tea.structVersion);

        *sectorsize = ssize;
        return 0;
    }
    
    err("Giving up VRS, maybe unclosed or bridged disc.\n");
    return 1;
}

#define ES_AVDP1 0x0001 
#define ES_AVDP2 0x0002
#define ES_PD    0x0004
#define ES_LVID  0x0008

/**
 * • 0 - No error
 * • 1 - Filesystem errors were fixed
 * • 2 - Filesystem errors were fixed, reboot is recomended
 * • 4 - Filesystem errors remained unfixed
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
    int blocksize = -1;
    struct udf_disc disc = {0};
    uint8_t *dev;
    //struct stat sb;
    off_t st_size;
    //metadata_err_map_t *seq;
    vds_sequence_t *seq; 
    struct filesystemStats stats =  {0};
    uint16_t error_status = 0;
    uint16_t fix_status = 0;
    int force_sectorsize = 0;

    int source = -1;

    signal(SIGINT, user_interrupt);
#ifndef DEBUG //if debugging, we want Address Sanitizer to catch those
    signal(SIGSEGV, segv_interrupt);
#endif

    parse_args(argc, argv, &path, &blocksize);	

    note("Verbose: %d, Autofix: %d, Interactive: %d\n", verbosity, autofix, interactive);

    if(path == NULL) {
        err("No medium given. Use -h for help.\n");
        exit(16);
    }
    
    if(blocksize > 0) {
        force_sectorsize = 1;
    }


    msg("Medium to analyze: %s\n", path);

    int prot = PROT_READ;
    int flags = O_RDONLY;
    // If is there some request for corrections, we need read/write access to medium
    if(interactive || autofix) {
        prot |= PROT_WRITE;
        flags = O_RDWR;
        dbg("RW\n");
    }

    if((fd = open(path, flags, 0660)) == -1) {
        fatal("Error opening %s: %s.", path, strerror(errno));
        exit(16);
    } 
    if((fp = fopen(path, "r")) == NULL) {
        fatal("Error opening %s: %s.", path, strerror(errno));
        exit(16);
    } 
    //Lock medium to ensure no-one is going to change during our operation. Make nonblocking, so it will fail when medium is already locked.
    if(flock(fd, LOCK_EX | LOCK_NB)) { 
        fatal("Error locking %s, %s. Is antoher process using it?\n", path, strerror(errno));
        exit(16);
    }

    note("FD: 0x%x\n", fd);

    if(fseeko(fp, 0 , SEEK_END) != 0) {
        if(errno == EBADF) {
            err("Medium is not seekable. Aborting.\n");
            exit(16);
        } else {
            err("Unknown seek error (errno: %d). Aborting.\n", errno);
            exit(16);
        }
    } 
    st_size = ftello(fp);
    dbg("Size: 0x%lx\n", (long)st_size);
    dev = (uint8_t *)mmap(NULL, st_size, prot, MAP_SHARED, fd, 0);
    if(dev == MAP_FAILED) {
        switch(errno) {
            case EACCES: dbg("EACCES\n"); break;
            case EAGAIN: dbg("EAGAIN\n"); break;
            case EBADF: dbg("EBADF\n"); break;
            case EINVAL: dbg("EINVAL\n"); break;
            case ENFILE: dbg("ENFILE\n"); break;
            case ENODEV: dbg("ENODEV\n"); break;
            case ENOMEM: dbg("ENOMEM\n"); break;
            case EPERM: dbg("EPERM\n"); break;
            case ETXTBSY: dbg("ETXTBSY\n"); break;
            case EOVERFLOW: dbg("EOVERFLOW\n"); break;
            default: dbg("EUnknown\n"); break;
        }


        fatal("Error maping %s: %s.\n", path, strerror(errno));
        exit(16);
    }

    // Unalloc path
    free(path);

    //------------- Detections -----------------------

    seq = calloc(1, sizeof(vds_sequence_t));
    //seq = calloc(1, sizeof(metadata_err_map_t));

    status = is_udf(dev, &blocksize, force_sectorsize); //this function is checking for UDF recognition sequence. It also tries to detect blocksize
    if(status < 0) {
        exit(status);
    } else if(status == 1) { //Unclosed or bridged medium 
        status = get_avdp(dev, &disc, &blocksize, st_size, -1, force_sectorsize); //load AVDP and verify blocksize
        source = FIRST_AVDP; // Unclosed medium have only one AVDP and that is saved at first position.
        if(status) {
            err("AVDP is broken. Aborting.\n");
            exit(4);
        }
    } else { //Normal medium
        seq->anchor[0].error = get_avdp(dev, &disc, &blocksize, st_size, FIRST_AVDP, force_sectorsize); //try load FIRST AVDP
        seq->anchor[1].error = get_avdp(dev, &disc, &blocksize, st_size, SECOND_AVDP, force_sectorsize); //load AVDP
        seq->anchor[2].error = get_avdp(dev, &disc, &blocksize, st_size, THIRD_AVDP, force_sectorsize); //load AVDP

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

    msg("Sectorsize: %d\n", blocksize);

    if(blocksize == -1) {
        err("Device blocksize is not defined. Please define it with -b BLOCKSIZE parameter\n");
        exit(16);
    }

    // Correct blocksize MUST be blocksize%512 == 0. We keep definitive list for now.
    if(!(blocksize == 512 | blocksize == 1024 | blocksize == 2048 | blocksize == 4096)) {
        err("Invalid blocksize. Posible blocksizes must be dividable by 512.\n");
        exit(16);
    }

    note("\nTrying to load VDS\n");
    status |= get_vds(dev, &disc, blocksize, source, MAIN_VDS, seq); //load main VDS
    status |= get_vds(dev, &disc, blocksize, source, RESERVE_VDS, seq); //load reserve VDS

    verify_vds(&disc, seq, MAIN_VDS, seq);
    verify_vds(&disc, seq, RESERVE_VDS, seq);

    status |= get_lvid(dev, &disc, blocksize, &stats, seq); //load LVID
    if(stats.minUDFReadRev > MAX_VERSION){
        err("Medium UDF revision is %04x and we are able to check up to %04x\n", stats.minUDFReadRev, MAX_VERSION);
        exit(8);
    }


#ifdef PRINT_DISC
    print_disc(&disc);
#endif

    //list_init(&stats.allocationTable);
    stats.blocksize = blocksize;

    if(get_pd(dev, &disc, blocksize, &stats, seq)) {
        err("PD error\n");
        exit(8);
    }

    uint32_t lbnlsn = 0;
    dbg("STATUS: 0x%02x\n", status);
    status |= get_fsd(dev, &disc, blocksize, &lbnlsn, &stats, seq);
    dbg("STATUS: 0x%02x\n", status);
    note("LBNLSN: %d\n", lbnlsn);
    status |= get_file_structure(dev, &disc, lbnlsn, &stats, seq);
    // if(status) exit(status);

    dbg("USD Alloc Descs\n");
    extent_ad *usdext;
    uint8_t *usdarr;
    for(int i=0; i<disc.udf_usd[0]->numAllocDescs; i++) {
        usdext = &disc.udf_usd[0]->allocDescs[i];
        dbg("Len: %d, Loc: 0x%x\n",usdext->extLength, usdext->extLocation);
        dbg("LSN loc: 0x%x\n", lbnlsn+usdext->extLocation);
        usdarr = (dev+(lbnlsn + usdext->extLocation)*blocksize);
        /*for(int j=0; j<usdext->extLength; ) {
          for(int k=0; k<2*8; k++,j++) {
          printf("%02x ", usdarr[j]);
          }
          printf("\n");
          }*/
    }

    dbg("PD PartitionsContentsUse\n");
    for(int i=0; i<128; ) {
        for(int j=0; j<8; j++, i++) {
            note("%02x ", disc.udf_pd[0]->partitionContentsUse[i]);
        }
        note("\n");
    }

    /*    if(get_pd(dev, &disc, blocksize, &stats)) {
          err("PD error\n");
          exit(8);
          }
          */
    //---------- Corrections --------------
    msg("\nFilesystem status\n-----------------\n");
    msg("Volume identifier: %s\n", stats.logicalVolIdent);
    msg("Next UniqueID: %d\n", stats.actUUID);
    msg("Max found UniqueID: %d\n", stats.maxUUID);
    msg("Last LVID recoreded change: %s\n", print_timestamp(stats.LVIDtimestamp));
    msg("expected number of files: %d\n", stats.expNumOfFiles);
    msg("expected number of dirs:  %d\n", stats.expNumOfDirs);
    msg("counted number of files: %d\n", stats.countNumOfFiles);
    msg("counted number of dirs:  %d\n", stats.countNumOfDirs);
    if(stats.expNumOfDirs != stats.countNumOfDirs || stats.expNumOfFiles != stats.countNumOfFiles) {
        seq->lvid.error |= E_FILES;
    }
    msg("UDF rev: min read:  %04x\n", stats.minUDFReadRev);
    msg("         min write: %04x\n", stats.minUDFWriteRev);
    msg("         max write: %04x\n", stats.maxUDFWriteRev);
    msg("Used Space: %lu (%lu)\n", stats.usedSpace, stats.usedSpace/blocksize);
    msg("Free Space: %lu (%lu)\n", stats.freeSpaceBlocks*blocksize, stats.freeSpaceBlocks);
    msg("Partition size: %lu (%lu)\n", stats.partitionSizeBlocks*blocksize, stats.partitionSizeBlocks);
    uint64_t expUsedSpace = (stats.partitionSizeBlocks-stats.freeSpaceBlocks)*blocksize;
    msg("Expected Used Space: %lu (%lu)\n", expUsedSpace, expUsedSpace/blocksize);
    msg("Expected Used Blocks: %d\nExpected Unused Blocks: %d\n", stats.expUsedBlocks, stats.expUnusedBlocks);
    int64_t usedSpaceDiff = expUsedSpace-stats.usedSpace;
    if(usedSpaceDiff != 0) {
        err("%d blocks is unused but not marked as unallocated in Free Space Table.\n", usedSpaceDiff/blocksize);
        err("Correct free space: %lu\n", stats.freeSpaceBlocks + usedSpaceDiff/blocksize);
        seq->lvid.error |= E_FREESPACE;
    }
    int32_t usedSpaceDiffBlocks = stats.expUsedBlocks - stats.usedSpace/blocksize;
    if(usedSpaceDiffBlocks != 0) {
        err("%d blocks is unused but not marked as unallocated in SBD.\n", usedSpaceDiffBlocks);
        seq->pd.error |= E_FREESPACE; 
    }

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
            exit(4);
        }

        if(target1 >= 0)
            error_status |= ES_AVDP1;

        if(target2 >= 0)
            error_status |= ES_AVDP2;

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
                } else {
                    imp("AVDP recovery was successful.\n");
                    error_status &= ~ES_AVDP1;
                    fix_status |= ES_AVDP1;
                } 
            } 
            if(target2 >= 0) {
                if(write_avdp(dev, &disc, blocksize, st_size, source, target2) != 0) {
                    fatal("AVDP recovery failed. Is medium writable?\n");
                } else {
                    imp("AVDP recovery was successful.\n");
                    error_status &= ~ES_AVDP2;
                    fix_status |= ES_AVDP2;
                }
            }
        }
    }


    print_metadata_sequence(seq);

    status |= fix_vds(dev, &disc, blocksize, source, seq, interactive, autofix); 

    int fixlvid = 0;
    int fixpd = 0;
    int lviderr = 0;
    if(seq->lvid.error == (E_CRC | E_CHECKSUM)) {
        //LVID is doomed.
        err("LVID is broken. Recovery is not possible.\n");
        error_status |= ES_LVID;
    } else {
        if(stats.maxUUID >= stats.actUUID || (seq->lvid.error & E_UUID)) {
            err("Max found Unique ID is same or bigger that Unique ID found at LVID.\n");
            lviderr = 1;
        }
        if(disc.udf_lvid->integrityType != LVID_INTEGRITY_TYPE_CLOSE) {
            //There are some unfinished writes
            err("Opened integrity type. Some writes may be unfinished.\n");
            lviderr = 1;
        }
        if(seq->lvid.error & E_TIMESTAMP) {
            err("LVID timestamp is older than timestamps of files.\n");
            lviderr=1;
        }
        if(seq->lvid.error & E_FILES) {
            err("Number of files or directories is not corresponding to counted number\n");
            lviderr=1;
        }
        if(seq->lvid.error & E_FREESPACE) {
            err("Free Space table is not corresponding to reality.\n");
            lviderr=1;
        }

        if(lviderr) {
            error_status |= ES_LVID;
            if(interactive) {
                if(prompt("Fix it? [Y/n]") != 0) {
                    fixlvid = 1;
                }
            }
            if(autofix)
                fixlvid = 1;
        }
    }

    if(seq->pd.error != 0) {
        error_status |= ES_PD;
        if(interactive) {
            if(prompt("Fix SBD? [Y/n]") != 0)
                fixpd = 1;
        }
        if(autofix)
            fixpd = 1;
    }


    if(fixlvid == 1) {
        if(fix_lvid(dev, &disc, blocksize, &stats, seq) == 0) {
            error_status &= ~(ES_LVID | ES_PD); 
            fix_status |= (ES_LVID | ES_PD);
        }
    } else if(fixlvid == 0 && fixpd == 1) {
        if(fix_pd(dev, &disc, blocksize, &stats, seq) == 0) {
            error_status &= ~(ES_PD); 
            fix_status |= ES_PD;
        }
    }

#ifdef DEBUG
    note("\n ACT \t EXP\n");
    uint32_t shift = 0;
    for(int i=0+shift, k=0+shift; i<stats.partitionSizeBlocks/8 && i < 100+shift; ) {
        for(int j=0; j<16; j++, i++) {
            note("%02x ", stats.actPartitionBitmap[i]);
        }
        note("| "); 
        for(int j=0; j<16; j++, k++) {
            note("%02x ", stats.expPartitionBitmap[k]);
        }
        note("\n");
    }
#endif

    //---------------- Error & Fix Status -------------
    if(error_status != 0) {
        status |= 4; //Errors remained unfixed
    }

    if(fix_status != 0) {
        status |= 1; // Errors were fixed
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
    free(stats.actPartitionBitmap);

    //list_destoy(&stats.allocationTable);

    flock(fd, LOCK_UN);
    close(fd);
    fclose(fp);

    msg("All done\n");
    return status;
}
