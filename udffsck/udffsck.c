/*
 * udffsck.c
 *
 * Copyright (c) 2017    Vojtech Vladyka <vojtch.vladyka@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 
 * USA.
 *
 */

#include "config.h"

#include <math.h>
#include <time.h>
#include <limits.h>
#include <sys/mman.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/param.h>

#include "udffsck.h"
#include "utils.h"
#include "libudffs.h"
#include "options.h"

// Local function protypes
uint8_t get_file(int fd, uint8_t **dev, const struct udf_disc *disc, size_t st_size, uint32_t lbnlsn, uint32_t lsn, struct filesystemStats *stats, uint32_t depth, uint32_t uuid, struct fileInfo info, vds_sequence_t *seq );
void increment_used_space(struct filesystemStats *stats, uint64_t increment, uint32_t position);
uint8_t inspect_fid(int fd, uint8_t **dev, const struct udf_disc *disc, size_t st_size, uint32_t lbnlsn, uint32_t lsn, uint8_t *base, uint32_t *pos, struct filesystemStats *stats, uint32_t depth, vds_sequence_t *seq, uint8_t *status);
void print_file_chunks(struct filesystemStats *stats);
int copy_descriptor(int fd, uint8_t **dev, struct udf_disc *disc, size_t st_size, size_t sectorsize, uint32_t sourcePosition, uint32_t destinationPosition, size_t size);
int append_error(vds_sequence_t *seq, uint16_t tagIdent, vds_type_e vds, uint8_t error);

// Local defines
#define MARK_BLOCK 1    ///< Mark switch for markUsedBlock() function
#define UNMARK_BLOCK 0  ///< Unmark switch for markUsedBlock() function

#define MAX_DEPTH 100 ///< Maximal printed filetree depth is MAX_DEPTH/4. Required by function depth2str().

/**
 * \brief File tree prefix creator
 *
 * This fuction takes depth and based on that prints lines and splits
 *
 * \param[in] depth required depth to print
 * \return NULL terminated static char array with printed depth
 */
char * depth2str(uint32_t depth) {
    static char prefix[MAX_DEPTH] = {0};

    if(depth == 0) {
        return prefix;
    }

    if(depth < MAX_DEPTH) {
        int i=0, c=0;
        int width = 4;
        for(i=0, c=0; c<depth-1; c++, i+=width) {
            strcpy(prefix+i, "\u2502 ");
        }
        strcpy(prefix+i, "\u251C\u2500");
    }
    return prefix;
}

/**
 * \brief Checksum calculation function for tags
 *
 * This function is tailored for checksum calculation for UDF tags.
 * It skips 5th byte, because there is result stored. 
 *
 * \param[in] descTag target for checksum calculation
 * \return checksum result
 */
uint8_t calculate_checksum(tag descTag) {
    uint8_t i;
    uint8_t tagChecksum = 0;

    for (i=0; i<16; i++)
        if (i != 4)
            tagChecksum += (uint8_t)(((char *)&(descTag))[i]);

    return tagChecksum;
}

/**
 * \brief Wrapper function for checksum
 *
 * \param[in] descTag target for checksum calculation
 * \return result of checksum comparison, 1 if match, 0 if differs
 *
 * \warning This function have oposite results than crc() and check_position()
 */
int checksum(tag descTag) {
    uint8_t checksum =  calculate_checksum(descTag);
    dbg("Calc checksum: 0x%02x Tag checksum: 0x%02x\n", checksum, descTag.tagChecksum);
    return checksum == descTag.tagChecksum;
}

/**
 * \brief CRC calcultaion wrapper for udf_crc() function from libudffs
 *
 * \param[in] desc descriptor for calculation
 * \param[in] size size for calculation
 * \return CRC result
 */
uint16_t calculate_crc(void * restrict desc, uint16_t size) {
    uint8_t offset = sizeof(tag);
    tag *descTag = desc;
    uint16_t crc = 0;

    if(size >= 16) {
        uint16_t calcCrc = udf_crc((uint8_t *)(desc) + offset, size - offset, crc);
        return calcCrc;
    } else {
        return 0;
    }
}

/**
 * \brief Wrapper function for CRC calculation
 *
 * \param[in] desc descriptor for calculation
 * \param[in] size size for calculation
 * \return result of checksum comparsion, 0 if match, 1 if differs
 */
int crc(void * restrict desc, uint16_t size) {
    uint16_t calcCrc = calculate_crc(desc, size);
    tag *descTag = desc;
    dbg("Calc CRC: 0x%04x, TagCRC: 0x%04x\n", calcCrc, descTag->descCRC);
    return le16_to_cpu(descTag->descCRC) != calcCrc;
}

/**
 * \brief Position check function
 *
 * Checks declared position from tag against inserted position
 *
 * \param[in] descTag tag with declared position
 * \param[in] position actual position to compare
 * \return result of position comparsion, 0 if match, 1 if differs
 */
int check_position(tag descTag, uint32_t position) {
    dbg("tag pos: 0x%x, pos: 0x%x\n", descTag.tagLocation, position);
    return (descTag.tagLocation != position);
}

/**
 * \brief Timestamp printing function
 *
 * This function prints timestamp to static char array in human readable form
 * 
 * Used format is YYYY-MM-DD hh:mm:ss.cshmms+hh:mm\n
 * cs -- centiseconds\n
 * hm -- hundreds of microseconds\n
 * ms -- microseconds\n
 *
 * \param[in] ts UDF timestamp
 * \return pointer to char static char array
 *
 * \warning char array is NOT NULL terminated
 */
char * print_timestamp(timestamp ts) {
    static char str[34] = {0};
    uint8_t type = ts.typeAndTimezone >> 12;
    int16_t offset = (ts.typeAndTimezone & 0x0800) > 0 ? (ts.typeAndTimezone & 0x0FFF) - (0x1000) : (ts.typeAndTimezone & 0x0FFF);
    int8_t hrso = 0;
    int8_t mino = 0;
    dbg("offset: %d\n", offset);
    if(type == 1 && offset > -2047) { // timestamp is in local time. Convert to UCT.
        hrso = offset/60; // offset in hours
        mino = offset%60; // offset in minutes
    }
    dbg("TypeAndTimezone: 0x%04x\n", ts.typeAndTimezone);
    sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d.%02d%02d%02d+%02d:%02d", ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second, ts.centiseconds, ts.hundredsOfMicroseconds, ts.microseconds, hrso, mino);
    return str; 
}

/**
 * \brief UDF timestamp to Unix timestamp conversion function
 *
 * This function fills Unix timestamp structure with its values, add time offset from UDF timestamp to it and create timestamp.
 *
 * \warning Because Unix timestamp have second as minimal unit of time, there is precission loss since UDF operates up to microseconds
 *
 * \param[in] t UDF timestamp
 * \return time_t Unix timestamp structure
 */
time_t timestamp2epoch(timestamp t) {
    struct tm tm = {0};
    tm.tm_year = t.year - 1900;
    tm.tm_mon = t.month - 1; 
    tm.tm_mday = t.day;
    tm.tm_hour = t.hour;
    tm.tm_min = t.minute;
    tm.tm_sec = t.second;
    float rest = (t.centiseconds * 10000 + t.hundredsOfMicroseconds * 100 + t.microseconds)/1000000.0;
    if(rest > 0.5)
        tm.tm_sec++;
    uint8_t type = t.typeAndTimezone >> 12;
    int16_t offset = (t.typeAndTimezone & 0x0800) > 0 ? (t.typeAndTimezone & 0x0FFF) - (0x1000) : (t.typeAndTimezone & 0x0FFF);
    if(type == 1 && offset > -2047) { // timestamp is in local time. Convert to UCT.
        int8_t hrso = offset/60; // offset in hours
        int8_t mino = offset%60; // offset in minutes
        tm.tm_hour -= hrso;
        tm.tm_min -= mino;
    } else if(type == 2) {
        warn("Time interpretation is not specified.\n");
    }
    return mktime(&tm);
}

/**
 * \brief UDF Timestamp comparison wrapper
 *
 * Timestamps are converted to Unix timestamps and compared with difftime()
 *
 * \param[in] a first timestamp
 * \param[in] b second timestamp
 * \return result of difftime(). Basically result a-b.
 */
double compare_timestamps(timestamp a, timestamp b) {
    double dt = difftime(timestamp2epoch(a), timestamp2epoch(b));
    return dt;
}

/**
 * \brief File information printing function
 *
 * This function wraps file charactersitics, file type, permissions, modification time, size and record name
 * and prints it in human readable form.
 *
 * Format is this: HdDPM:darwxd:arwxd:arwx <FILETYPE> <TIMESTAMP> <SIZE> "<NAME>"\n
 * - H -- Hidden
 * - d -- Directory
 * - D -- Deleted
 * - P -- Parent
 * - M -- Metadata
 * - d -- right to delete
 * - r -- right to read
 * - w -- right to write
 * - x -- right to execute
 * - a -- right to change attributes
 * - . -- bit not set
 *
 * \param[in] info file information to print
 * \param[in] depth parameter for prefix, required by depth2str().
 */
void print_file_info(struct fileInfo info, uint32_t depth) {
    msg("%s", depth2str(depth));

    //Print file char
    uint8_t deleted = 0;
    for(int i=0; i<5; i++) {
        switch(info.fileCharacteristics & (1 << i)) {
            case FID_FILE_CHAR_HIDDEN:   msg("H"); break;
            case FID_FILE_CHAR_DIRECTORY:msg("d"); break;
            case FID_FILE_CHAR_DELETED:  msg("D"); deleted = 1; break;
            case FID_FILE_CHAR_PARENT:   msg("P"); break;
            case FID_FILE_CHAR_METADATA: msg("M"); break;
            default:                     msg(".");
        }
    }

    if(deleted == 0) {
        msg(":");

        //Print permissions
        for(int i=14; i>=0; i--) {
            switch(info.permissions & (1 << i)) {
                case FE_PERM_O_EXEC:    msg("x");  break;
                case FE_PERM_O_WRITE:   msg("w");  break;
                case FE_PERM_O_READ:    msg("r");  break;
                case FE_PERM_O_CHATTR:  msg("a");  break;
                case FE_PERM_O_DELETE:  msg("d");  break;
                case FE_PERM_G_EXEC:    msg("x");  break;
                case FE_PERM_G_WRITE:   msg("w");  break;
                case FE_PERM_G_READ:    msg("r");  break;
                case FE_PERM_G_CHATTR:  msg("a");  break;
                case FE_PERM_G_DELETE:  msg("d");  break;
                case FE_PERM_U_EXEC:    msg("x");  break;
                case FE_PERM_U_WRITE:   msg("w");  break;
                case FE_PERM_U_READ:    msg("r");  break;
                case FE_PERM_U_CHATTR:  msg("a");  break;
                case FE_PERM_U_DELETE:  msg("d");  break;

                default:                msg(".");
            }
            if(i == 4 || i == 9 ) {
                msg(":");
            }
        }

        switch(info.fileType) {
            case ICBTAG_FILE_TYPE_DIRECTORY: msg(" DIR    "); break;
            case ICBTAG_FILE_TYPE_REGULAR:   msg(" FILE   "); break;
            case ICBTAG_FILE_TYPE_BLOCK:     msg(" BLOCK  "); break;
            case ICBTAG_FILE_TYPE_CHAR:      msg(" CHAR   "); break;
            case ICBTAG_FILE_TYPE_FIFO:      msg(" FIFO   "); break;
            case ICBTAG_FILE_TYPE_SOCKET:    msg(" SOCKET "); break;
            case ICBTAG_FILE_TYPE_SYMLINK:   msg(" SYMLIN "); break;
            case ICBTAG_FILE_TYPE_STREAMDIR: msg(" STREAM "); break;
            default:                     msg(" UNKNOWN   "); break;
        }

        //Print timestamp
        msg(" %s ", print_timestamp(info.modTime));

        //Print size
        msg(" %8d ", info.size);

    } else {
        msg("          <Unused FID>                                          ");
    }

    //Print filename
    if(info.filename == NULL) {
        msg(" <ROOT> ");
    } else {
        msg(" \"%s\"", info.filename);
    }

    msg("\n");
}

void sync_chunk(uint8_t **dev, uint32_t chunk, size_t st_size) {
    uint32_t chunksize = CHUNK_SIZE;
    uint64_t rest = st_size%chunksize;
    if(dev[chunk] != NULL) {
#ifndef MEMTRACE
        dbg("Going to sync chunk #%d\n", chunk);
#else
        dbg("Going to sync chunk #%d, ptr: %p\n", chunk, dev[chunk]);
#endif
        if(rest > 0 && chunk==st_size/chunksize) {
            dbg("\tRest used\n");
            msync(dev[chunk], chunksize, MS_SYNC);
        } else {
            dbg("\tChunk size used\n");
            msync(dev[chunk], chunksize, MS_SYNC);
        }
        dbg("\tChunk #%d synced\n", chunk);
    } else {
        dbg("\tChunk #%d is unmapped\n");
    }
}

void unmap_chunk(uint8_t **dev, uint32_t chunk, size_t st_size) {
    uint32_t chunksize = CHUNK_SIZE;
    uint64_t rest = st_size%chunksize;
    if(dev[chunk] != NULL) {
        sync_chunk(dev, chunk, st_size);
#ifndef MEMTRACE
        dbg("Going to unmap chunk #%d\n", chunk);
#else
        dbg("Going to unmap chunk #%d, ptr: %p\n", chunk, dev[chunk]);
#endif
        if(rest > 0 && chunk==st_size/chunksize) {
            dbg("\tRest used\n");
            munmap(dev[chunk], rest);
        } else {
            dbg("\tChunk size used\n");
            munmap(dev[chunk], chunksize);
        }
        dev[chunk] = NULL;
        dbg("\tChunk #%d unmapped\n", chunk);
    } else {
        dbg("\tChunk #%d is already unmapped\n", chunk);
#ifdef MEMTRACE
        dbg("[MEMTRACE] Chunk #%d is already unmapped\n", chunk);
#endif
    }
}

void map_chunk(int fd, uint8_t **dev, uint32_t chunk, size_t st_size, char * file, int line) {
    uint32_t chunksize = CHUNK_SIZE;
    uint64_t rest = st_size%chunksize;
    if(dev[chunk] != NULL) {
        dbg("\tChunk #%d is already mapped.\n", chunk);
        return;
    }
#ifdef MEMTRACE
    dbg("[MEMTRACE] map_chunk source call: %s:%d\n", file, line);
#endif
    dbg("\tSize: 0x%x, chunk size 0x%x, rest: 0x%x\n", st_size, chunksize, rest);

    int prot = PROT_READ;
    // If is there some request for corrections, we need read/write access to medium
    if(interactive || autofix) {
        prot |= PROT_WRITE;
        dbg("\tRW\n");
    }

    dbg("\tst_size/chunksize = %d\n", st_size/chunksize);
    if(rest > 0 && chunk==st_size/chunksize) {
        dbg("\tRest used\n");
        dev[chunk] = (uint8_t *)mmap(NULL, rest, prot, MAP_SHARED, fd, (uint64_t)(chunk)*chunksize);
    } else {
        dbg("\tChunk size used\n");
        dev[chunk] = (uint8_t *)mmap(NULL, chunksize, prot, MAP_SHARED, fd, (uint64_t)(chunk)*chunksize);
    }
    if(dev[chunk] == MAP_FAILED) {
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

        fatal("\tError maping: %s.\n", strerror(errno));
        exit(8);
    }
#ifdef MEMTRACE
    dbg("\tChunk #%d allocated, pointer: %p, offset 0x%x\n", chunk, dev[chunk], (uint64_t)(chunk)*chunksize);
#else
    dbg("\tChunk #%d allocated\n", chunk);
#endif
}

void unmap_raw(uint8_t **ptr, uint32_t offset, size_t size) {
    if(*ptr != NULL) {
#ifdef MEMTRACE
        dbg("Going to unmap area, ptr: %p\n", ptr);
#endif
        munmap(*ptr, size);
        ptr = NULL;
        dbg("\tArea unmapped\n");
    } else {
        dbg("\tArea is already unmapped\n");
    }
}

void map_raw(int fd, uint8_t **ptr, uint64_t offset, size_t size, size_t st_size) {
    if(*ptr != NULL) {
        dbg("\tArea is already mapped.\n");
        return;
    }

    dbg("\tSize: 0x%x, Alloc size 0x%x\n", st_size, size);

    int prot = PROT_READ;
    // If is there some request for corrections, we need read/write access to medium
    if(interactive || autofix) {
        prot |= PROT_WRITE;
        dbg("\tRW\n");
    }

    *ptr = (uint8_t *)mmap(NULL, size, prot, MAP_SHARED, fd, offset);
    if(ptr == MAP_FAILED) {
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

        fatal("\tError maping: %s.\n", strerror(errno));
        exit(8); 
    }
#ifdef MEMTRACE
    dbg("\tArea allocated, pointer: %p, offset 0x%x\n", ptr, offset);
#else
    dbg("\tArea allocated\n");
#endif
}

