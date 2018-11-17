/*
 * Copyright (C) 2017 Vojtech Vladyka <vojtech.vladyka@gmail.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _POSIX_C_SOURCE 200808L
#define _DEFAULT_SOURCE

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
#include <mntent.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <ecma_167.h>
#include <libudffs.h>

#include "utils.h"
#include "options.h"
#include "udffsck.h"


#define PRINT_DISC 

#define MAX_VERSION 0x0201

/**
 * \brief User interrupt hander (Ctrl + C or SIGINT)
 *
 * After calling this, program exits with code 32 (User interrupt).
 */
void user_interrupt(int dummy) {
    (void)dummy;
    warn("\nUser interrupted operation. Exiting.\n");
    exit(32);
}

/**
 * \brief Segmentation fault handler
 *
 * If program dies because of SEGV, this handler catches it and die properly.
 * It instructs user to report it, because this behavior is bug.
 */
void segv_interrupt(int dummy) {
    (void)dummy;
    fatal("Unexpected error (SEGV), please report it. More info at man page. Exiting.\n");
    exit(8);
}

/**
 * \brief Bus error handler
 *
 * If program dies because of SIGBUS, this handler catches it and die properly.
 * It instructs user to report it, because this behavior is bug.
 */
void sigbus_interrupt(int dummy) {
    (void)dummy;
    fatal("Medium changed its size during fsck run. Is somebody manipulating with it? Exiting.\n");
    exit(8);
}

/**
 * \brief Return non-zero when error found in any generic descriptor (e.g. not in filetree)
 *
 * \param[in] seq Error sequence structure
 *
 * \return 0 all descriptors are Ok
 */
int any_error(vds_sequence_t *seq) {
    int status = 0;

    status |= seq->anchor[0].error;
    status |= seq->anchor[1].error;
    if(seq->anchor[2].error != 255)
        status |= seq->anchor[2].error;
    status |= seq->lvid.error;
    status |= seq->pd.error;

    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        status |= seq->main[MAIN_VDS].error;
        status |= seq->main[RESERVE_VDS].error;
    }

    return status;
}

#define ES_AVDP1 0x0001 
#define ES_AVDP2 0x0002
#define ES_PD    0x0004
#define ES_LVID  0x0008

/**
 * \brief **udffsck** entry point
 *
 * This is entry point for **udffsck**. It contains structure of whole tool and calls 
 * function from udffsck.c
 *
 * It accepts inputs via argv.
 *
 * \return 0 -- No error
 * \return 1 -- Filesystem errors were fixed
 * \return 2 -- Filesystem errors were fixed, reboot is recomended (Unsupported)
 * \return 4 -- Filesystem errors remained unfixed
 * \return 8 -- Program error
 * \return 16 -- Wrong input parameters
 * \return 32 -- Check was interrupted by user request
 * \return 128 -- Shared library error (Unsupported)
 */