/**
 * \brief UDF VRS detection function
 *
 * This function is trying to find VRS at sector 16.
 * It also do first attempt to guess sectorsize.
 *
 * \param[in] *dev memory mapped device array
 * \param[in,out] *sectorsize found sectorsize candidate
 * \param[in] force_sectorsize if -B param is used, this flag should be set
 *                                and sectorsize should be used automatically.
 * \return 0 -- UDF succesfully detected, sectorsize candidate found
 * \return -1 -- found BOOT2 or CDW02. Unsupported for now
 * \return 1 -- UDF not detected 
 */
int is_udf(int fd, uint8_t **dev, int *sectorsize, size_t st_size, int force_sectorsize) {
    struct volStructDesc vsd;
    struct beginningExtendedAreaDesc bea;
    struct volStructDesc nsr;
    struct terminatingExtendedAreaDesc tea;
    int ssize = 512;
    int notFound = 0;
    int foundBEA = 0;
    uint32_t chunk = 0;
    uint32_t chunksize = CHUNK_SIZE;

    //void map_chunk(int fd, uint8_t **dev, uint32_t chunk, uint64_t st_size) {

    for(int it=0; it<5; it++, ssize *= 2) {
        if(force_sectorsize) {
            ssize = *sectorsize;
            it = INT_MAX - 1; //End after this iteration
            dbg("Forced sectorsize\n");
        }

        dbg("Try sectorsize %d\n", MIN(ssize, BLOCK_SIZE));

        for(int i = 0; i<6; i++) {
            chunk = (16 * BLOCK_SIZE + i*MIN(ssize, BLOCK_SIZE)) / chunksize; 
            map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__);
            dbg("try #%d at address 0x%x, chunk %d, chunk address: 0x%x\n", i, 16*BLOCK_SIZE+i*MIN(ssize, BLOCK_SIZE), chunk, (16*BLOCK_SIZE+i*MIN(ssize, BLOCK_SIZE))%chunksize);
#ifdef MEMTRACE
            dbg("Chunk pointer: %p\n", dev[chunk]);
#endif
            memcpy(&vsd, dev[chunk]+(16*BLOCK_SIZE+i*MIN(ssize, BLOCK_SIZE))%chunksize, sizeof(vsd));
            dbg("vsd: type:%d, id:%s, v:%d\n", vsd.structType, vsd.stdIdent, vsd.structVersion);

            if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BEA01, 5)) {
                //It's Extended area descriptor, so it might be UDF, check next sector
                memcpy(&bea, &vsd, sizeof(bea)); // store it for later
                foundBEA = 1; 
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BOOT2, 5)) {
                err("BOOT2 found, unsuported for now.\n");
                unmap_chunk(dev, chunk, st_size);
                return -1;
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CD001, 5)) { 
                //CD001 means there is ISO9660, we try search for UDF at sector 18
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CDW02, 5)) {
                err("CDW02 found, unsuported for now.\n");
                unmap_chunk(dev, chunk, st_size);
                return -1;
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR02, 5)) {
                memcpy(&nsr, &vsd, sizeof(nsr));
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR03, 5)) {
                memcpy(&nsr, &vsd, sizeof(nsr));
            } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_TEA01, 5)) {
                //We found TEA01, so we can end recognition sequence
                memcpy(&tea, &vsd, sizeof(tea));
                unmap_chunk(dev, chunk, st_size);
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
        unmap_chunk(dev, chunk, st_size);
        return 0;
    }

    err("Giving up VRS, maybe unclosed or bridged disc.\n");
    unmap_chunk(dev, chunk, st_size);
    return 1;
}

/**
 * \brief Counts used bits (blocks) at stats->actParitionBitmap.
 *
 * Support function for getting free space from actual bitmap.
 *
 * \param[in] *stats file system stats structure with filled bitmap
 * \return used blocks amount
 */
uint64_t count_used_bits(struct filesystemStats *stats) {
    if(stats->actPartitionBitmap == NULL)
        return -1;

    uint64_t countedBits = 0;
    uint8_t rest = stats->partitionNumOfBits % 8;
    for(int i = 0; i<stats->partitionNumOfBytes; i++) {
        uint8_t piece = ~stats->actPartitionBitmap[i];
        if(i<stats->partitionNumOfBytes-1) {
            for(int j = 0; j<8; j++) {
                countedBits += (piece>>j)&1;
            }
        } else {
            for(int j = 0; j<rest; j++) {
                countedBits += (piece>>j)&1;
            }
        }
    }
    return countedBits;
}

/**
 * \brief Locate AVDP on device and store it
 *
 * This function searches AVDP at its positions. If it finds it, store it to udf_disc structure to required position by type parameter.
 *
 * It also determine sector size, since AVDP have fixed position. 
 *
 * \param[in] *dev pointer to device array
 * \param[out] *disc AVDP is stored in udf_disc structure
 * \param[in,out] *sectorsize device logical sector size
 * \param[in] devsize size of whole device in B
 * \param[in] type selector of AVDP - first or second
 * \param[in] *stats statistics of file system
 *
 * \return  0 everything is ok
 * \return 255 Only for Third AVDP: it is not AVDP. Abort.
 * \return sum of E_CRC, E_CHECKSUM, E_WRONGDESC, E_POSITION, E_EXTLEN  
 */
int get_avdp(int fd, uint8_t **dev, struct udf_disc *disc, int *sectorsize, size_t devsize, avdp_type_e type, int force_sectorsize, struct filesystemStats *stats) {
    int64_t position = 0;
    tag desc_tag;
    int ssize = 512;
    int it = 0;
    int status = 0;
    uint32_t chunksize = CHUNK_SIZE;
    uint32_t chunk = 0;
    uint32_t offset = 0;

    for(int it = 0; it < 5; it++, ssize *= 2) { 

        //Check if sectorsize is already found
        if(force_sectorsize) {
            ssize = *sectorsize;
            it = INT_MAX-1; //break after this round
        }
        dbg("Trying sectorsize %d\n", ssize);

        //Reset status for new round
        status = 0;

        if(type == 0) {
            position = ssize*256; //First AVDP is on LSN=256
        } else if(type == 1) {
            position = devsize-ssize; //Second AVDP is on last LSN
        } else if(type == 2) {
            position = devsize-ssize-256*ssize; //Third AVDP can be at last LSN-256
        } else {
            position = ssize*512; //Unclosed disc have AVDP at sector 512
            type = 0; //Save it to FIRST_AVDP positon
        }

        dbg("DevSize: %zu\n", devsize);
        dbg("Current position: %lx\n", position);
        chunk = position/chunksize;
        offset = position%chunksize;
        dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
        map_chunk(fd, dev, chunk, devsize, __FILE__, __LINE__); 

        if(disc->udf_anchor[type] == NULL) {
            disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP
        }

#ifdef MEMTRACE
        dbg("AVDP chunk ptr: %p\n", dev[chunk]+offset);
#endif
        desc_tag = *(tag *)(dev[chunk]+offset);
        dbg("Tag allocated\n");

        if(!checksum(desc_tag)) {
            status |= E_CHECKSUM;
            unmap_chunk(dev, chunk, devsize);
            if(type == THIRD_AVDP) {
                return -1;
            } 
            continue;
        }
        if(le16_to_cpu(desc_tag.tagIdent) != TAG_IDENT_AVDP) {
            status |= E_WRONGDESC;
            unmap_chunk(dev, chunk, devsize); 
            if(type == THIRD_AVDP) {
                return -1;
            } 
            continue;
        }
        dbg("Tag Serial Num: %d\n", desc_tag.tagSerialNum);
        if(stats->AVDPSerialNum == 0xFFFF) { // Default state -> save first found 
            stats->AVDPSerialNum = desc_tag.tagSerialNum;
        } else if(stats->AVDPSerialNum != desc_tag.tagSerialNum) { //AVDP serial numbers differs, no recovery support. UDF 2.1.6
            stats->AVDPSerialNum = 0; //No recovery support
        }

        memcpy(disc->udf_anchor[type], dev[chunk]+offset, sizeof(struct anchorVolDescPtr));

        if(crc(disc->udf_anchor[type], sizeof(struct anchorVolDescPtr))) {
            status |= E_CRC;
            unmap_chunk(dev, chunk, devsize); 
            continue;
        }

        if(check_position(desc_tag, position/ssize)) {
            status |= E_POSITION;
            unmap_chunk(dev, chunk, devsize); 
            continue;
        }

        dbg("AVDP[%d]: Main Ext Len: %d, Reserve Ext Len: %d\n", type, disc->udf_anchor[type]->mainVolDescSeqExt.extLength, disc->udf_anchor[type]->reserveVolDescSeqExt.extLength);    
        dbg("AVDP[%d]: Main Ext Pos: 0x%08x, Reserve Ext Pos: 0x%08x\n", type, disc->udf_anchor[type]->mainVolDescSeqExt.extLocation, disc->udf_anchor[type]->reserveVolDescSeqExt.extLocation);    
        if(disc->udf_anchor[type]->mainVolDescSeqExt.extLength < 16*ssize ||  disc->udf_anchor[type]->reserveVolDescSeqExt.extLength < 16*ssize) {
            status |= E_EXTLEN;
        }

        msg("AVDP[%d] successfully loaded.\n", type);
        *sectorsize = ssize;

        if(status & E_CHECKSUM) {
            err("Checksum failure at AVDP[%d]\n", type);
        }
        if(status & E_WRONGDESC) {
            err("AVDP not found at 0x%lx\n", position);
        }
        if(status & E_CRC) {
            err("CRC error at AVDP[%d]\n", type);
        }
        if(status & E_POSITION) {
            err("Position mismatch at AVDP[%d]\n", type);
        }
        if(status & E_EXTLEN) {
            err("Main or Reserve Extent Length at AVDP[%d] is less than 16 sectors\n", type);
        }  
        unmap_chunk(dev, chunk, devsize); 
        return status;
    }
    unmap_chunk(dev, chunk, devsize); 
    return status;
}


/**
 * \brief Loads Volume Descriptor Sequence (VDS) and stores it at struct udf_disc
 *
 * \param[in] *dev pointer to device array
 * \param[out] *disc VDS is stored in udf_disc structure
 * \param[in] sectorsize device logical sector size
 * \param[in] vds MAIN_VDS or RESERVE_VDS selector
 * \param[out] *seq structure capturing actual order of descriptors in VDS for recovery
 * \return 0 everything ok
 *         -3 found unknown tag
 *         -4 descriptor is already set
 */
int get_vds(int fd, uint8_t **dev, struct udf_disc *disc, int sectorsize, size_t st_size, avdp_type_e avdp, vds_type_e vds, vds_sequence_t *seq) {
    uint8_t *position;
    int8_t counter = 0;
    tag descTag;
    uint64_t location = 0;
    uint32_t chunksize = CHUNK_SIZE;
    uint32_t chunk = 0;
    uint32_t offset = 0;

    // Go to first address of VDS
    switch(vds) {
        case MAIN_VDS:
            location = sectorsize*((uint64_t)(disc->udf_anchor[avdp]->mainVolDescSeqExt.extLocation));
            dbg("VDS location: 0x%x\n", disc->udf_anchor[avdp]->mainVolDescSeqExt.extLocation);
            break;
        case RESERVE_VDS:
            location = sectorsize*((uint64_t)(disc->udf_anchor[avdp]->reserveVolDescSeqExt.extLocation));
            dbg("VDS location: 0x%x\n", disc->udf_anchor[avdp]->reserveVolDescSeqExt.extLocation);
            break;
    }
    chunk = location/chunksize;
    offset = (uint32_t)(location % (uint64_t)chunksize);
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 
    position = dev[chunk]+offset;
    dbg("VDS Location: 0x%llx, chunk: %d, offset: 0x%lx\n", location, chunk, offset);

    // Go thru descriptors until TagIdent is 0 or amout is too big to be real
    while(counter < VDS_STRUCT_AMOUNT) {

        // Read tag
        memcpy(&descTag, position, sizeof(descTag));

        dbg("Tag ID: %d\n", descTag.tagIdent);

        if(vds == MAIN_VDS) {
            seq->main[counter].tagIdent = descTag.tagIdent;
            seq->main[counter].tagLocation = (location)/sectorsize;
        } else {
            seq->reserve[counter].tagIdent = descTag.tagIdent;
            seq->reserve[counter].tagLocation = (location)/sectorsize;
        }

        counter++;
        dbg("Tag stored\n");

        // What kind of descriptor is that?
        switch(le16_to_cpu(descTag.tagIdent)) {
            case TAG_IDENT_PVD:
                if(disc->udf_pvd[vds] != 0) {
                    err("Structure PVD is already set. Probably error at tag or media\n");
                    unmap_chunk(dev, chunk, st_size);
                    return -4;
                }
                disc->udf_pvd[vds] = malloc(sizeof(struct primaryVolDesc)); // Prepare memory
                memcpy(disc->udf_pvd[vds], position, sizeof(struct primaryVolDesc)); 
                dbg("VolNum: %d\n", disc->udf_pvd[vds]->volDescSeqNum);
                dbg("pVolNum: %d\n", disc->udf_pvd[vds]->primaryVolDescNum);
                dbg("seqNum: %d\n", disc->udf_pvd[vds]->volSeqNum);
                dbg("predLoc: %d\n", disc->udf_pvd[vds]->predecessorVolDescSeqLocation);
                break;
            case TAG_IDENT_IUVD:
                if(disc->udf_iuvd[vds] != 0) {
                    err("Structure IUVD is already set. Probably error at tag or media\n");
                    unmap_chunk(dev, chunk, st_size);
                    return -4;
                }
                dbg("Store IUVD\n");
                disc->udf_iuvd[vds] = malloc(sizeof(struct impUseVolDesc)); // Prepare memory
#ifdef MEMTRACE
                dbg("Malloc ptr: %p\n", disc->udf_iuvd[vds]);
#endif
                memcpy(disc->udf_iuvd[vds], position, sizeof(struct impUseVolDesc));
                dbg("Stored\n"); 
                break;
            case TAG_IDENT_PD:
                if(disc->udf_pd[vds] != 0) {
                    err("Structure PD is already set. Probably error at tag or media\n");
                    unmap_chunk(dev, chunk, st_size);
                    return -4;
                }
                disc->udf_pd[vds] = malloc(sizeof(struct partitionDesc)); // Prepare memory
                memcpy(disc->udf_pd[vds], position, sizeof(struct partitionDesc)); 
                break;
            case TAG_IDENT_LVD:
                if(disc->udf_lvd[vds] != 0) {
                    err("Structure LVD is already set. Probably error at tag or media\n");
                    unmap_chunk(dev, chunk, st_size);
                    return -4;
                }
                dbg("LVD size: 0x%lx\n", sizeof(struct logicalVolDesc));

                struct logicalVolDesc *lvd;
                lvd = (struct logicalVolDesc *)(position);

                disc->udf_lvd[vds] = malloc(sizeof(struct logicalVolDesc)+lvd->mapTableLength); // Prepare memory
                memcpy(disc->udf_lvd[vds], position, sizeof(struct logicalVolDesc)+lvd->mapTableLength);
                dbg("NumOfPartitionMaps: %d\n", disc->udf_lvd[vds]->numPartitionMaps);
                dbg("MapTableLength: %d\n", disc->udf_lvd[vds]->mapTableLength);
                for(int i=0; i<le32_to_cpu(lvd->mapTableLength); i++) {
                    note("[0x%02x] ", disc->udf_lvd[vds]->partitionMaps[i]);
                }
                note("\n");
                break;
            case TAG_IDENT_USD:
                if(disc->udf_usd[vds] != 0) {
                    err("Structure USD is already set. Probably error at tag or media\n");
                    unmap_chunk(dev, chunk, st_size);
                    return -4;
                }

                struct unallocSpaceDesc *usd;
                usd = (struct unallocSpaceDesc *)(position);
                dbg("VolDescNum: %d\n", usd->volDescSeqNum);
                dbg("NumAllocDesc: %d\n", usd->numAllocDescs);

                disc->udf_usd[vds] = malloc(sizeof(struct unallocSpaceDesc)+(usd->numAllocDescs)*sizeof(extent_ad)); // Prepare memory
                memcpy(disc->udf_usd[vds], position, sizeof(struct unallocSpaceDesc)+(usd->numAllocDescs)*sizeof(extent_ad)); 
                break;
            case TAG_IDENT_TD:
                if(disc->udf_td[vds] != 0) {
                    err("Structure TD is already set. Probably error at tag or media\n");
                    unmap_chunk(dev, chunk, st_size);
                    return -4;
                }
                disc->udf_td[vds] = malloc(sizeof(struct terminatingDesc)); // Prepare memory
                memcpy(disc->udf_td[vds], position, sizeof(struct terminatingDesc)); 
                // Found terminator, ending.
                unmap_chunk(dev, chunk, st_size);
                return 0;
            case 0:
                // Found end of VDS, ending.
                unmap_chunk(dev, chunk, st_size);
                return 0;
            default:
                // Unkown TAG
                fatal("Unknown TAG found at %p. Ending.\n", position);
                unmap_chunk(dev, chunk, st_size);
                return -3;
        }

        dbg("Unmap old chunk...\n");
        unmap_chunk(dev, chunk, st_size);
        dbg("Unmapped\n");
        location = location + sectorsize;
        chunk = location/chunksize;
        offset = location%chunksize;
        dbg("New VDS Location: 0x%llx, chunk: %d, offset: 0x%lx\n", location, chunk, offset);
        map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 
        position = dev[chunk]+offset;
    }
    //unmap_chunk(dev, chunk);
    return 0;
}

/**
 * \brief Selects **MAIN_VDS** or **RESERVE_VDS** for required descriptor based on errors
 *
 * If some function needs some descriptor from VDS, it requires check if descriptor is structurally correct.
 * This is already checked and stored in seq->main[vds].error and seq->reserve[vds].error.
 * This function search thru this sequence based on tagIdent and looks at errors when found.
 *
 * \param[in] *seq descriptor sequence
 * \param[in] tagIdent identifier to find
 * \return MAIN_VDS or RESERVE_VDS if correct descriptor found
 * \return -1 if no correct descriptor found or both are broken.
 */
int get_correct(vds_sequence_t *seq, uint16_t tagIdent) {
    for(int i=0; i<VDS_STRUCT_AMOUNT; i++) {
        if(seq->main[i].tagIdent == tagIdent && (seq->main[i].error & (E_CRC | E_CHECKSUM | E_WRONGDESC)) == 0) {
            return MAIN_VDS; 
        } else if(seq->reserve[i].tagIdent == tagIdent && (seq->reserve[i].error & (E_CRC | E_CHECKSUM | E_WRONGDESC)) == 0) {
            return RESERVE_VDS;
        }
    }
    return -1; 
}

/**
 * \brief Loads Logical Volume Integrity Descriptor (LVID) and stores it at struct udf_disc
 *
 * Loads LVID descriptor to disc stucture. Beside that, it stores selected params in stats structure for 
 * easier access.
 *
 * \param[in] *dev pointer to device array
 * \param[out] *disc LVID is stored in udf_disc structure
 * \param[in] sectorsize device logical sector size
 * \param[out] *stats file system status
 * \param[in] *seq descriptor sequence
 * \return 0 everything ok
 * \return 4 structure is already set or no correct LVID found
 */
int get_lvid(int fd, uint8_t **dev, struct udf_disc *disc, int sectorsize, size_t st_size, struct filesystemStats *stats, vds_sequence_t *seq ) {
    uint32_t chunksize = CHUNK_SIZE;
    uint32_t chunk = 0;
    uint32_t offset = 0;

    if(disc->udf_lvid != 0) {
        err("Structure LVID is already set. Probably error at tag or media\n");
        return 4;
    }
    int vds = -1;
    if((vds=get_correct(seq, TAG_IDENT_LVD)) < 0) {
        err("No correct LVD found. Aborting.\n");
        return 4;
    }

    uint32_t loc = disc->udf_lvd[vds]->integritySeqExt.extLocation;
    uint32_t len = disc->udf_lvd[vds]->integritySeqExt.extLength;
    dbg("LVID: loc: %d, len: %d\n", loc, len);

    chunk = (loc*sectorsize)/chunksize;
    offset = (loc*sectorsize)%chunksize;
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

    struct logicalVolIntegrityDesc *lvid;
    lvid = (struct logicalVolIntegrityDesc *)(dev[chunk]+offset);

    disc->udf_lvid = malloc(len);
    memcpy(disc->udf_lvid, dev[chunk]+offset, len);
    dbg("LVID: lenOfImpUse: %d\n",disc->udf_lvid->lengthOfImpUse);
    dbg("LVID: numOfPartitions: %d\n", disc->udf_lvid->numOfPartitions);

    struct impUseLVID *impUse = (struct impUseLVID *)((uint8_t *)(disc->udf_lvid) + sizeof(struct logicalVolIntegrityDesc) + 8*disc->udf_lvid->numOfPartitions); //this is because of ECMA 167r3, 3/24, fig 22
    uint8_t *impUseArr = (uint8_t *)impUse;
    stats->actUUID = (((struct logicalVolHeaderDesc *)(disc->udf_lvid->logicalVolContentsUse))->uniqueID);

    stats->LVIDtimestamp = lvid->recordingDateAndTime;

    dbg("LVID: number of files: %d\n", impUse->numOfFiles);
    dbg("LVID: number of dirs:  %d\n", impUse->numOfDirs);
    dbg("LVID: UDF rev: min read:  %04x\n", impUse->minUDFReadRev);
    dbg("               min write: %04x\n", impUse->minUDFWriteRev);
    dbg("               max write: %04x\n", impUse->maxUDFWriteRev);
    dbg("Next Unique ID: %d\n", stats->actUUID); 
    dbg("LVID recording timestamp: %s\n", print_timestamp(stats->LVIDtimestamp)); 

    stats->expNumOfFiles = impUse->numOfFiles;
    stats->expNumOfDirs = impUse->numOfDirs;

    stats->minUDFReadRev = impUse->minUDFReadRev;
    stats->minUDFWriteRev = impUse->minUDFWriteRev;
    stats->maxUDFWriteRev = impUse->maxUDFWriteRev;

    dbg("Logical Volume Contents Use\n");
    for(int i=0; i<32; ) {
        for(int j=0; j<8; j++, i++) {
            note("%02x ", disc->udf_lvid->logicalVolContentsUse[i]);
        }
        note("\n");
    }
    dbg("Free Space Table\n");
    for(int i=0; i<disc->udf_lvid->numOfPartitions * 4; i++) {
        note("0x%08x, %d\n", disc->udf_lvid->freeSpaceTable[i], disc->udf_lvid->freeSpaceTable[i]);
    }
    stats->freeSpaceBlocks = disc->udf_lvid->freeSpaceTable[0];
    stats->partitionSizeBlocks = disc->udf_lvid->freeSpaceTable[1];

    dbg("Size Table\n");
    for(int i=disc->udf_lvid->numOfPartitions * 4; i<disc->udf_lvid->numOfPartitions * 4 * 2; i++) {
        note("0x%08x, %d\n", disc->udf_lvid->freeSpaceTable[i],disc->udf_lvid->freeSpaceTable[i]);
    }

    if(disc->udf_lvid->nextIntegrityExt.extLength > 0) {
        dbg("Next integrity extent found.\n");
    } else {
        dbg("No other integrity extents are here.\n");
    }

    unmap_chunk(dev, chunk, st_size); 
    return 0; 
}

/**
 * \brief Checks Logical Block Size stored in LVD with autodetected or declared size.
 *
 * Compare LVD->LogicalBlockSize with detected or declared block size. If they matches, fsck can continue.
 * Otherwise it stops with fatal error, because medium is badly created and therefore unfixable.
 *
 * Return can be sum of its parts.
 *
 * \param[in] fd device file descriptor
 * \param[in] *dev pointer to device array
 * \param[out] *disc udf_disc structure
 * \param[in] blocksize device logical sector size
 * \param[in] force_blocksize 1 represent user defined sector size
 * \param[in] *seq descriptor sequence
 * \return 0 blocksize matches
 * \return 4 blocksize differs from detected one
 * \return 16 blocksize differs from declared one
 */
int check_blocksize(int fd, uint8_t **dev, struct udf_disc *disc, int blocksize, int force_sectorsize, vds_sequence_t *seq) {

    int vds = -1;
    if((vds=get_correct(seq, TAG_IDENT_LVD)) < 0) {
        err("No correct LVD found. Aborting.\n");
        return 4;
    }

    uint32_t lvd_blocksize = disc->udf_lvd[vds]->logicalBlockSize;
    

    if(lvd_blocksize != blocksize) {
        if(force_sectorsize) {
            err("User defined block size is not corresponding to detected. Aborting.\n");
            return 16 | 4;
        }

        err("Detected block size is not corresponding to stored in medium. Probably badly created UDF. Aborting.\n");
        return 4;
    }
   
    dbg("Blocksize matches.\n"); 
    return 0; 
}
/**
 * \brief Select various volume identifiers and store them at stats structure
 * 
 * At this moment it selects PVD->volSetIdent and FSD->logicalVolIdent
 *
 * \param[in] *disc disc structure
 * \param[out] *stats file system status structure
 * \param[in] *seq VDS sequence
 *
 * \return 0 -- everything OK
 * \return 4 -- no correct PVD found.
 */
int get_volume_identifier(struct udf_disc *disc, struct filesystemStats *stats, vds_sequence_t *seq ) {
    int vds = -1;
    if((vds=get_correct(seq, TAG_IDENT_PVD)) < 0) {
        err("No correct PVD found. Aborting.\n");
        return 4;
    }
    char *namebuf = calloc(1,128*2);
    memset(namebuf, 0, 128*2);
    int size = decode_string(disc, disc->udf_pvd[vds]->volSetIdent, namebuf, 128);

    for(int i=0; i<16; i++) {
        if((namebuf[i] >= '0' && namebuf[i]<='9') || (namebuf[i] >= 'a' && namebuf[i] <= 'z')) {
            continue; 
        } else {
            warn("Volume Set Identifier Unique Identifier is not compliant.\n");
            //append_error(seq, TAG_IDENT_PVD, MAIN_VDS, E_UUID);
            //append_error(seq, TAG_IDENT_PVD, RESERVE_VDS, E_UUID);
            //TODO create fix somewhere. Use this gen_uuid_from_vol_set_ident() for generating new UUID.
            break;
        }
    }

    stats->volumeSetIdent = namebuf;
    stats->partitionIdent = disc->udf_fsd->logicalVolIdent;
    return 0;
}

/**
 * \brief Marks used blocks in actual bitmap
 *
 * This function mark or unmark specified areas of block bitmap at stats->actPartitionBitmap
 * If medium is consistent, this bitmap should be same as declared (stats->expPartitionBitmap)
 *
 * \param[in,out] *stats file system status structure
 * \param[in] lbn starting logical block of area
 * \param[in] size length of marked area
 * \param[in] MARK_BLOCK or UNMARK_BLOCK switch
 *
 * \return 0 everything is OK or size is 0 (nothing to mark)
 * \return -1 marking failed (actParititonBitmap is uninitialized)
 */ 
uint8_t markUsedBlock(struct filesystemStats *stats, uint32_t lbn, uint32_t size, uint8_t mark) {
    if(lbn+size < stats->partitionNumOfBits) {
        uint32_t byte = 0;
        uint8_t bit = 0;

        dbg("Marked LBN %d with size %d\n", lbn, size);
        if(size == 0) {
            dbg("Size is 0, return.\n");
            return 0;
        }
        int i = 0;
        do {
            byte = lbn/8;
            bit = lbn%8;
            if(mark) { // write 0
                if(stats->actPartitionBitmap[byte] & (1<<bit)) {
                    stats->actPartitionBitmap[byte] &= ~(1<<bit);
                } else {
                    err("[%d:%d]Error marking block as used. It is already marked.\n", byte, bit);
                }
            } else { // write 1
                if(stats->actPartitionBitmap[byte] & (1<<bit)) {
                    err("[%d:%d]Error marking block as unused. It is already unmarked.\n", byte, bit);
                } else {
                    stats->actPartitionBitmap[byte] |= 1<<bit;
                }
            }
            lbn++;
            i++;
        } while(i < size);
        dbg("Last LBN: %d, Byte: %d, Bit: %d\n", lbn, byte, bit);
        dbg("Real size: %d\n", i);

#if 0   // For debug purposes only
        note("\n ACT \t EXP\n");
        uint32_t shift = 0;
        for(int i=0+shift, k=0+shift; i<stats->partitionSizeBlocks/8 && i < 100+shift; ) {
            for(int j=0; j<16; j++, i++) {
                note("%02x ", stats->actPartitionBitmap[i]);
            }
            note("| "); 
            for(int j=0; j<16; j++, k++) {
                note("%02x ", stats->expPartitionBitmap[k]);
            }
            note("\n");
        }
        note("\n");
        shift = 4400;
        for(int i=0+shift, k=0+shift; i<stats->partitionSizeBlocks/8 && i < 100+shift; ) {
            for(int j=0; j<16; j++, i++) {
                note("%02x ", stats->actPartitionBitmap[i]);
            }
            note("| "); 
            for(int j=0; j<16; j++, k++) {
                note("%02x ", stats->expPartitionBitmap[k]);
            }
            note("\n");
        }
        note("\n");
#endif
    } else {
        err("MARKING USED BLOCK TO BITMAP FAILED\n");
        return -1;
    }
    return 0;
}

/**
 * \brief Loads File Set Descriptor and stores it at struct udf_disc
 *
 * \param[in] *dev pointer to device array
 * \param[out] *disc FSD is stored in udf_disc structure
 * \param[in] sectorsize device logical sector size
 * \param[out] lbnlsn LBN starting offset
 * \param[in] *stats file system status
 * \param[in] *seq VDS sequence
 *
 * \return 0 everything ok
 * \return 4 no correct PD or LVD found
 */
uint8_t get_fsd(int fd, uint8_t **dev, struct udf_disc *disc, int sectorsize, size_t st_size, uint32_t *lbnlsn, struct filesystemStats * stats, vds_sequence_t *seq) {
    long_ad *lap;
    tag descTag;
    int vds = -1;
    uint32_t offset = 0, chunk = 0;
    uint32_t chunksize = CHUNK_SIZE;
    uint64_t position = 0;

    if((vds=get_correct(seq, TAG_IDENT_PD)) < 0) {
        err("No correct PD found. Aborting.\n");
        return 4;
    }
    dbg("PD partNum: %d\n", disc->udf_pd[vds]->partitionNumber);
    uint32_t lsnBase = 0;
    lsnBase = disc->udf_pd[vds]->partitionStartingLocation;
    dbg("Partition Length: %d\n", disc->udf_pd[vds]->partitionLength);

    dbg("LSN base: %d\n", lsnBase);

    vds = -1;
    if((vds=get_correct(seq, TAG_IDENT_LVD)) < 0) {
        err("No correct LVD found. Aborting.\n");
        return 4;
    }
    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[vds]->logicalBlockSize); 

    lap = (long_ad *)disc->udf_lvd[vds]->logicalVolContentsUse; //FIXME BIG_ENDIAN use lela_to_cpu, but not on ptr to disc. Must store it on different place.
    lb_addr filesetblock = lelb_to_cpu(lap->extLocation);
    uint32_t filesetlen = lap->extLength;

    dbg("FSD at (%d, p%d)\n", 
            lap->extLocation.logicalBlockNum,
            lap->extLocation.partitionReferenceNum);

    dbg("LAP: length: %x, LBN: %x, PRN: %x\n", filesetlen, filesetblock.logicalBlockNum, filesetblock.partitionReferenceNum);
    dbg("LAP: LSN: %d\n", lsnBase/*+filesetblock.logicalBlockNum*/);

    position = (lsnBase+filesetblock.logicalBlockNum)*lbSize;
    chunk = position/chunksize;
    offset = position%chunksize;
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__);

    disc->udf_fsd = malloc(sizeof(struct fileSetDesc));
    memcpy(disc->udf_fsd, dev[chunk]+offset, sizeof(struct fileSetDesc));

    if(le16_to_cpu(disc->udf_fsd->descTag.tagIdent) != TAG_IDENT_FSD) {
        err("Error identifiing FSD. Tag ID: 0x%x\n", disc->udf_fsd->descTag.tagIdent);
        free(disc->udf_fsd);
        unmap_chunk(dev, chunk, st_size);
        return 8;
    }
    dbg("LogicVolIdent: %s\nFileSetIdent: %s\n", (disc->udf_fsd->logicalVolIdent), (disc->udf_fsd->fileSetIdent));

    increment_used_space(stats, filesetlen, lap->extLocation.logicalBlockNum);

    *lbnlsn = lsnBase;

    unmap_chunk(dev, chunk, st_size);
    return 0;
}