int main(int argc, char *argv[]) {
    char *path = NULL;
    int fd;
    FILE *fp;
    int status = 0;
    int blocksize = -1;
    struct udf_disc disc;
    memset(&disc, 0, sizeof(struct udf_disc));
    struct stat stat;
    uint8_t **dev;
    off_t st_size;
    vds_sequence_t *seq; 
    struct filesystemStats stats;
    memset(&stats, 0, sizeof(struct filesystemStats));
    uint16_t error_status = 0;
    uint16_t fix_status = 0;
    int force_sectorsize = 0;
    int third_avdp_missing = 0;
    struct sigaction new_action;
    int source = -1;

    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;

    new_action.sa_handler = user_interrupt;
    sigaction (SIGINT, &new_action, NULL);

    new_action.sa_handler = sigbus_interrupt;
    sigaction (SIGBUS, &new_action, NULL);

#ifndef DEBUG //if debugging, we want Address Sanitizer to catch those
    new_action.sa_handler = segv_interrupt;
    sigaction (SIGSEGV, &new_action, NULL);
#endif

    parse_args(argc, argv, &path, &blocksize);
#ifdef MEMTRACE
    dbg("Path: %p\n", path);    
#endif

    note("Verbose: %d, Autofix: %d, Interactive: %d\n", verbosity, autofix, interactive);
    if(fast_mode)
        warn("Fast mode active. File tree checks will be skipped if rest of filesystem will be clean.\n");

    if(path == NULL) {
        err("No medium given. Use -h for help.\n");
        exit(16);
    }

    if(blocksize > 0) {
        force_sectorsize = 1;
    }

    msg("Medium to analyze: %s\n", path);

    /* TODO remove. Replaced by check during opening
    //Check if medium is mounted or not
    FILE* mtab = setmntent("/proc/mounts", "r");
    struct mntent* m;
    struct mntent mnt;
    char strings[4096];
    while ((m = getmntent_r(mtab, &mnt, strings, sizeof(strings)))) {
    dbg("%s\n", mnt.mnt_fsname);
    if(strcmp(mnt.mnt_fsname, path) == 0) { //Match
    err("Medium is mounted, therefore cannot be checked. Exiting.\n");
    exit(16);
    }
    }

    endmntent(mtab);
    */

    int flags = O_RDONLY;
    // If is there some request for corrections, we need read/write access to medium
    if(interactive || autofix) {
        flags = O_RDWR;
        dbg("RW\n");
    }

    if((fd = open(path, flags, 0660)) == -1) {
        fatal("Error opening %s: %s.", path, strerror(errno));
        exit(16);
    } else {
        int fd2;
        int flags2;
        char filename2[64];
        const char *error;

        if (fstat(fd, &stat) != 0) {
            fatal("Cannot stat device '%s': %s\n", path, strerror(errno));
            exit(16);
        }

        flags2 = flags; 
        if (snprintf(filename2, sizeof(filename2), "/proc/self/fd/%d", fd) >= (int)sizeof(filename2))
        {
            fatal("Cannot open device '%s': %s\n", path, strerror(ENAMETOOLONG));
            exit(16);
        }

        // Re-open block device with O_EXCL mode which fails when device is already mounted
        if (S_ISBLK(stat.st_mode))
            flags2 |= O_EXCL;

        fd2 = open(filename2, flags2);
        if (fd2 < 0)
        {
            if (errno != ENOENT)
            {
                error = (errno != EBUSY) ? strerror(errno) : "Device is mounted or mkudffs is already running";
                fatal("Cannot open device '%s': %s\n", path, error);
                exit(16);
            }

            // Fallback to orignal filename when /proc is not available, but this introduce race condition between stat and open
            fd2 = open(path, flags2);
            if (fd2 < 0)
            {
                error = (errno != EBUSY) ? strerror(errno) : "Device is mounted or mkudffs is already running";
                fatal("Cannot open device '%s': %s\n", path, error);
                exit(16);
            }
        }

        close(fd);
        fd = fd2;

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

    uint32_t chunksize = CHUNK_SIZE;
    uint64_t rest = st_size%chunksize;
    dbg("Chunk size %ld, rest: %ld\n", chunksize, rest);
    dev = calloc(sizeof(uint8_t *), st_size/chunksize + (rest > 0 ? 1 : 0));
    dbg("Amount of chunks: %d\n", st_size/chunksize + (rest > 0 ? 1 : 0));
    for(uint64_t i=0; i<(uint64_t)(st_size/chunksize +(rest > 0 ? 1 : 0)) ; i++) {
        dev[i] = NULL;
    }

    //------------- Detections -----------------------

    seq = calloc(1, sizeof(vds_sequence_t));

    stats.AVDPSerialNum = 0xFFFF;
    status = is_udf(fd, dev, &blocksize, st_size, force_sectorsize); //this function is checking for UDF recognition sequence. It also tries to detect blocksize
    if(status < 0) {
        exit(status);
    } else if(status == 1) { //Unclosed or bridged medium 
        status = get_avdp(fd, dev, &disc, &blocksize, st_size, -1, force_sectorsize, &stats); //load AVDP and verify blocksize
        source = FIRST_AVDP; // Unclosed medium have only one AVDP and that is saved at first position.
        if(status) {
            err("AVDP is broken. Aborting.\n");
            exit(4);
        }
    } else { //Normal medium
        seq->anchor[0].error = get_avdp(fd, dev, &disc, &blocksize, st_size, FIRST_AVDP, force_sectorsize, &stats); //try load FIRST AVDP
        if(seq->anchor[0].error) {
            err("AVDP[0] is broken.\n");
        } else {
            force_sectorsize = 1;
        }

        seq->anchor[1].error = get_avdp(fd, dev, &disc, &blocksize, st_size, SECOND_AVDP, force_sectorsize, &stats); //load AVDP
        if(seq->anchor[1].error) {
            err("AVDP[1] is broken.\n");
        } else {
            force_sectorsize = 1;
        }

        seq->anchor[2].error = get_avdp(fd, dev, &disc, &blocksize, st_size, THIRD_AVDP, force_sectorsize, &stats); //load AVDP
        if(seq->anchor[2].error) {
            dbg("AVDP[2] somehow errored, not necessarily bad thing.\n");
            if(seq->anchor[2].error < 255) { //Third AVDP is not necessarily present.
                err("AVDP[2] is broken.\n");
            } else {
                third_avdp_missing = 1;
            }
        }

        if((seq->anchor[0].error & ~E_EXTLEN) == 0) {
            dbg("FIRST AVDP as source\n");
            source = FIRST_AVDP;
        } else if((seq->anchor[1].error & ~E_EXTLEN) == 0) {
            dbg("SECOND AVDP as source\n");
            source = SECOND_AVDP;
        } else if((seq->anchor[2].error & ~E_EXTLEN) == 0 && third_avdp_missing == 0) {
            dbg("THIRD AVDP as source\n");
            source = THIRD_AVDP;
        } else {
            if(force_sectorsize)
                err("All AVDP are broken or wrong block size was entered. Try running without -B option. Aborting.\n");
            else
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
    if(!((blocksize == 512) | (blocksize == 1024) | (blocksize == 2048) | (blocksize == 4096))) {
        err("Invalid blocksize. Posible blocksizes must be dividable by 512.\n");
        exit(16);
    }

    note("\nTrying to load first VDS\n");
    status |= get_vds(fd, dev, &disc, blocksize, st_size, source, MAIN_VDS, seq); //load main VDS
    note("\nTrying to load second VDS\n");
    status |= get_vds(fd, dev, &disc, blocksize, st_size, source, RESERVE_VDS, seq); //load reserve VDS

    dbg("First VDS verification\n");
    verify_vds(&disc, MAIN_VDS, seq, &stats);
    dbg("Second VDS verification\n");
    verify_vds(&disc, RESERVE_VDS, seq, &stats);

    //Check if blocksizes matches. If not, exit.
    int blocksize_status = check_blocksize(fd, dev, &disc, blocksize, force_sectorsize, seq);
    if(blocksize_status != 0)
        exit(status | blocksize_status);


    status |= get_lvid(fd, dev, &disc, blocksize, st_size, &stats, seq); //load LVID
    if(stats.minUDFReadRev > MAX_VERSION){
        err("Medium UDF revision is %04x and we are able to check up to %04x\n", stats.minUDFReadRev, MAX_VERSION);
        exit(8);
    }

#ifdef PRINT_DISC
    print_disc(&disc);
#endif

    stats.blocksize = blocksize;

    if(get_pd(fd, dev, &disc, blocksize, st_size, &stats, seq)) {
        err("PD error\n");
        exit(8);
    }

    uint32_t lbnlsn = 0;
    dbg("STATUS: 0x%02x\n", status);
    status |= get_fsd(fd, dev, &disc, blocksize, st_size, &lbnlsn, &stats, seq);
    dbg("STATUS: 0x%02x\n", status);
    if(status >= 8) {
        err("Unable to continue without FSD. Consider submitting bug report. Exiting.\n");
        exit(status);
    } else if(status >= 4) {
        err("Unable to continue without FSD. Medium seems unrecoverable. Exiting.\n");
        exit(status);
    }

    note("LBNLSN: %d\n", lbnlsn);
    if(any_error(seq) || disc.udf_lvid->integrityType != LVID_INTEGRITY_TYPE_CLOSE || fast_mode == 0) {
        status |= get_file_structure(fd, dev, &disc, st_size, lbnlsn, &stats, seq);
    }

    dbg("PD PartitionsContentsUse\n");
    for(int i=0; i<128; ) {
        for(int j=0; j<8; j++, i++) {
            note("%02x ", disc.udf_pd[0]->partitionContentsUse[i]);
        }
        note("\n");
    }

    uint64_t countedBits = count_used_bits(&stats);
    dbg("**** BITMAP USED SPACE: %d ****\n", countedBits);

    //---------- Corrections --------------
    msg("\nFilesystem status\n-----------------\n");
    get_volume_identifier(&disc, &stats, seq);  
    msg("Volume set identifier: %s\n", stats.volumeSetIdent);
    msg("Partition identifier: %s\n", stats.partitionIdent);
    msg("Next UniqueID: %d\n", stats.actUUID);
    if(fast_mode == 0) {
        msg("Max found UniqueID: %d\n", stats.maxUUID);
    }
    msg("Last LVID recoreded change: %s\n", print_timestamp(stats.LVIDtimestamp));
    msg("expected number of files: %d\n", stats.expNumOfFiles);
    msg("expected number of dirs:  %d\n", stats.expNumOfDirs);
    if(fast_mode == 0) {
        msg("counted number of files: %d\n", stats.countNumOfFiles);
        msg("counted number of dirs:  %d\n", stats.countNumOfDirs);
        if(stats.expNumOfDirs != stats.countNumOfDirs || stats.expNumOfFiles != stats.countNumOfFiles) {
            seq->lvid.error |= E_FILES;
        }
    }
    msg("UDF rev: min read:  %04x\n", stats.minUDFReadRev);
    msg("         min write: %04x\n", stats.minUDFWriteRev);
    msg("         max write: %04x\n", stats.maxUDFWriteRev);
    if(fast_mode == 0) {
        msg("Used Space: %"PRIu64" (%"PRIu64")\n", stats.usedSpace, stats.usedSpace/blocksize);
    }
    msg("Free Space: %"PRIu64" (%"PRIu64")\n", (uint64_t)(stats.freeSpaceBlocks)*(uint64_t)(blocksize), (uint64_t)(stats.freeSpaceBlocks));
    msg("Partition size: %"PRIu64" (%"PRIu64")\n", (uint64_t)(stats.partitionSizeBlocks)*(uint64_t)(blocksize), (uint64_t)(stats.partitionSizeBlocks));
    uint64_t expUsedSpace = 0;
    if(fast_mode == 0) {
        expUsedSpace = (stats.partitionSizeBlocks-stats.freeSpaceBlocks)*blocksize;
        msg("Expected Used Space: %"PRIu64" (%"PRIu64")\n", (uint64_t)expUsedSpace, (uint64_t)(expUsedSpace)/(uint64_t)(blocksize));
        msg("Expected Used Blocks: %d\nExpected Unused Blocks: %d\n", stats.expUsedBlocks, stats.expUnusedBlocks);
    }
    if(fast_mode == 0) {
        int64_t usedSpaceDiff = expUsedSpace-stats.usedSpace;
        if(usedSpaceDiff != 0) {
            err("%d blocks is unused but not marked as unallocated in Free Space Table.\n", usedSpaceDiff/blocksize);
            err("Correct free space: %lu\n", stats.freeSpaceBlocks + usedSpaceDiff/blocksize);
            seq->lvid.error |= E_FREESPACE;
        }
        int32_t usedSpaceDiffBlocks = stats.expUsedBlocks - countedBits;//stats.usedSpace/blocksize;
        if(usedSpaceDiffBlocks != 0) {
            err("%d blocks is unused but not marked as unallocated in SBD.\n", usedSpaceDiffBlocks);
            seq->pd.error |= E_FREESPACE; 
        }
    }

    status |= dstring_error("FSD, Logical Volume Identifier", stats.dstringFSDLogVolIdentErr);
    status |= dstring_error("FSD, File Set Identifier", stats.dstringFSDFileSetIdentErr);
    status |= dstring_error("FSD, Copyright File Identifier", stats.dstringFSDCopyrightFileIdentErr);
    status |= dstring_error("FSD, Abstract File Identifier", stats.dstringFSDAbstractFileIdentErr);

    status |= dstring_error("PVD, Main VDS, Volume Identifier", stats.dstringPVDVolIdentErr[MAIN_VDS]);
    status |= dstring_error("PVD, Main VDS, Volume Set Identifier", stats.dstringPVDVolSetIdentErr[MAIN_VDS]);
    status |= dstring_error("LVD, Main VDS, Logical Volume Identifier", stats.dstringLVDLogicalVolIdentErr[MAIN_VDS]);
    status |= dstring_error("IUVD, Main VDS, Logical Volume Info 1", stats.dstringIUVDLVInfo1Err[MAIN_VDS]);
    status |= dstring_error("IUVD, Main VDS, Logical Volume Info 2", stats.dstringIUVDLVInfo2Err[MAIN_VDS]);
    status |= dstring_error("IUVD, Main VDS, Logical Volume Info 3", stats.dstringIUVDLVInfo3Err[MAIN_VDS]);
    status |= dstring_error("IUVD, Main VDS, Logical Volume Identifier", stats.dstringIUVDLogicalVolIdentErr[MAIN_VDS]);
     
    status |= dstring_error("PVD, Reserve VDS, Volume Identifier", stats.dstringPVDVolIdentErr[RESERVE_VDS]);
    status |= dstring_error("PVD, Reserve VDS, Volume Set Identifier", stats.dstringPVDVolSetIdentErr[RESERVE_VDS]);
    status |= dstring_error("LVD, Reserve VDS, Logical Volume Identifier", stats.dstringLVDLogicalVolIdentErr[RESERVE_VDS]);
    status |= dstring_error("IUVD, Reserve VDS, Logical Volume Info 1", stats.dstringIUVDLVInfo1Err[RESERVE_VDS]);
    status |= dstring_error("IUVD, Reserve VDS, Logical Volume Info 2", stats.dstringIUVDLVInfo2Err[RESERVE_VDS]);
    status |= dstring_error("IUVD, Reserve VDS, Logical Volume Info 3", stats.dstringIUVDLVInfo3Err[RESERVE_VDS]);
    status |= dstring_error("IUVD, Reserve VDS, Logical Volume Identifier", stats.dstringIUVDLogicalVolIdentErr[RESERVE_VDS]);

    if(seq->anchor[0].error + seq->anchor[1].error + seq->anchor[2].error != 0) { //Something went wrong with AVDPs
        int target1 = -1;
        int target2 = -1;

        if((seq->anchor[0].error & ~E_EXTLEN) == 0) {
            source = FIRST_AVDP;
            if((seq->anchor[1].error & ~E_EXTLEN) != 0)
                target1 = SECOND_AVDP;
            if((seq->anchor[2].error & ~E_EXTLEN) != 0 && third_avdp_missing == 0)
                target2 = THIRD_AVDP;
        } else if((seq->anchor[1].error & ~E_EXTLEN) == 0) {
            source = SECOND_AVDP;
            target1 = FIRST_AVDP;
            if((seq->anchor[2].error & ~E_EXTLEN) != 0 && third_avdp_missing == 0)
                target2 = THIRD_AVDP;
        } else if((seq->anchor[2].error & ~E_EXTLEN) == 0 && third_avdp_missing == 0) {
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

        int fixavdp = 0;
        if(interactive) {
            if(prompt("Found error at AVDP. Do you want to fix them? [Y/n]") != 0) {
                fixavdp = 1;
            }
        }
        if(autofix)
            fixavdp = 1;


        if(fixavdp) {
            msg("Source: %d, Target1: %d, Target2: %d\n", source, target1, target2);
            if(target1 >= 0) {
                if(write_avdp(fd, dev, &disc, blocksize, st_size, source, target1) != 0) {
                    fatal("AVDP recovery failed. Is medium writable?\n");
                } else {
                    imp("AVDP recovery was successful.\n");
                    error_status &= ~ES_AVDP1;
                    fix_status |= ES_AVDP1;
                } 
            } 
            if(target2 >= 0) {
                if(write_avdp(fd, dev, &disc, blocksize, st_size, source, target2) != 0) {
                    fatal("AVDP recovery failed. Is medium writable?\n");
                } else {
                    imp("AVDP recovery was successful.\n");
                    error_status &= ~ES_AVDP2;
                    fix_status |= ES_AVDP2;
                }
            }
        }

        if(fixavdp) {
            if(seq->anchor[0].error & E_EXTLEN) {
                status |= fix_avdp(fd, dev, &disc, blocksize, st_size, FIRST_AVDP);                 
            }
            if(seq->anchor[1].error & E_EXTLEN) {
                status |= fix_avdp(fd, dev, &disc, blocksize, st_size, SECOND_AVDP);                 
            }
            if((seq->anchor[2].error & E_EXTLEN) && third_avdp_missing == 0) {
                status |= fix_avdp(fd, dev, &disc, blocksize, st_size, THIRD_AVDP);                 
            }
        }

    }


    print_metadata_sequence(seq);

    status |= fix_vds(fd, dev, &disc, st_size, blocksize, source, seq); 

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
        if(fix_lvid(fd, dev, &disc, st_size, blocksize, &stats, seq) == 0) {
            error_status &= ~(ES_LVID | ES_PD); 
            fix_status |= (ES_LVID | ES_PD);
        }
    } else if(fixlvid == 0 && fixpd == 1) {
        if(fix_pd(fd, dev, &disc, st_size, blocksize, &stats, seq) == 0) {
            error_status &= ~(ES_PD); 
            fix_status |= ES_PD;
        }
    }

#if DEBUG && 0
    note("\n ACT \t EXP\n");
    uint32_t shift = 0;
    uint32_t line = 0;
    uint32_t amount = 50000;
    for(int i=0+shift, k=0+shift; i<stats.partitionSizeBlocks/8 && i < amount+shift; ) {
        note("[%04d] ",line++);
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

    if(status <= 1) {
        msg("Filesystem clean.\n");
    }
    else if(status == 1) {
        msg("Filesystem errors were fixed.\n");
    }
    //---------------- Clean up -----------------

    note("Clean allocations\n");
    free(dev);

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
    free(stats.volumeSetIdent);

    flock(fd, LOCK_UN);
    close(fd);
    fclose(fp);

    msg("All done\n");
    return status;
}