/**
 * \brief Inspect AED and return array of its allocation descriptors
 *
 * This function returns pointer to array of allocation descriptors. This pointer points to memory mapped device!
 *
 * \param[in] *dev memory mapped device
 * \param[in] lsnBase LBN offset to LSN
 * \param[in] aedlbn LBN of AED
 * \param[out] *lengthADArray size of allocation descriptor array ADArray
 * \param[out] **ADAarray allocation descriptors array itself
 * \param[in] *stats file system status
 * \param[out] status error status
 *
 * \return 0 -- AED found and ADArray is set
 * \return 4 -- AED not found
 * \return 4 -- checksum failed
 * \return 4 -- CRC failed
 */
uint8_t inspect_aed(int fd, uint8_t **dev, size_t st_size, uint32_t lsnBase, uint32_t aedlbn, uint32_t *lengthADArray, uint8_t **ADArray, struct filesystemStats *stats, uint8_t *status) {
    uint16_t lbSize = stats->blocksize; 
    uint32_t lad = 0;
    uint32_t nAD = 0;
    short_ad *sad = NULL;
    uint32_t offset = 0, chunk = 0, chunksize = CHUNK_SIZE;

    chunk = ((lsnBase + aedlbn)*lbSize)/chunksize;
    offset = ((lsnBase + aedlbn)*lbSize)%chunksize;
    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

    struct allocExtDesc *aed = (struct allocExtDesc *)(dev[chunk]+offset);
    if(aed->descTag.tagIdent == TAG_IDENT_AED) {
        //checksum
        if(!checksum(aed->descTag)) {
            err("AED checksum failed\n");
            *status |= 4; 
            return 4;
        }
        //CRC
#if 0
        dbg("AED: descCRCLength: %d, LAD: %d\n", aed->descTag.descCRCLength, aed->lengthAllocDescs);
        dbg("AED: CRC: 0x%04x\n", aed->descTag.descCRC);
        for(int i=16; i<aed->descTag.descCRCLength+16; i++) {
            dbg("[%d]: CRC: 0x%04x\n", i, udf_crc((uint8_t *)(aed) + sizeof(tag), i - sizeof(tag), 0));
        }
        if(crc(aed, aed->descTag.descCRCLength-8)) {
            err("AED CRC failed\n");
            *status |= 4;
            return 4;
        }
#endif
        // position
        if(check_position(aed->descTag, aedlbn)) {
            err("AED position differs\n");
            *status |= 4;
        }

        lad = aed->lengthAllocDescs;
        *ADArray = (uint8_t *)(aed)+sizeof(struct allocExtDesc);
        *lengthADArray = lad;
#if 0  //For debug purposes only
        uint32_t line = 0;
        dbg("AED Array\n");
        for(int i=0; i<*lengthADArray; ) {
            note("[%04d] ",line++);
            for(int j=0; j<8; j++, i++) {
                note("%02x ", (*ADArray)[i]);
            }
            note("\n");
        }
#endif
#ifdef MEMTRACE
        dbg("ADArray ptr: %p\n", *ADArray);
#endif
        dbg("lengthADArray: %d\n", *lengthADArray);
        increment_used_space(stats, lad%lbSize == 0 ? lad/lbSize : lad/lbSize + 1, aedlbn);
        return 0;
    } else {
        err("There should be AED, but is not\n");
    }
    return 4;
}

/**
 * \brief FID allocation descriptor position translation function
 *
 * FID's allocation descriptors are stored at Allocation Descriptors area of FE. Problem is, this area is not
 * necessarily in one piece and can be splitted, even in middle of descriptor. This function creates virtual
 * linear area for futher processing.
 *
 * This function internally calls inspect_fid().
 *
 * \param[in] *dev memory mapped device
 * \param[in] *disc udf_disc structure
 * \param[in] lbnlsn LBN offset against LSN
 * \param[in] lsn actual LSN
 * \param[in] *allocDescs pointer to allocation descriptors area
 * \param[in] lengthAllocDescs length of allocation descriptors area
 * \param[in] icb_ad type od AD
 * \param[in] *stats file system status
 * \param[in] depth depth of FE for printing
 * \param[in] *seq VDS sequence
 * \param[out] *status run status
 *
 * \return 0 -- everything OK
 * \return 1 -- Unsupported AD
 * \return 2 -- FID array allocation failed
 * \return 255 -- inspect_aed() failed
 */
uint8_t translate_fid(int fd, uint8_t **dev, const struct udf_disc *disc, size_t st_size, uint32_t lbnlsn, uint32_t lsn, uint8_t *allocDescs, uint32_t lengthAllocDescs, uint16_t icb_ad, struct filesystemStats *stats, uint32_t depth, vds_sequence_t *seq, uint8_t *status) {

    uint32_t descSize = 0;
    uint8_t *fidArray = NULL;
    uint32_t nAD = 0;
    uint32_t overallLength = 0;
    uint32_t overallBodyLength = 0;
    short_ad *sad = NULL;
    long_ad *lad = NULL;
    ext_ad *ead = NULL;
    uint16_t lbSize = stats->blocksize;
    uint32_t lsnBase = lbnlsn;
    uint32_t offset = 0, chunk = 0, chunksize = CHUNK_SIZE;

    switch(icb_ad) {
        case ICBTAG_FLAG_AD_SHORT:
            dbg("Short AD\n");
            descSize = sizeof(short_ad);
            break;
        case ICBTAG_FLAG_AD_LONG:
            dbg("Long AD\n");
            descSize = sizeof(long_ad);
            break;
        case ICBTAG_FLAG_AD_EXTENDED:
            dbg("Extended AD\n");
            descSize = sizeof(ext_ad);
            break;
defualt:
            err("[translate_fid] Unsupported icb_ad: 0x%04x\n", icb_ad);
            return 1;
    }
    dbg("LengthOfAllocDescs: %d\n", lengthAllocDescs);

    nAD = lengthAllocDescs/descSize;

#if 0 // For debug purposes only
    uint32_t line = 0;
    dbg("FID Alloc Array\n");
    for(int i=0; i<lengthAllocDescs; ) {
        note("[%04d] ",line++);
        for(int j=0; j<8; j++, i++) {
            note("%02x ", allocDescs[i]);
        }
        note("\n");
    }
#endif

    uint32_t lengthADArray = 0;
    uint8_t *ADArray = NULL;

    for(int i = 0; i < nAD; i++) {
        uint32_t aedlbn = 0;
        sad = (short_ad *)(allocDescs + i*descSize); //we can do that, beause all ADs have size as first.
        overallLength += sad->extLength & 0x3FFFFFFF; //lower 30 bits are unsiged length
        overallBodyLength += lbSize;
        dbg("ExtLength: %d, type: %d\n", sad->extLength & 0x3FFFFFFF, sad->extLength>>30);
        if(sad->extLength>>30 == 3) { //Extent is AED
            switch(icb_ad) {
                case ICBTAG_FLAG_AD_SHORT:
                    //we already have sad
                    aedlbn = sad->extPosition;
                    break;
                case ICBTAG_FLAG_AD_LONG:
                    lad = (long_ad *)(allocDescs + i*descSize);
                    aedlbn = lad->extLocation.logicalBlockNum;
                    break;
                case ICBTAG_FLAG_AD_EXTENDED:
                    ead = (ext_ad *)(allocDescs + i*descSize);
                    aedlbn = ead->extLocation.logicalBlockNum;
                    break;
            }
            if(inspect_aed(fd, dev, st_size, lsnBase, aedlbn, &lengthADArray, &ADArray, stats, status)) {
                err("AED inspection failed.\n");
                return -1;
            }
#if 1
            uint32_t line = 0;
            dbg("FID Alloc Array after AED\n");
#ifdef MEMTRACE
            dbg("ADArray ptr: %p\n", ADArray);
#endif
            dbg("lengthADArray: %d\n", lengthADArray);
#endif
#if 0       //For debug purposes only
            for(int i=0; i<lengthADArray; ) {
                note("[%04d] ",line++);
                for(int j=0; j<descSize; j++, i++) {
                    note("%02x ", ADArray[i]);
                }
                note("\n");
            }
#endif
            nAD += lengthADArray/descSize;
            overallBodyLength += lengthADArray;
            overallLength += lengthADArray;         
        }
    }

    dbg("Overall length: %d\n", overallLength);
    fidArray = calloc(1, overallBodyLength);
    if(fidArray == NULL) {
        err("FID array allocatiob failed.\n");
        return 2;
    }

    uint32_t prevExtLength = 0;
    uint8_t aed = 0;
    for(int i = 0; i < nAD; i++) {
        switch(icb_ad) {
            case ICBTAG_FLAG_AD_SHORT:
                if(aed) {
                    sad = (short_ad *)(ADArray + i*descSize - lengthAllocDescs);
                } else {
                    sad = (short_ad *)(allocDescs + i*descSize);
                }
                if(sad->extLength >> 30 == 3) { //Extent is AED
                    aed = 1;   
                    continue;     
                }
                chunk = ((lsnBase + sad->extPosition)*lbSize)/chunksize;
                offset = ((lsnBase + sad->extPosition)*lbSize)%chunksize;
                dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                memcpy(fidArray+prevExtLength, (uint8_t *)(dev[chunk]+offset), sad->extLength);
                increment_used_space(stats, 1, sad->extPosition);
                prevExtLength += sad->extLength;
                break;
            case ICBTAG_FLAG_AD_LONG:
                if(aed) {
                    lad = (long_ad *)(ADArray + i*descSize - lengthAllocDescs);
                } else {
                    lad = (long_ad *)(allocDescs + i*descSize);
                }
                if(lad->extLength >> 30 == 3) { //Extent is AED
                    aed = 1;   
                    continue;     
                }
                chunk = ((lsnBase + lad->extLocation.logicalBlockNum)*lbSize)/chunksize;
                offset = ((lsnBase + lad->extLocation.logicalBlockNum)*lbSize)%chunksize;
                dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                memcpy(fidArray+prevExtLength, (uint8_t *)(dev[chunk]+offset), lad->extLength);
                increment_used_space(stats, 1, lad->extLocation.logicalBlockNum);
                prevExtLength += lad->extLength;
                break;
            case ICBTAG_FLAG_AD_EXTENDED:
                if(aed) {
                    ead = (ext_ad *)(ADArray + i*descSize - lengthAllocDescs);
                } else {
                    ead = (ext_ad *)(allocDescs + i*descSize);
                }
                if(ead->extLength >> 30 == 3) { //Extent is AED
                    aed = 1;   
                    continue;     
                }
                chunk = ((lsnBase + ead->extLocation.logicalBlockNum)*lbSize)/chunksize;
                offset = ((lsnBase + ead->extLocation.logicalBlockNum)*lbSize)%chunksize;
                dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                memcpy(fidArray+prevExtLength, (uint8_t *)(dev[chunk]+offset), ead->extLength);
                increment_used_space(stats, 1, ead->extLocation.logicalBlockNum);
                prevExtLength += ead->extLength;
                break;
        }
    }

    uint8_t tempStatus = 0;
    int counter = 0;
    for(uint32_t pos=0; pos < overallLength; ) {
        dbg("FID #%d\n", counter++);
        if(inspect_fid(fd, dev, disc, st_size, lbnlsn, lsn, fidArray, &pos, stats, depth+1, seq, &tempStatus) != 0) {
            dbg("1 FID inspection over.\n");
            break;
        }
    } 
    dbg("2 FID inspection over.\n");

    aed = 0;
    if(tempStatus & 0x01) { //Something was fixed - we need to copy back array
        prevExtLength = 0;
        for(int i = 0; i < nAD; i++) {
            switch(icb_ad) {
                case ICBTAG_FLAG_AD_SHORT:
                    if(aed) {
                        sad = (short_ad *)(ADArray + i*descSize - lengthAllocDescs);
                    } else {
                        sad = (short_ad *)(allocDescs + i*descSize);
                    }
                    if(sad->extLength >> 30 == 3) { //Extent is AED
                        aed = 1;   
                        continue;     
                    }
                    chunk = ((lsnBase + sad->extPosition)*lbSize)/chunksize;
                    offset = ((lsnBase + sad->extPosition)*lbSize)%chunksize;
                    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                    memcpy((uint8_t *)(dev[chunk]+offset), fidArray+prevExtLength, sad->extLength);
                    prevExtLength += sad->extLength;
                    break;
                case ICBTAG_FLAG_AD_LONG:
                    if(aed) {
                        lad = (long_ad *)(ADArray + i*descSize - lengthAllocDescs);
                    } else {
                        lad = (long_ad *)(allocDescs + i*descSize);
                    }
                    if(lad->extLength >> 30 == 3) { //Extent is AED
                        aed = 1;   
                        continue;     
                    }
                    chunk = ((lsnBase + lad->extLocation.logicalBlockNum)*lbSize)/chunksize;
                    offset = ((lsnBase + lad->extLocation.logicalBlockNum)*lbSize)%chunksize;
                    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                    memcpy((uint8_t *)(dev[chunk]+offset), fidArray+prevExtLength, lad->extLength);
                    prevExtLength += lad->extLength;
                    break;
                case ICBTAG_FLAG_AD_EXTENDED:
                    if(aed) {
                        ead = (ext_ad *)(ADArray + i*descSize - lengthAllocDescs);
                    } else {
                        ead = (ext_ad *)(allocDescs + i*descSize);
                    }
                    if(ead->extLength >> 30 == 3) { //Extent is AED
                        aed = 1;   
                        continue;     
                    }
                    chunk = ((lsnBase + ead->extLocation.logicalBlockNum)*lbSize)/chunksize;
                    offset = ((lsnBase + ead->extLocation.logicalBlockNum)*lbSize)%chunksize;
                    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                    memcpy((uint8_t *)(dev[chunk]+offset), fidArray+prevExtLength, ead->extLength);
                    prevExtLength += ead->extLength;
                    break;
            }
        }     
    }

    dbg("3 FID inspection copyback done.\n");
    //free array
    free(fidArray);
    (*status) |= tempStatus;
    return 0;
}

/**
 * \brief FID parsing function
 *
 * This function pareses via FIDs. It continues to its FE using get_file() function. 
 * Checks and fixes *Unique ID*, *Serial Numbers* or unfinished writings.
 *
 * This fucntion is complement to get_file() and translate_fid().
 *
 * \param[in,out] *dev memory mapped device
 * \param[in] *disc udf_disc structure
 * \param[in] lbnlsn LBN offset against LSN
 * \param[in] lsn actual LSN
 * \param[in] *base base pointer for for FID area
 * \param[in,out] *pos actial position in FID area
 * \param[in] *stats file system status
 * \param[in] depth depth of FE for printing
 * \param[in] *seq VDS sequence
 * \param[out] *status run status
 *
 * \return 0 -- everything OK
 * \return 1 -- Unknown descriptor found
 * \return 252 -- FID checksum failed
 * \return 251 -- FID CRC failed
 */
uint8_t inspect_fid(int fd, uint8_t **dev, const struct udf_disc *disc, size_t st_size, uint32_t lbnlsn, uint32_t lsn, uint8_t *base, uint32_t *pos, struct filesystemStats *stats, uint32_t depth, vds_sequence_t *seq, uint8_t *status) {
    uint32_t flen, padding;
    uint32_t lsnBase = lbnlsn; 
    struct fileIdentDesc *fid = (struct fileIdentDesc *)(base + *pos);
    struct fileInfo info = {0};
    uint32_t offset = 0, chunk = 0;
    uint64_t position = 0;
    uint32_t chunksize = CHUNK_SIZE;

    dbg("FID pos: 0x%x\n", *pos);
    if (!checksum(fid->descTag)) {
        err("[inspect fid] FID checksum failed.\n");
        return -4;
        warn("DISABLED ERROR RETURN\n");
    }
    if (le16_to_cpu(fid->descTag.tagIdent) == TAG_IDENT_FID) {
        dwarn("FID found (%d)\n",*pos);
        flen = 38 + le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent;
        padding = 4 * ((le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38 + 3)/4) - (le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38);

        dbg("lengthOfImpUse: %d\n", fid->lengthOfImpUse);
        dbg("flen+padding: %d\n", flen+padding);
        if(crc(fid, flen + padding)) {
            err("FID CRC failed.\n");
            return -5;
            warn("DISABLED ERROR RETURN\n");
        }
        dbg("FID: ImpUseLen: %d\n", fid->lengthOfImpUse);
        dbg("FID: FilenameLen: %d\n", fid->lengthFileIdent);
        if(fid->lengthFileIdent == 0) {
            dbg("ROOT directory\n");
        } else {
            char *namebuf = calloc(1,256*2);
            memset(namebuf, 0, 256*2);
            int size = decode_utf8(fid->fileIdent, namebuf, fid->lengthFileIdent);
            if(size == (size_t) - 1) { //Decoding failed
                warn("Filename decoding failed."); //TODO add tests
            } else {
                dbg("Size: %d\n", size);
                dbg("%sFilename: %s\n", depth2str(depth), namebuf/*fid->fileIdent*/);
                info.filename = namebuf/*(char *)fid->fileIdent+1*/;
            }
        }

        dbg("Tag Serial Num: %d\n", fid->descTag.tagSerialNum);
        if(stats->AVDPSerialNum != fid->descTag.tagSerialNum) {
            err("(%s) Tag Serial Number differs.\n", info.filename);
            uint8_t fixsernum = autofix;
            if(interactive) {
                if(prompt("Fix it? [Y/n] ")) {
                    fixsernum = 1;
                }
            }
            if(fixsernum) {
                fid->descTag.tagSerialNum = stats->AVDPSerialNum;
                fid->descTag.descCRC = calculate_crc(fid, flen+padding);             
                fid->descTag.tagChecksum = calculate_checksum(fid->descTag);

                position = (lsn) * stats->blocksize;
                chunk = position/chunksize;
                offset = position%chunksize;
                dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                struct fileEntry *fe = (struct fileEntry *)(dev[chunk]+offset);
                struct extendedFileEntry *efe = (struct extendedFileEntry *)(dev[chunk]+offset);
                if(efe->descTag.tagIdent == TAG_IDENT_EFE) {
                    efe->descTag.descCRC = calculate_crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs));
                    efe->descTag.tagChecksum = calculate_checksum(efe->descTag);
                    dbg("[CHECKSUM] %"PRIx16"\n", efe->descTag.tagChecksum);
                } else if(efe->descTag.tagIdent == TAG_IDENT_FE) {
                    fe->descTag.descCRC = calculate_crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs));
                    fe->descTag.tagChecksum = calculate_checksum(fe->descTag);
                    dbg("[CHECKSUM] %"PRIx16"\n", fe->descTag.tagChecksum);
                } else {
                    err("(%s) FID parent FE not found.\n", info.filename);
                }
                imp("(%s) Tag Serial Number was fixed.\n", info.filename);
                sync_chunk(dev, chunk, st_size);
                *status |= 1;
            } else {
                *status |= 4;
            }
        }

        dbg("FileVersionNum: %d\n", fid->fileVersionNum);

        info.fileCharacteristics = fid->fileCharacteristics;
        if((fid->fileCharacteristics & FID_FILE_CHAR_DELETED) == 0) { //NOT deleted, continue
            dbg("ICB: LSN: %d, length: %d\n", fid->icb.extLocation.logicalBlockNum + lsnBase, fid->icb.extLength);
            dbg("ROOT ICB: LSN: %d\n", disc->udf_fsd->rootDirectoryICB.extLocation.logicalBlockNum + lsnBase);

            if(*pos == 0) {
                dbg("Parent. Not Following this one\n");
            }else if(fid->icb.extLocation.logicalBlockNum + lsnBase == lsn) {
                dbg("Self. Not following this one\n");
            } else if(fid->icb.extLocation.logicalBlockNum + lsnBase == disc->udf_fsd->rootDirectoryICB.extLocation.logicalBlockNum + lsnBase) {
                dbg("ROOT. Not following this one.\n");
            } else {
                uint32_t uuid = 0;
                memcpy(&uuid, (fid->icb).impUse+2, sizeof(uint32_t));
                dbg("UUID: %d\n", uuid);
                if(stats->maxUUID < uuid) {
                    stats->maxUUID = uuid;
                    dwarn("New MAX UUID\n");
                }
                int fixuuid = 0;
                if(uuid == 0) {
                    err("(%s) FID Unique ID is 0. There should be %d.\n", info.filename, stats->actUUID);
                    if(interactive) {
                        if(prompt("Fix it? [Y/n] ")) {
                            fixuuid = 1;
                        } else {
                            *status |= 4;
                        }
                    }       
                    if(autofix) {
                        fixuuid = 1;
                    } else {
                        *status |= 4;
                    }
                    if(fixuuid) {
                        uuid = stats->actUUID;
                        stats->maxUUID = uuid;
                        stats->actUUID++;
                        seq->lvid.error |= E_UUID;
                        fid->icb.impUse[2] = uuid;
                        fid->descTag.descCRC = calculate_crc(fid, flen+padding);             
                        fid->descTag.tagChecksum = calculate_checksum(fid->descTag);
                        dbg("Location: %d\n", fid->descTag.tagLocation);

                        position = (lsn) * stats->blocksize;
                        chunk = position/chunksize;
                        offset = position%chunksize;
                        dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                        map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                        struct fileEntry *fe = (struct fileEntry *)(dev[chunk]+offset);
                        struct extendedFileEntry *efe = (struct extendedFileEntry *)(dev[chunk]+offset);
                        if(efe->descTag.tagIdent == TAG_IDENT_EFE) {
                            efe->descTag.descCRC = calculate_crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs));
                            efe->descTag.tagChecksum = calculate_checksum(efe->descTag);
                        } else if(efe->descTag.tagIdent == TAG_IDENT_FE) {
                            fe->descTag.descCRC = calculate_crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs));
                            fe->descTag.tagChecksum = calculate_checksum(fe->descTag);
                        } else {

                        }
                        imp("(%s) UUID was fixed.\n", info.filename);
                        *status |= 1;
                    }
                }
                dbg("ICB to follow.\n");
                int tmp_status = get_file(fd, dev, disc, st_size, lbnlsn, (fid->icb).extLocation.logicalBlockNum + lsnBase, stats, depth, uuid, info, seq);
                if(tmp_status == 32) { //32 means delete this FID
                    fid->fileCharacteristics |= FID_FILE_CHAR_DELETED; //Set deleted flag
                    memset(&(fid->icb), 0, sizeof(long_ad)); //clear ICB according ECMA-167r3, 4/14.4.5
                    fid->descTag.descCRC = calculate_crc(fid, flen+padding);             
                    fid->descTag.tagChecksum = calculate_checksum(fid->descTag);
                    dbg("Location: %d\n", fid->descTag.tagLocation);

                    position = (fid->descTag.tagLocation + lbnlsn) * stats->blocksize;
                    chunk = position/chunksize;
                    offset = position%chunksize;
                    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
                    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

                    struct fileEntry *fe = (struct fileEntry *)(dev[chunk] + offset);
                    struct extendedFileEntry *efe = (struct extendedFileEntry *)(dev[chunk]+offset);
                    if(efe->descTag.tagIdent == TAG_IDENT_EFE) {
                        efe->descTag.descCRC = calculate_crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs));
                        efe->descTag.tagChecksum = calculate_checksum(efe->descTag);
                    } else if(efe->descTag.tagIdent == TAG_IDENT_EFE) {
                        fe->descTag.descCRC = calculate_crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs));
                        fe->descTag.tagChecksum = calculate_checksum(fe->descTag);
                    } else {
                        err("(%s) FID parent FE not found.\n", info.filename);
                    }
                    imp("(%s) Unifinished file was removed.\n", info.filename);

                    tmp_status = 1;    
                }
                *status |= tmp_status;
                dbg("Return from ICB\n"); 
            }
        } else {
            dbg("DELETED FID\n");
            print_file_info(info, depth);
        }
        dbg("Len: %d, padding: %d\n", flen, padding);
        *pos = *pos + flen + padding;
        note("\n");
    } else {
        msg("Ident: %x\n", le16_to_cpu(fid->descTag.tagIdent));
        uint8_t *fidarray = (uint8_t *)fid;
        for(int i=0; i<80;) {
            for(int j=0; j<8; j++, i++) {
                note("%02x ", fidarray[i]);
            }
            note("\n");
        }
        return 1;
    }

    free(info.filename);
    return 0;
}

/**
 * \brief Pair function capturing used space and its position
 *
 * This function is pair with decrement_used_space()
 * 
 * It only stores information about used:free space ration and positions
 *
 * \param[in,out] *stats file system status contatins fields used for free space counting and bitmaps for position marking
 * \param[in] increment size of space to mark
 * \param[in] its position
 */
void increment_used_space(struct filesystemStats *stats, uint64_t increment, uint32_t position) {
    stats->usedSpace += (increment % stats->blocksize == 0 ? increment/stats->blocksize : increment/stats->blocksize+1)*stats->blocksize;
    markUsedBlock(stats, position, increment % stats->blocksize == 0 ? increment/stats->blocksize : increment/stats->blocksize+1, MARK_BLOCK);
#if DEBUG
    uint64_t bits = count_used_bits(stats);
    dwarn("INCREMENT to %d (%d) / (%d)\n", stats->usedSpace, stats->usedSpace/stats->blocksize, bits);
#endif
}

/**
 * \brief Pair function capturing used space and its position
 *
 * This function is pair with increment_used_space()
 * 
 * It only stores information about used:free space ration and positions
 *
 * \param[in,out] *stats file system status contatins fields used for free space counting and bitmaps for position marking
 * \param[in] increment size of space to mark
 * \param[in] its position
 */
void decrement_used_space(struct filesystemStats *stats, uint64_t increment, uint32_t position) {
    stats->usedSpace -= (increment % stats->blocksize == 0 ? increment/stats->blocksize : increment/stats->blocksize+1)*stats->blocksize;
    markUsedBlock(stats, position, increment % stats->blocksize == 0 ? increment/stats->blocksize : increment/stats->blocksize+1, UNMARK_BLOCK);
#if DEBUG
    uint64_t bits = count_used_bits(stats);
    dwarn("DECREMENT to %d (%d) / (%d)\n", stats->usedSpace, stats->usedSpace/stats->blocksize, bits);
#endif
}

/**
 * \brief (E)FE parsing function
 *
 * This function parses thru file tree, made of FE. It is complement to inspect_fid() function, which parses FIDs.
 * 
 * It fixes *Unifinished writes*, *File modifiacation timestamps* (or records them for LVID fix, depending on error) and *Unique ID*.
 *
 * When it finds directory, it calls inspect_fid() to process its contents.
 *
 * \param[in,out] *dev memory mapped device
 * \param[in] *disc udf_disc structure
 * \param[in] lbnlsn LBN offset against LSN
 * \param[in] lsn actual LSN
 * \param[in,out] *stats file system status
 * \param[in] depth depth of FE for printing
 * \param[in] uuid Unique ID from parent FID
 * \param[in] info file information structure for easier handling for print
 * \param[in] *seq VDS sequence
 *
 * \return 4 -- No correct LVD found
 * \return 4 -- Checksum failed
 * \return 4 -- CRC failed 
 * \return 32 -- removed unfinished file
 * \return sum of status returned from inspect_fid(), translate_fid() or own actions (4 for unfixed error, 1 for fixed error, 0 for no error)
 */
uint8_t get_file(int fd, uint8_t **dev, const struct udf_disc *disc, size_t st_size, uint32_t lbnlsn, uint32_t lsn, struct filesystemStats *stats, uint32_t depth, uint32_t uuid, struct fileInfo info, vds_sequence_t *seq ) {
    tag descTag;
    struct fileIdentDesc *fid;
    struct fileEntry *fe;
    struct extendedFileEntry *efe;
    int vds = -1;

    if((vds=get_correct(seq, TAG_IDENT_LVD)) < 0) {
        err("No correct LVD found. Aborting.\n");
        return 4;
    }

    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[vds]->logicalBlockSize);  
    uint32_t lsnBase = lbnlsn; 
    uint32_t flen, padding;
    uint8_t dir = 0;
    uint8_t status = 0;
    uint32_t chunksize = CHUNK_SIZE;
    uint32_t chunk = 0;
    uint32_t offset = 0;
    uint64_t position = 0;

    dwarn("\n(%d) ---------------------------------------------------\n", lsn);
    position = lbSize*lsn; 
    chunk = position/chunksize;
    offset = position%chunksize;
    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

    descTag = *(tag *)(dev[chunk]+offset);
    if(!checksum(descTag)) {
        err("Tag checksum failed. Unable to continue.\n");
        return 4;
    }

    dbg("global FE increment.\n");
    dbg("usedSpace: %d\n", stats->usedSpace);
    increment_used_space(stats, lbSize, lsn-lbnlsn);
    dbg("usedSpace: %d\n", stats->usedSpace);
    switch(le16_to_cpu(descTag.tagIdent)) {
        case TAG_IDENT_FE:
        case TAG_IDENT_EFE:
            dir = 0;
            fe = (struct fileEntry *)(dev[chunk]+offset);
            efe = (struct extendedFileEntry *)fe;
            uint8_t ext = 0;

            if(le16_to_cpu(descTag.tagIdent) == TAG_IDENT_EFE) {
                dwarn("[EFE]\n");
                if(crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs))) {
                    err("EFE CRC failed.\n");
                    int cont = 0;
                    if(interactive) {
                        if(prompt("Continue with caution, yes? [Y/n] ")) {
                            cont = 1;
                        }
                    }
                    if(cont == 0) {
                        unmap_chunk(dev, chunk, st_size); 
                        return 4;
                    }
                }
                ext = 1;
            } else {
                if(crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs))) {
                    err("FE CRC failed.\n");
                    int cont = 0;
                    if(interactive) {
                        if(prompt("Continue with caution, yes? [Y/n] ")) {
                            cont = 1;
                        }
                    }
                    if(cont == 0) {
                        unmap_chunk(dev, chunk, st_size); 
                        return 4;
                    }
                }
            }
            dbg("Tag Serial Num: %d\n", descTag.tagSerialNum);
            if(stats->AVDPSerialNum != descTag.tagSerialNum) {
                err("(%s) Tag Serial Number differs.\n", info.filename);
                uint8_t fixsernum = autofix;
                if(interactive) {
                    if(prompt("Fix it? [Y/n] ")) {
                        fixsernum = 1;
                    } else {
                        status |= 4;
                    }
                }
                if(fixsernum) {
                    if(lsn==1704005)
                        dbg("[1704005] fixsernum");
                    descTag.tagSerialNum = stats->AVDPSerialNum;
                    if(ext) {
                        efe->descTag.descCRC = calculate_crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs));
                        efe->descTag.tagChecksum = calculate_checksum(efe->descTag);
                    } else {
                        fe->descTag.descCRC = calculate_crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs));
                        fe->descTag.tagChecksum = calculate_checksum(fe->descTag);
                    }
                    status |= 1;
                }
            }
            dbg("\nFE, LSN: %d, EntityID: %s ", lsn, fe->impIdent.ident);
            dbg("fileLinkCount: %d, LB recorded: %lu\n", fe->fileLinkCount, ext ? efe->logicalBlocksRecorded: fe->logicalBlocksRecorded);
            uint32_t lea = ext ? efe->lengthExtendedAttr : fe->lengthExtendedAttr;
            uint32_t lad =  ext ? efe->lengthAllocDescs : fe->lengthAllocDescs;
            dbg("LEA %d, LAD %d\n", lea, lad);
            dbg("Information Length: %d\n", fe->informationLength);


            if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) != ICBTAG_FLAG_AD_IN_ICB && fe->icbTag.fileType == ICBTAG_FILE_TYPE_REGULAR && (fe->informationLength % stats->blocksize == 0? fe->informationLength/stats->blocksize : fe->informationLength/stats->blocksize + 1) != (ext ? efe->logicalBlocksRecorded : fe->logicalBlocksRecorded)) {
                dbg("InfLenBlocks: %d\n", fe->informationLength % stats->blocksize == 0? fe->informationLength/stats->blocksize : fe->informationLength/stats->blocksize + 1);
                dbg("BlocksRecord: %d\n", ext ? efe->logicalBlocksRecorded : fe->logicalBlocksRecorded);
                err("(%s) File size mismatch. Probably unfinished file write.\n", info.filename);
                int fixit = 0;

                if(interactive) {
                    if(prompt("Fix it? [Y/n] ")) {
                        fixit = 1;
                    } else {
                        status |= 4;
                    }
                } else if(autofix) {
                    fixit = 1;
                } 

                if(fixit) {
                    imp("Removing unfinished file...\n");
                    dbg("global FE decrement.\n");
                    dbg("usedSpace: %d\n", stats->usedSpace);
                    decrement_used_space(stats, lbSize, lsn-lbnlsn);
                    dbg("usedSpace: %d\n", stats->usedSpace);
                    uint8_t *blank;
                    blank = malloc(stats->blocksize);
                    memcpy(fe, blank, stats->blocksize);
                    free(blank);
                    //unmap_chunk(dev, chunk, st_size); 
                    sync_chunk(dev, chunk, st_size);
                    return 32; 
                }
            }

            info.size = fe->informationLength;
            info.fileType = fe->icbTag.fileType;
            info.permissions = fe->permissions;
            dbg("Permissions: 0x%04x : 0x%04x\n", info.permissions, fe->permissions);

            switch(fe->icbTag.fileType) {
                case ICBTAG_FILE_TYPE_UNDEF:
                    dbg("Filetype: undef\n");
                    break;  
                case ICBTAG_FILE_TYPE_USE:
                    dbg("Filetype: USE\n");
                    break;  
                case ICBTAG_FILE_TYPE_PIE:
                    dbg("Filetype: PIE\n");
                    break;  
                case ICBTAG_FILE_TYPE_IE:
                    dbg("Filetype: IE\n");
                    break;  
                case ICBTAG_FILE_TYPE_DIRECTORY:
                    dbg("Filetype: DIR\n");
                    stats->countNumOfDirs ++;
                    // stats->usedSpace += lbSize;
                    //increment_used_space(stats, lbSize);
                    dir = 1;
                    break;  
                case ICBTAG_FILE_TYPE_REGULAR:
                    dbg("Filetype: REGULAR\n");
                    stats->countNumOfFiles ++;
                    //                    stats->usedSpace += lbSize;
                    break;  
                case ICBTAG_FILE_TYPE_BLOCK:
                    dbg("Filetype: BLOCK\n");
                    stats->countNumOfFiles ++;
                    break;  
                case ICBTAG_FILE_TYPE_CHAR:
                    dbg("Filetype: CHAR\n");
                    stats->countNumOfFiles ++;
                    break;  
                case ICBTAG_FILE_TYPE_EA:
                    dbg("Filetype: EA\n");
                    break;  
                case ICBTAG_FILE_TYPE_FIFO:
                    dbg("Filetype: FIFO\n");
                    stats->countNumOfFiles ++;
                    break;  
                case ICBTAG_FILE_TYPE_SOCKET:
                    dbg("Filetype: SOCKET\n");
                    break;  
                case ICBTAG_FILE_TYPE_TE:
                    dbg("Filetype: TE\n");
                    break;  
                case ICBTAG_FILE_TYPE_SYMLINK:
                    dbg("Filetype: SYMLINK\n");
                    stats->countNumOfFiles ++;
                    break;  
                case ICBTAG_FILE_TYPE_STREAMDIR:
                    dbg("Filetype: STRAMDIR\n");
                    //stats->usedSpace += lbSize;
                    break; 
                default:
                    dbg("Unknown filetype\n");
                    break; 
            }

            dbg("numEntries: %d\n", fe->icbTag.numEntries);
            dbg("Parent ICB loc: %d\n", fe->icbTag.parentICBLocation.logicalBlockNum);

            double cts = 0;
            if((cts = compare_timestamps(stats->LVIDtimestamp, ext ? efe->modificationTime : fe->modificationTime)) < 0) {
                err("(%s) File timestamp is later than LVID timestamp. LVID need to be fixed.\n", info.filename);
#ifdef DEBUG
                err("CTS: %f\n", cts);
#endif
                seq->lvid.error |= E_TIMESTAMP; 
            }
            info.modTime = ext ? efe->modificationTime : fe->modificationTime;


            uint64_t feUUID = (ext ? efe->uniqueID : fe->uniqueID);
            dbg("Unique ID: FE: %"PRIu64" FID: %"PRIu32"\n", (feUUID), uuid); //PRIu32 is fixing uint32_t printing
            int fixuuid = 0;
            if(uuid != feUUID) {
                err("(%s) FE Unique ID differs from FID Unique ID.\n", info.filename);
                if(interactive) {
                    if(prompt("Fix it (set Unique ID to %d, value according FID)? [Y/n] ", uuid) != 0) {
                        fixuuid = 1;
                    } else {
                        status |= 4;
                    }
                }
                if(autofix) {
                    fixuuid = 1;
                }
            }
            if(fixuuid) {
                    if(lsn==1704005)
                        dbg("[1704005] fixuuid");
                if(ext) {
                    efe->uniqueID = uuid;
                    efe->descTag.descCRC = calculate_crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs));
                    efe->descTag.tagChecksum = calculate_checksum(efe->descTag);
                } else {
                    fe->uniqueID = uuid;
                    fe->descTag.descCRC = calculate_crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs));
                    fe->descTag.tagChecksum = calculate_checksum(fe->descTag);
                }
                status |= 1;
            }

            dbg("FC: %04d DC: %04d ", stats->countNumOfFiles, stats->countNumOfDirs);
            print_file_info(info, depth);

            uint8_t fid_inspected = 0;
            uint8_t *allocDescs = (ext ? efe->allocDescs : fe->allocDescs) + lea; 
            if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT) {
                if(dir) {
                    fid_inspected = 1;
                    translate_fid(fd, dev, disc, st_size, lbnlsn, lsn, allocDescs, lad, ICBTAG_FLAG_AD_SHORT, stats, depth, seq, &status);
                } else {
                    dbg("SHORT\n");
                    dbg("LAD: %d, N: %d, rest: %d\n", lad, lad/sizeof(short_ad), lad%sizeof(short_ad));
                    for(int si = 0; si < lad/sizeof(short_ad); si++) {
                        dwarn("SHORT #%d\n", si);
                        short_ad *sad = (short_ad *)(allocDescs + si*sizeof(short_ad));
                        dbg("ExtLen: %d, ExtLoc: %d\n", sad->extLength, sad->extPosition);

                        dbg("usedSpace: %d\n", stats->usedSpace);
                        uint32_t usedsize = sad->extLength;
                        dbg("Used size: %d\n", usedsize);
                        increment_used_space(stats, usedsize, sad->extPosition);
                        lsn = lsn + sad->extLength/lbSize;
                        dbg("LSN: %d, ExtLocOrig: %d\n", lsn, sad->extPosition);
                        dbg("usedSpace: %d\n", stats->usedSpace);
                        dwarn("Size: %d, Blocks: %d\n", usedsize, usedsize/lbSize);
                    }
                }
            } else if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG) {
                if(dir) {
                    fid_inspected = 1;
                    translate_fid(fd, dev, disc, st_size, lbnlsn, lsn, allocDescs, lad, ICBTAG_FLAG_AD_LONG, stats, depth, seq, &status);
                } else {
                    for(int si = 0; si < lad/sizeof(long_ad); si++) {
                        dbg("LONG\n");
                        long_ad *lad = (long_ad *)(allocDescs + si*sizeof(long_ad));
                        dbg("ExtLen: %d, ExtLoc: %d\n", lad->extLength/lbSize, lad->extLocation.logicalBlockNum+lsnBase);

                        dbg("usedSpace: %d\n", stats->usedSpace);
                        uint32_t usedsize = lad->extLength;//(fe->informationLength%lbSize == 0 ? fe->informationLength : (fe->informationLength + lbSize - fe->informationLength%lbSize));
                        increment_used_space(stats, usedsize, lad->extLocation.logicalBlockNum);
                        lsn = lsn + lad->extLength/lbSize;
                        dbg("LSN: %d\n", lsn);
                        dbg("usedSpace: %d\n", stats->usedSpace);
                        dwarn("Size: %d, Blocks: %d\n", usedsize, usedsize/lbSize);
                    }
                }
            } else if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_EXTENDED) {
                if(dir) {
                    fid_inspected = 1;
                    translate_fid(fd, dev, disc, st_size, lbnlsn, lsn, allocDescs, lad, ICBTAG_FLAG_AD_EXTENDED, stats, depth, seq, &status);
                } else {
                    err("EAD found. Please report.\n");
                }
            } else if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB) {
                dbg("AD in ICB\n");
                struct extendedAttrHeaderDesc eahd;
                struct genericFormat *gf;
                struct impUseExtAttr *impAttr;
                struct appUseExtAttr *appAttr;
                tag *descTag;
                uint8_t *array;
                uint8_t *base = NULL;
                if(ext) {
                    eahd = *(struct extendedAttrHeaderDesc *)(efe + sizeof(struct extendedFileEntry) + efe->lengthExtendedAttr);
                    descTag = (tag *)((uint8_t *)(efe) + sizeof(struct extendedFileEntry) + efe->lengthExtendedAttr); 
#ifdef MEMTRACE
                    dbg("efe: %p, POS: %d, descTag: %p\n",efe,  sizeof(struct extendedFileEntry) + efe->lengthExtendedAttr, descTag);
#endif
                } else {    
                    eahd = *(struct extendedAttrHeaderDesc *)(fe + sizeof(struct fileEntry) + fe->lengthExtendedAttr);
                    descTag = (tag *)((uint8_t *)(fe) + sizeof(struct fileEntry) + fe->lengthExtendedAttr); 
#ifdef MEMTRACE
                    dbg("fe: %p, POS: %d, descTag: %p\n", fe, sizeof(struct fileEntry) + fe->lengthExtendedAttr, descTag);
#endif
                }
                array = (uint8_t *)descTag;

                if(descTag->tagIdent == TAG_IDENT_EAHD) {
                    base = (ext ? efe->allocDescs : fe->allocDescs) + eahd.appAttrLocation;

                    dbg("impAttrLoc: %d, appAttrLoc: %d\n", eahd.impAttrLocation, eahd.appAttrLocation);
                    gf = (struct genericFormat *)(fe->allocDescs + eahd.impAttrLocation);

                    dbg("AttrType: %d\n", gf->attrType);
                    dbg("AttrLength: %d\n", gf->attrLength);
                    if(gf->attrType == EXTATTR_IMP_USE) {
                        impAttr = (struct impUseExtAttr *)gf;
                        dbg("ImpUseLength: %d\n", impAttr->impUseLength);
                        dbg("ImpIdent: Flags: 0x%02x\n", impAttr->impIdent.flags);
                        dbg("ImpIdent: Ident: %s\n", impAttr->impIdent.ident);
                        dbg("ImpIdent: IdentSuffix: "); 
                        for(int k=0; k<8; k++) {
                            note("0x%02x ", impAttr->impIdent.identSuffix[k]);
                        }
                        note("\n");
                    } else {
                        err("EAHD mismatch. Expected IMP, found %d\n", gf->attrType);
                    }

                    gf = (struct genericFormat *)(fe->allocDescs + eahd.appAttrLocation);

                    dbg("AttrType: %d\n", gf->attrType);
                    dbg("AttrLength: %d\n", gf->attrLength);
                    if(gf->attrType == EXTATTR_APP_USE) {
                        appAttr = (struct appUseExtAttr *)gf;
                    } else {
                        err("EAHD mismatch. Expected APP, found %d\n", gf->attrType);

                        fid_inspected = 1;
                        for(uint32_t pos=0; ; ) {
                            if(inspect_fid(fd, dev, disc, st_size, lbnlsn, lsn, base, &pos, stats, depth, seq, &status) != 0) {
                                dbg("FID inspection over\n");
                                break;
                            }
                        }
                    }
                } else {
                    dwarn("ID: 0x%02x\n",descTag->tagIdent);
                }

            } else {
                dbg("ICB TAG->flags: 0x%02x\n", fe->icbTag.flags);
            }

            // We can assume that directory have one or more FID inside.
            // FE have inside long_ad/short_ad.
            if(dir && fid_inspected == 0) {
                if(ext) {
                    dbg("[EFE DIR] lengthExtendedAttr: %d\n", efe->lengthExtendedAttr);
                    dbg("[EFE DIR] lengthAllocDescs: %d\n", efe->lengthAllocDescs);
                    for(uint32_t pos=0; pos < efe->lengthAllocDescs; ) {
                        if(inspect_fid(fd, dev, disc, st_size, lbnlsn, lsn, efe->allocDescs + efe->lengthExtendedAttr, &pos, stats, depth+1, seq, &status) != 0) {
                            break;
                        }
                    }
                } else {
                    dbg("[FE DIR] lengthExtendedAttr: %d\n", fe->lengthExtendedAttr);
                    dbg("[FE DIR] lengthAllocDescs: %d\n", fe->lengthAllocDescs);
                    for(uint32_t pos=0; pos < fe->lengthAllocDescs; ) {
                        if(inspect_fid(fd, dev, disc, st_size, lbnlsn, lsn, fe->allocDescs + fe->lengthExtendedAttr, &pos, stats, depth+1, seq, &status) != 0) {
                            break;
                        }
                    }
                }
            }
            break;  
        default:
            err("IDENT: %x, LSN: %d, addr: 0x%x\n", descTag.tagIdent, lsn, lsn*lbSize);
    }            
    // unmap_chunk(dev, chunk, st_size); 
    return status;
}

/**
 * \brief File tree entry point
 *
 * This function is entry for file tree parsing. It actually parses two trees, Stream file tree based on Stream Directory ICB and normal File tree based on Root Directory ICB.
 *
 * \param[in,out] *dev memory mapped device
 * \param[in] *disc udf disc structure
 * \param[in] lbnlsn LBN offset from LSN
 * \pararm[in,out] *stats file system status
 * \param[in] *seq VDS sequence
 *
 * \return sum of returns from stream and normal get_file()
 */
uint8_t get_file_structure(int fd, uint8_t **dev, const struct udf_disc *disc, size_t st_size,  uint32_t lbnlsn, struct filesystemStats *stats, vds_sequence_t *seq ) {
    struct fileEntry *file;
    struct fileIdentDesc *fid;
    tag descTag;
    uint32_t lsn, slsn;

    uint8_t ptLength = 1;
    uint32_t extLoc;
    char *filename;
    uint16_t pos = 0;
    uint32_t lsnBase = lbnlsn; 
    int status = 0;
    uint32_t elen = 0, selen = 0;

    int vds = -1;
    if((vds=get_correct(seq, TAG_IDENT_LVD)) < 0) {
        err("No correct LVD found. Aborting.\n");
        return 4;
    }
    dbg("VDS used: %d\n", vds);
#ifdef MEMTRACE
    dbg("Disc ptr: %p, LVD ptr: %p\n", disc, disc->udf_lvd[vds]);
    dbg("Disc ptr: %p, FSD ptr: %p\n", disc, disc->udf_fsd);
#endif

    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[vds]->logicalBlockSize); 
    // Go to ROOT ICB 
    lb_addr icbloc = lelb_to_cpu(disc->udf_fsd->rootDirectoryICB.extLocation); 
    // Get Stream Dir ICB
    lb_addr sicbloc = lelb_to_cpu(disc->udf_fsd->streamDirectoryICB.extLocation); 

    lsn = icbloc.logicalBlockNum+lsnBase;
    slsn = sicbloc.logicalBlockNum+lsnBase;
    elen = disc->udf_fsd->rootDirectoryICB.extLength;
    selen = disc->udf_fsd->streamDirectoryICB.extLength;
    dbg("ROOT LSN: %d, len: %d, partition: %d\n", lsn, elen, icbloc.partitionReferenceNum);
    dbg("STREAM LSN: %d len: %d, partition: %d\n", slsn, selen, sicbloc.partitionReferenceNum);

    dbg("Used space offset: %d\n", stats->usedSpace);
    struct fileInfo info = {0};

    if(selen > 0) {
        msg("\nStream file tree\n----------------\n");
        status |= get_file(fd, dev, disc, st_size, lbnlsn, slsn, stats, 0, 0, info, seq);
    }
    if(elen > 0) {
        msg("\nMedium file tree\n----------------\n");
        status |= get_file(fd, dev, disc, st_size, lbnlsn, lsn, stats, 0, 0, info, seq);
    }
    return status;
}

/**
 * \brief Support function for appending error to seq structure
 *
 * \param[in,out] seq VDS sequence
 * \param[in] tagIdent identifer of descriptor to append
 * \param[in] vds VDS to search
 * \param[in] error to append
 *
 * \return 0 everything OK
 * \return -1 required descriptor not found
 */
int append_error(vds_sequence_t *seq, uint16_t tagIdent, vds_type_e vds, uint8_t error) {
    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        if(vds == MAIN_VDS) {
            if(seq->main[i].tagIdent == tagIdent) {
                seq->main[i].error |= error;
                return 0;
            }
        } else {
            if(seq->reserve[i].tagIdent == tagIdent) {
                seq->reserve[i].error |= error;
                return 0;
            }
        }
    }
    return -1;
}

/**
 * \brief Support function for getting tag location from seq structure
 *
 * \param[in,out] *seq VDS sequence
 * \param[in] tagIdent identifier of descriptor to find
 * \param[in] vds VDS to search
 *
 * \return requested location if found or UINT32_MAX if not 
 */
uint32_t get_tag_location(vds_sequence_t *seq, uint16_t tagIdent, vds_type_e vds) {
    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        if(vds == MAIN_VDS) {
            if(seq->main[i].tagIdent == tagIdent) {
                return seq->main[i].tagLocation;
            }
        } else {
            if(seq->reserve[i].tagIdent == tagIdent) {
                return seq->reserve[i].tagLocation;
            }
        }
    }
    return -1;
}

/**
 * \brief VDS verification structure
 *
 * This function go thru all VDS descriptors and checks them for checksum, CRC and position. Result are stored using append_error() function.
 *
 * \param[in] *disc UDF disc structure
 * \param[in] vds VDS to search
 * \param[in,out] *seq VDS sequence for error storing
 *
 * \return 0
 */
int verify_vds(struct udf_disc *disc, vds_type_e vds, vds_sequence_t *seq) {
    uint8_t *data;
    uint16_t offset = sizeof(tag);

    if(!checksum(disc->udf_pvd[vds]->descTag)) {
        err("Checksum failure at PVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_PVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_lvd[vds]->descTag)) {
        err("Checksum failure at LVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_LVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_pd[vds]->descTag)) {
        err("Checksum failure at PD[%d]\n", vds);
        append_error(seq, TAG_IDENT_PD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_usd[vds]->descTag)) {
        err("Checksum failure at USD[%d]\n", vds);
        append_error(seq, TAG_IDENT_USD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_iuvd[vds]->descTag)) {
        err("Checksum failure at IUVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_IUVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_td[vds]->descTag)) {
        err("Checksum failure at TD[%d]\n", vds);
        append_error(seq, TAG_IDENT_TD, vds, E_CHECKSUM);
    }

    if(check_position(disc->udf_pvd[vds]->descTag, get_tag_location(seq, TAG_IDENT_PVD, vds))) {
        err("Position failure at PVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_PVD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_lvd[vds]->descTag, get_tag_location(seq, TAG_IDENT_LVD, vds))) {
        err("Position failure at LVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_LVD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_pd[vds]->descTag, get_tag_location(seq, TAG_IDENT_PD, vds))) {
        err("Position failure at PD[%d]\n", vds);
        append_error(seq, TAG_IDENT_PD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_usd[vds]->descTag, get_tag_location(seq, TAG_IDENT_USD, vds))) {
        err("Position failure at USD[%d]\n", vds);
        append_error(seq, TAG_IDENT_USD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_iuvd[vds]->descTag, get_tag_location(seq, TAG_IDENT_IUVD, vds))) {
        err("Position failure at IUVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_IUVD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_td[vds]->descTag, get_tag_location(seq, TAG_IDENT_TD, vds))) {
        err("Position failure at TD[%d]\n", vds);
        append_error(seq, TAG_IDENT_TD, vds, E_POSITION);
    }

    if(crc(disc->udf_pvd[vds], sizeof(struct primaryVolDesc))) {
        err("CRC error at PVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_PVD, vds, E_CRC);
    }
    if(crc(disc->udf_lvd[vds], sizeof(struct logicalVolDesc)+disc->udf_lvd[vds]->mapTableLength)) {
        err("CRC error at LVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_LVD, vds, E_CRC);
    }
    if(crc(disc->udf_pd[vds], sizeof(struct partitionDesc))) {
        err("CRC error at PD[%d]\n", vds);
        append_error(seq, TAG_IDENT_PD, vds, E_CRC);
    }
    if(crc(disc->udf_usd[vds], sizeof(struct unallocSpaceDesc)+(disc->udf_usd[vds]->numAllocDescs)*sizeof(extent_ad))) {
        err("CRC error at USD[%d]\n", vds);
        append_error(seq, TAG_IDENT_USD, vds, E_CRC);
    }
    if(crc(disc->udf_iuvd[vds], sizeof(struct impUseVolDesc))) {
        err("CRC error at IUVD[%d]\n", vds);
        append_error(seq, TAG_IDENT_IUVD, vds, E_CRC);
    }
    if(crc(disc->udf_td[vds], sizeof(struct terminatingDesc))) {
        err("CRC error at TD[%d]\n", vds);
        append_error(seq, TAG_IDENT_TD, vds, E_CRC);
    }

    dbg("Verify VDS done\n");
    return 0;
}

/**
 * \brief Copy descriptor from one position to another on medium
 *
 * Beside actual copy it also fixes declared position, CRC and checksum.
 *
 * \param[in,out] *dev memory mapped device
 * \param[in,out] *disc UDF disc structure
 * \param[in] sectorsize 
 * \param[in] sourcePosition in blocks
 * \param[in] destinationPosition in blocks
 * \param[in] size size of descriptor to copy
 *
 * return 0
 */
int copy_descriptor(int fd, uint8_t **dev, struct udf_disc *disc, size_t st_size, size_t sectorsize, uint32_t sourcePosition, uint32_t destinationPosition, size_t size) {
    tag sourceDescTag, destinationDescTag;
    uint8_t *destArray;
    uint32_t offset = 0, chunk = 0;
    uint32_t chunksize = CHUNK_SIZE;

    dbg("source: 0x%x, destination: 0x%x\n", sourcePosition, destinationPosition);

    chunk = (sourcePosition*sectorsize)/chunksize;
    offset = (sourcePosition*sectorsize)%chunksize;
    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

    sourceDescTag = *(tag *)(dev[chunk]+offset);
    memcpy(&destinationDescTag, &sourceDescTag, sizeof(tag));
    destinationDescTag.tagLocation = destinationPosition;
    destinationDescTag.tagChecksum = calculate_checksum(destinationDescTag);

    dbg("srcChecksum: 0x%x, destChecksum: 0x%x\n", sourceDescTag.tagChecksum, destinationDescTag.tagChecksum);

    destArray = calloc(1, size);
    memcpy(destArray, &destinationDescTag, sizeof(tag));
    memcpy(destArray+sizeof(tag), dev[chunk]+offset+sizeof(tag), size-sizeof(tag));

    unmap_chunk(dev, chunk, st_size); 
    chunk = (destinationPosition*sectorsize)/chunksize;
    offset = (destinationPosition*sectorsize)%chunksize;
    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

    memcpy(dev[chunk]+offset, destArray, size);

    free(destArray);

    unmap_chunk(dev, chunk, st_size); 

    return 0;
}

/**
 * \brief Writes back specified AVDP from udf_disc structure to device
 *
 * \param[in,out] *dev memory mapped device
 * \param[in,out] *disc UDF disc structure
 * \param[in] sectorsize
 * \param[in] devsize size of device in bytes
 * \param[in] source source AVDP
 * \param[in] target target AVDP
 *
 * \return 0 everything OK
 * \return -2 after write checksum failed 
 * \return -4 after write CRC failed
 */
int write_avdp(int fd, uint8_t **dev, struct udf_disc *disc, size_t sectorsize, size_t devsize,  avdp_type_e source, avdp_type_e target) {
    uint64_t sourcePosition = 0;
    uint64_t targetPosition = 0;
    tag desc_tag;
    avdp_type_e type = target;
    uint32_t offset = 0, chunk = 0;
    uint32_t chunksize = CHUNK_SIZE;

    // Taget type to determine position on media
    if(source == 0) {
        sourcePosition = sectorsize*256; //First AVDP is on LSN=256
    } else if(source == 1) {
        sourcePosition = devsize-sectorsize; //Second AVDP is on last LSN
    } else if(source == 2) {
        sourcePosition = devsize-sectorsize-256*sectorsize; //Third AVDP can be at last LSN-256
    } else {
        sourcePosition = sectorsize*512; //Unclosed disc have AVDP at sector 512
    }

    // Taget type to determine position on media
    if(target == 0) {
        targetPosition = sectorsize*256; //First AVDP is on LSN=256
    } else if(target == 1) {
        targetPosition = devsize-sectorsize; //Second AVDP is on last LSN
    } else if(target == 2) {
        targetPosition = devsize-sectorsize-256*sectorsize; //Third AVDP can be at last LSN-256
    } else {
        targetPosition = sectorsize*512; //Unclosed disc have AVDP at sector 512
        type = FIRST_AVDP; //Save it to FIRST_AVDP positon
    }

    dbg("DevSize: %zu\n", devsize);
    dbg("Current position: %lx\n", targetPosition);

    copy_descriptor(fd, dev, disc, devsize, sectorsize, sourcePosition/sectorsize, targetPosition/sectorsize, sizeof(struct anchorVolDescPtr));

    free(disc->udf_anchor[type]);
    disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP

    chunk = targetPosition/chunksize;
    offset = targetPosition%chunksize;
    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
    map_chunk(fd, dev, chunk, devsize, __FILE__, __LINE__); 

    desc_tag = *(tag *)(dev[chunk]+offset);

    if(!checksum(desc_tag)) {
        err("Checksum failure at AVDP[%d]\n", type);
        map_chunk(fd, dev, chunk, devsize, __FILE__, __LINE__); 
        return -2;
    } else if(le16_to_cpu(desc_tag.tagIdent) != TAG_IDENT_AVDP) {
        err("AVDP not found at 0x%lx\n", targetPosition);
        map_chunk(fd, dev, chunk, devsize, __FILE__, __LINE__); 
        return -4;
    }

    memcpy(disc->udf_anchor[type], dev[chunk]+offset, sizeof(struct anchorVolDescPtr));

    if(crc(disc->udf_anchor[type], sizeof(struct anchorVolDescPtr))) {
        err("CRC error at AVDP[%d]\n", type);
        map_chunk(fd, dev, chunk, devsize, __FILE__, __LINE__); 
        return -3;
    }

    imp("AVDP[%d] successfully written.\n", type);
    map_chunk(fd, dev, chunk, devsize, __FILE__, __LINE__); 
    return 0;
}

/**
 * \brief Fix target AVDP's extent length
 *
 * \param[in,out] *dev memory mapped device
 * \param[in,out] *disc UDF disc structure
 * \param[in] sectorsize
 * \param[in] devsize size of device in bytes
 * \param[in] target target AVDP
 *
 * \return 0 everything OK
 * \return -2 checksum failed 
 * \return -4 CRC failed
 */
int fix_avdp(int fd, uint8_t **dev, struct udf_disc *disc, size_t sectorsize, size_t devsize,  avdp_type_e target) {
    uint64_t targetPosition = 0;
    tag desc_tag;
    avdp_type_e type = target;
    uint32_t offset = 0, chunk = 0;
    uint32_t chunksize = CHUNK_SIZE;

    // Taget type to determine position on media
    if(target == 0) {
        targetPosition = sectorsize*256; //First AVDP is on LSN=256
    } else if(target == 1) {
        targetPosition = devsize-sectorsize; //Second AVDP is on last LSN
    } else if(target == 2) {
        targetPosition = devsize-sectorsize-256*sectorsize; //Third AVDP can be at last LSN-256
    } else {
        targetPosition = sectorsize*512; //Unclosed disc have AVDP at sector 512
        type = FIRST_AVDP; //Save it to FIRST_AVDP positon
    }

    dbg("DevSize: %zu\n", devsize);
    dbg("Current position: %lx\n", targetPosition);

    chunk = targetPosition/chunksize;
    offset = targetPosition%chunksize;
    dbg("Chunk: %d, offset: 0x%x\n", chunk, offset);
    map_chunk(fd, dev, chunk, devsize, __FILE__, __LINE__); 

    desc_tag = *(tag *)(dev[chunk]+offset);

    if(!checksum(desc_tag)) {
        err("Checksum failure at AVDP[%d]\n", type);
        return -2;
    } else if(le16_to_cpu(desc_tag.tagIdent) != TAG_IDENT_AVDP) {
        err("AVDP not found at 0x%lx\n", targetPosition);
        return -4;
    }

    if(disc->udf_anchor[type]->mainVolDescSeqExt.extLength > disc->udf_anchor[type]->reserveVolDescSeqExt.extLength) { //main is bigger
        if(disc->udf_anchor[type]->mainVolDescSeqExt.extLength >= 16*sectorsize) { //and is big enough
            disc->udf_anchor[type]->reserveVolDescSeqExt.extLength = disc->udf_anchor[type]->mainVolDescSeqExt.extLength;
        } 
    } else { //reserve is bigger
        if(disc->udf_anchor[type]->reserveVolDescSeqExt.extLength >= 16*sectorsize) { //and is big enough
            disc->udf_anchor[type]->mainVolDescSeqExt.extLength = disc->udf_anchor[type]->reserveVolDescSeqExt.extLength;
        } 
    }
    disc->udf_anchor[type]->descTag.descCRC = calculate_crc(disc->udf_anchor[type], sizeof(struct anchorVolDescPtr));
    disc->udf_anchor[type]->descTag.tagChecksum = calculate_checksum(disc->udf_anchor[type]->descTag);

    memcpy(dev[chunk]+offset, disc->udf_anchor[type], sizeof(struct anchorVolDescPtr));

    imp("AVDP[%d] Extent Length successfully fixed.\n", type);
    return 0;
}

/**
 * \brief Get descriptor name string
 *
 * \param[in] descIdent descriptor identifier
 * 
 * \return constant char array
 */
char * descriptor_name(uint16_t descIdent) {
    switch(descIdent) {
        case TAG_IDENT_PVD:
            return "PVD";
        case TAG_IDENT_LVD:
            return "LVD";
        case TAG_IDENT_PD:
            return "PD";
        case TAG_IDENT_USD:
            return "USD";
        case TAG_IDENT_IUVD:
            return "IUVD";
        case TAG_IDENT_TD:
            return "TD";
        case TAG_IDENT_AVDP:
            return "AVDP";
        case TAG_IDENT_LVID:
            return "LVID";
        default:
            return "Unknown";
    }
}

/**
 * \brief Fix VDS sequence 
 *
 * This function go thru VDS and if find error, check second VDS. If secondary is ok, copy it, if not, report unrecoverable error.
 *
 * \param[in,out] *dev memory mapped medium
 * \param[in,out] *disc UDF disc structure
 * \param[in] sectorsize 
 * \param[in] source AVDP pointing to VDS
 * \param[in,out] *seq VDS sequence
 *
 * \return sum of 0, 1 and 4 according fixing and found errors
 */
int fix_vds(int fd, uint8_t **dev, struct udf_disc *disc, size_t st_size, size_t sectorsize, avdp_type_e source, vds_sequence_t *seq) { 
    uint32_t position_main, position_reserve;
    int8_t counter = 0;
    tag descTag;
    uint8_t fix=0;
    uint8_t status = 0;

    // Go to first address of VDS
    position_main = (disc->udf_anchor[source]->mainVolDescSeqExt.extLocation);
    position_reserve = (disc->udf_anchor[source]->reserveVolDescSeqExt.extLocation);


    msg("\nVDS verification status\n-----------------------\n");

    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        if(seq->main[i].error != 0 && seq->reserve[i].error != 0) {
            //Both descriptors are broken
            //TODO It can be possible to reconstruct some descriptors, but not all. 
            err("[%d] Both descriptors are broken. Maybe not able to continue later.\n",i);     
        } else if(seq->main[i].error != 0) {
            //Copy Reserve -> Main
            if(interactive) {
                fix = prompt("%s is broken. Fix it? [Y/n]", descriptor_name(seq->reserve[i].tagIdent)); 
            } else if (autofix) {
                fix = 1;
            }

            //int copy_descriptor(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, uint32_t sourcePosition, uint32_t destinationPosition, size_t amount);
            if(fix) {
                warn("[%d] Fixing Main %s\n",i,descriptor_name(seq->reserve[i].tagIdent));
                warn("sectorsize: %d\n", sectorsize);
                warn("src pos: 0x%x\n", position_reserve + i);
                warn("dest pos: 0x%x\n", position_main + i);
                //                memcpy(position_main + i*sectorsize, position_reserve + i*sectorsize, sectorsize);
                copy_descriptor(fd, dev, disc, st_size, sectorsize, position_reserve + i, position_main + i, sectorsize);
                status |= 1;
            } else {
                err("[%i] %s is broken.\n", i,descriptor_name(seq->reserve[i].tagIdent));
                status |= 4;
            }
            fix = 0;
        } else if(seq->reserve[i].error != 0) {
            //Copy Main -> Reserve
            if(interactive) {
                fix = prompt("%s is broken. Fix it? [Y/n]", descriptor_name(seq->main[i].tagIdent)); 
            } else if (autofix) {
                fix = 1;
            }

            if(fix) {
                warn("[%i] Fixing Reserve %s\n", i,descriptor_name(seq->main[i].tagIdent));
                copy_descriptor(fd, dev, disc, st_size, sectorsize, position_reserve + i, position_main + i, sectorsize);
                status |= 1;
            } else {
                err("[%i] %s is broken.\n", i,descriptor_name(seq->main[i].tagIdent));
                status |= 4;
            }
            fix = 0;
        } else {
            msg("[%d] %s is fine. No fixing needed.\n", i, descriptor_name(seq->main[i].tagIdent));
        }
        if(seq->main[i].tagIdent == TAG_IDENT_TD)
            break;
    }


    return status;
}

static const unsigned char BitsSetTable256[256] = 
{
#define B2(n) n,     n+1,     n+1,     n+2
#define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
}; ///< Support array for bit counting

/**
 * \brief Fix PD Partition Header contents
 *
 * At this moment it fixes only SBD, because no other descriptors were found.
 *
 * \param[in,out] *dev memory mapped medium
 * \param[in] *disc UDF disc
 * \param[in] sectorsize
 * \param[in,out] *stats file system status
 * \param[in] *seq VDS sequence
 *
 * \return 0 -- All OK
 * \return 1 -- PD SBD recovery failed
 * \return 4 -- No correct PD found
 * \return -1 -- no SBD found even if declared
 */
int fix_pd(int fd, uint8_t **dev, struct udf_disc *disc, size_t st_size, size_t sectorsize, struct filesystemStats *stats, vds_sequence_t *seq) {
    int vds = -1;
    uint32_t chunksize = CHUNK_SIZE;
    uint32_t chunk = 0;
    uint32_t offset = 0;

    if((vds=get_correct(seq, TAG_IDENT_PD)) < 0) {
        err("No correct PD found. Aborting.\n");
        return 4;
    }
    struct partitionHeaderDesc *phd = (struct partitionHeaderDesc *)(disc->udf_pd[vds]->partitionContentsUse);
    dbg("[USD] UST pos: %d, len: %d\n", phd->unallocSpaceTable.extPosition, phd->unallocSpaceTable.extLength);
    dbg("[USD] USB pos: %d, len: %d\n", phd->unallocSpaceBitmap.extPosition, phd->unallocSpaceBitmap.extLength);
    dbg("[USD] FST pos: %d, len: %d\n", phd->freedSpaceTable.extPosition, phd->freedSpaceTable.extLength);
    dbg("[USD] FSB pos: %d, len: %d\n", phd->freedSpaceBitmap.extPosition, phd->freedSpaceBitmap.extLength);

    if(phd->unallocSpaceTable.extLength > 0) {
        //Unhandled. Not found on any medium.
        err("[USD] Unallocated Space Table is unhandled. Skipping.\n");
    }
    if(phd->freedSpaceTable.extLength > 0) {
        //Unhandled. Not found on any medium.
        err("[USD] Free Space Table is unhandled. Skipping.\n");
    }
    if(phd->freedSpaceBitmap.extLength > 0) {
        //Unhandled. Not found on any medium.
        err("[USD] Unallocated Space Table is unhandled. Skipping.\n");
    }

    if(phd->unallocSpaceBitmap.extLength > 3) { //0,1,2,3 are special values ECMA 167r3 4/14.14.1.1
        uint32_t lsnBase = disc->udf_pd[vds]->partitionStartingLocation;       

        chunk = ((lsnBase + phd->unallocSpaceBitmap.extPosition)*sectorsize)/chunksize;
        offset = ((lsnBase + phd->unallocSpaceBitmap.extPosition)*sectorsize)%chunksize;
        map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

        struct spaceBitmapDesc *sbd = (struct spaceBitmapDesc *)(dev[chunk]+offset);
        if(sbd->descTag.tagIdent != TAG_IDENT_SBD) {
            err("SBD not found\n");
            return -1;
        }
        dbg("[SBD] NumOfBits: %d\n", sbd->numOfBits);
        dbg("[SBD] NumOfBytes: %d\n", sbd->numOfBytes);
        dbg("[SBD] Chunk: %d, Offset: %d\n", chunk, offset);

#ifdef MEMTRACE    
        dbg("Bitmap: %d, %p\n", (lsnBase + phd->unallocSpaceBitmap.extPosition), sbd->bitmap);
#else
        dbg("Bitmap: %d\n", (lsnBase + phd->unallocSpaceBitmap.extPosition));
#endif
        memcpy(sbd->bitmap, stats->actPartitionBitmap, sbd->numOfBytes);
        dbg("MEMCPY DONE\n");

        //Recalculate CRC and checksum
        sbd->descTag.descCRC = calculate_crc(sbd, sbd->descTag.descCRCLength + sizeof(tag));
        sbd->descTag.tagChecksum = calculate_checksum(sbd->descTag);
        
        imp("PD SBD recovery was successful.\n");
        return 0;
    }
    err("PD SBD recovery failed.\n");
    return 1;
}

/**
 * \brief Get PD Partition Header contents
 *
 * At this moment handles only SBD, because none other was found.
 *
 * \param[in] *dev memory mapped device
 * \param[in] *disc UDF disc
 * \param[in] sectorsize 
 * \param[out] *stats filesystem status
 * \param[in] *seq VDS sequence
 *
 * \return 0 -- All OK
 * \return 4 --No correct PD found
 * \return -1 -- SBD not found even if declared
 * \return -128 -- UST, FST or FSB found
 */
int get_pd(int fd, uint8_t **dev, struct udf_disc *disc, size_t sectorsize, size_t st_size, struct filesystemStats *stats, vds_sequence_t *seq) { 
    int vds = -1;
    uint32_t offset = 0, chunk = 0;
    uint32_t chunksize = CHUNK_SIZE;
    uint64_t position = 0;

    if((vds=get_correct(seq, TAG_IDENT_PD)) < 0) {
        err("No correct PD found. Aborting.\n");
        return 4;
    }
    struct partitionHeaderDesc *phd = (struct partitionHeaderDesc *)(disc->udf_pd[vds]->partitionContentsUse);
    dbg("[USD] UST pos: %d, len: %d\n", phd->unallocSpaceTable.extPosition, phd->unallocSpaceTable.extLength);
    dbg("[USD] USB pos: %d, len: %d\n", phd->unallocSpaceBitmap.extPosition, phd->unallocSpaceBitmap.extLength);
    dbg("[USD] FST pos: %d, len: %d\n", phd->freedSpaceTable.extPosition, phd->freedSpaceTable.extLength);
    dbg("[USD] FSB pos: %d, len: %d\n", phd->freedSpaceBitmap.extPosition, phd->freedSpaceBitmap.extLength);

    if(phd->unallocSpaceTable.extLength > 0) {
        //Unhandled. Not found on any medium.
        err("[USD] Unallocated Space Table is unhandled. Skipping.\n");
        return -128;
    }
    if(phd->freedSpaceTable.extLength > 0) {
        //Unhandled. Not found on any medium.
        err("[USD] Free Space Table is unhandled. Skipping.\n");
        return -128;
    }
    if(phd->freedSpaceBitmap.extLength > 0) {
        //Unhandled. Not found on any medium.
        err("[USD] Freed Space Bitmap is unhandled. Skipping.\n");
        return -128;
    }
    if(phd->unallocSpaceBitmap.extLength > 3) { //0,1,2,3 are special values ECMA 167r3 4/14.14.1.1
        uint32_t lsnBase = disc->udf_pd[vds]->partitionStartingLocation;      
        dbg("LSNBase: %d\n", lsnBase); 
        position = (lsnBase + phd->unallocSpaceBitmap.extPosition)*sectorsize;
        chunk = position/chunksize;
        offset = position%chunksize;
        map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__);

        struct spaceBitmapDesc *sbd = (struct spaceBitmapDesc *)(dev[chunk]+offset);
        if(sbd->descTag.tagIdent != TAG_IDENT_SBD) {
            err("SBD not found\n");
            return -1;
        }
        if(!checksum(sbd->descTag)) {
            err("SBD checksum error. Continue with caution.\n");
            seq->pd.error |= E_CHECKSUM;
        }
        if(crc(sbd, sbd->descTag.descCRCLength + sizeof(tag))) {
            err("SBD CRC error. Continue with caution.\n");
            seq->pd.error |= E_CRC; 
        }
        dbg("SBD is ok\n");
        dbg("[SBD] NumOfBits: %d\n", sbd->numOfBits);
        dbg("[SBD] NumOfBytes: %d\n", sbd->numOfBytes);
#ifdef MEMTRACE
        dbg("Bitmap: %d, %p\n", (lsnBase + phd->unallocSpaceBitmap.extPosition), sbd->bitmap);
#else
        dbg("Bitmap: %d\n", (lsnBase + phd->unallocSpaceBitmap.extPosition));
#endif

        //Create array for used/unused blocks counting
        stats->actPartitionBitmap = calloc(sbd->numOfBytes, 1);
        memset(stats->actPartitionBitmap, 0xff, sbd->numOfBytes);
        stats->partitionNumOfBytes = sbd->numOfBytes;
        stats->partitionNumOfBits = sbd->numOfBits;

        dbg("Crete array done\n");

        uint8_t *ptr = NULL;
        dbg("Chunk: %d\n", chunk);
        map_raw(fd, &ptr, (uint64_t)(chunk)*CHUNK_SIZE, (sbd->numOfBytes + offset), st_size);
#ifdef MEMTRACE
        dbg("Ptr: %p\n", ptr); 
#endif
        sbd = (struct spaceBitmapDesc *)(ptr+offset);
       
        dbg("Get bitmap statistics\n"); 
        //Get actual bitmap statistics
        uint32_t usedBlocks = 0;
        uint32_t unusedBlocks = 0;
        uint8_t count = 0;
        uint8_t v = 0;
        for(int i=0; i<sbd->numOfBytes-1; i++) {
            v = sbd->bitmap[i];
            //if(i%1000 == 0) dbg("0x%02x %d\n",v, i);
            count = BitsSetTable256[v & 0xff] + BitsSetTable256[(v >> 8) & 0xff] + BitsSetTable256[(v >> 16) & 0xff] + BitsSetTable256[v >> 24];     
            usedBlocks += 8-count;
            unusedBlocks += count;
        }
        dbg("Unused blocks: %d\n", unusedBlocks);
        dbg("Used Blocks: %d\n", usedBlocks);

        uint8_t bitCorrection = sbd->numOfBytes*8-sbd->numOfBits;
        dbg("BitCorrection: %d\n", bitCorrection);
        v = sbd->bitmap[sbd->numOfBytes-1];
        dbg("Bitmap last: 0x%02x\n", v);
        for(int i=0; i<8 - bitCorrection; i++) {
            dbg("Mask: 0x%02x, Result: 0x%02x\n", (1 << i), v & (1 << i));
            if(v & (1 << i))
                unusedBlocks++;
            else
                usedBlocks++;
        }


        stats->expUsedBlocks = usedBlocks;
        stats->expUnusedBlocks = unusedBlocks;
        stats->expPartitionBitmap = sbd->bitmap;
        dbg("Unused blocks: %d\n", unusedBlocks);
        dbg("Used Blocks: %d\n", usedBlocks);

        sbd = (struct spaceBitmapDesc *)(dev[chunk]+offset);
        unmap_raw(&ptr, (uint64_t)(chunk)*CHUNK_SIZE, sbd->numOfBytes);
        unmap_chunk(dev, chunk, st_size);
    }

    //Mark used space
    increment_used_space(stats, phd->unallocSpaceTable.extLength, phd->unallocSpaceTable.extPosition);
    increment_used_space(stats, phd->unallocSpaceBitmap.extLength, phd->unallocSpaceBitmap.extPosition);
    increment_used_space(stats, phd->freedSpaceTable.extLength, phd->freedSpaceTable.extPosition);
    increment_used_space(stats, phd->freedSpaceBitmap.extLength, phd->freedSpaceBitmap.extPosition);

    return 0; 
}

/**
 * \brief Fix LVID values
 *
 * This function fixes only values of LVID. It is not able to fix structurally broken LVID (wrong CRC/checksum).
 *
 * Fixes opened intergrity type, timestamps, amounts of files/directories, free space tables.
 *
 * \param[in,out] *dev memory mapped device
 * \param[in,out] *disc UDF disc
 * \param[in] sectorsize
 * \param[in] *stats filesystem status
 * \param[in] *seq VDS sequence
 *
 * \return 0 -- All Ok
 * \return 4 -- No correct LVD found
 */
int fix_lvid(int fd, uint8_t **dev, struct udf_disc *disc, size_t st_size, size_t sectorsize, struct filesystemStats *stats, vds_sequence_t *seq) {
    int vds = -1;
    uint32_t chunksize = CHUNK_SIZE;
    uint32_t chunk = 0;
    uint32_t offset = 0;

    if((vds=get_correct(seq, TAG_IDENT_LVD)) < 0) {
        err("No correct LVD found. Aborting.\n");
        return 4;
    }

    uint32_t loc = disc->udf_lvd[vds]->integritySeqExt.extLocation;
    uint32_t len = disc->udf_lvd[vds]->integritySeqExt.extLength;
    uint16_t size = sizeof(struct logicalVolIntegrityDesc) + disc->udf_lvid->numOfPartitions*4*2 + disc->udf_lvid->lengthOfImpUse;
    dbg("LVID: loc: %d, len: %d, size: %d\n", loc, len, size);

    chunk = (loc*sectorsize)/chunksize;
    offset = (loc*sectorsize)%chunksize;
    map_chunk(fd, dev, chunk, st_size, __FILE__, __LINE__); 

    struct logicalVolIntegrityDesc *lvid = (struct logicalVolIntegrityDesc *)(dev[chunk]+offset);
    struct impUseLVID *impUse = (struct impUseLVID *)((uint8_t *)(disc->udf_lvid) + sizeof(struct logicalVolIntegrityDesc) + 8*disc->udf_lvid->numOfPartitions); //this is because of ECMA 167r3, 3/24, fig 22

    // Fix PD too
    fix_pd(fd, dev, disc, st_size, sectorsize, stats, seq);

    // Fix files/dir amounts
    impUse->numOfFiles = stats->countNumOfFiles;
    impUse->numOfDirs = stats->countNumOfDirs;

    // Fix Next Unique ID by maximal found +1
    ((struct logicalVolHeaderDesc *)(disc->udf_lvid->logicalVolContentsUse))->uniqueID = stats->maxUUID+1;

    // Set recording date and time to now. 
    time_t t = time(NULL);
    struct tm tmlocal = *localtime(&t);
    struct tm tm = *gmtime(&t);
    int8_t hrso = tmlocal.tm_hour - tm.tm_hour;
    if(hrso > 12 || hrso < -12) {
        hrso += 24;
    }

    int8_t mino = tmlocal.tm_min - tm.tm_min;
    int16_t t_offset = hrso*60+mino;
    dbg("Offset: %d, hrs: %d, min: %d\n", t_offset, hrso, mino);
    dbg("lhr: %d, hr: %d\n", tmlocal.tm_hour, tm.tm_hour);
    timestamp *ts = &(disc->udf_lvid->recordingDateAndTime);
    ts->typeAndTimezone = (1 << 12) | (t_offset >= 0 ? t_offset : (0x1000-t_offset));
    ts->year = tmlocal.tm_year + 1900;
    ts->month = tmlocal.tm_mon + 1;
    ts->day = tmlocal.tm_mday;
    ts->hour = tmlocal.tm_hour;
    ts->minute = tmlocal.tm_min;
    ts->second = tmlocal.tm_sec;
    ts->centiseconds = 0;
    ts->hundredsOfMicroseconds = 0;
    ts->microseconds = 0;
    dbg("Type and Timezone: 0x%04x\n", ts->typeAndTimezone);

    uint32_t newFreeSpace = disc->udf_lvid->freeSpaceTable[1] - stats->usedSpace/sectorsize;
    disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(newFreeSpace);
    dbg("New Free Space: %d\n", disc->udf_lvid->freeSpaceTable[0]);

    // Close integrity (last thing before write)
    disc->udf_lvid->integrityType = LVID_INTEGRITY_TYPE_CLOSE;

    //Recalculate CRC and checksum
    disc->udf_lvid->descTag.descCRC = calculate_crc(disc->udf_lvid, size);
    disc->udf_lvid->descTag.tagChecksum = calculate_checksum(disc->udf_lvid->descTag);
    //Write changes back to medium
    memcpy(lvid, disc->udf_lvid, size);

    unmap_chunk(dev, chunk, st_size); 
    imp("LVID recovery was successful.\n");
    return 0;
}

