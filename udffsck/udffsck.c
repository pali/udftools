
#include <math.h>
#include <time.h>

#include "udffsck.h"
#include "utils.h"
#include "libudffs.h"
#include "list.h"
#include "options.h"

uint8_t get_file(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn, uint32_t lsn, struct filesystemStats *stats, uint32_t depth, uint32_t uuid, struct fileInfo info, vds_sequence_t *seq );
uint64_t uuid_decoder(uint64_t uuid);

#define MAX_DEPTH 100
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

uint8_t calculate_checksum(tag descTag) {
    uint8_t i;
    uint8_t tagChecksum = 0;

    for (i=0; i<16; i++)
        if (i != 4)
            tagChecksum += (uint8_t)(((char *)&(descTag))[i]);

    return tagChecksum;
}

int checksum(tag descTag) {
    uint8_t checksum =  calculate_checksum(descTag);
    dbg("Calc checksum: 0x%02x Tag checksum: 0x%02x\n", checksum, descTag.tagChecksum);
    return checksum == descTag.tagChecksum;
}

uint16_t calculate_crc(void * restrict desc, uint16_t size) {
    uint8_t offset = sizeof(tag);
    tag *descTag = desc;
    uint16_t crc = 0;
    uint16_t calcCrc = udf_crc((uint8_t *)(desc) + offset, size - offset, crc);
    return calcCrc;
}

int crc(void * restrict desc, uint16_t size) {
    uint16_t calcCrc = calculate_crc(desc, size);
    tag *descTag = desc;
    dbg("Calc CRC: 0x%04x, TagCRC: 0x%04x\n", calcCrc, descTag->descCRC);
    return le16_to_cpu(descTag->descCRC) != calcCrc;
}

int check_position(tag descTag, uint32_t position) {
    dbg("tag pos: 0x%x, pos: 0x%x\n", descTag.tagLocation, position);
    return (descTag.tagLocation != position);
}

char * print_timestamp(timestamp ts) {
    static char str[30] = {0};
    sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d.%02d%02d%02d", ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second, ts.centiseconds, ts.hundredsOfMicroseconds, ts.microseconds);
    return str; 
}

time_t timestamp2epoch(timestamp t) {
    struct tm tm;
    tm.tm_year = t.year - 1900;
    tm.tm_mon = t.month - 1; 
    tm.tm_mday = t.day;
    tm.tm_hour = t.hour;
    tm.tm_min = t.minute;
    tm.tm_sec = t.second;
    return mktime(&tm);
}

double compare_timestamps(timestamp a, timestamp b) {
    return difftime(timestamp2epoch(a), timestamp2epoch(b));
}

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

/**
 * @brief Locate AVDP on device and store it
 * @param[in] dev pointer to device array
 * @param[out] disc AVDP is stored in udf_disc structure
 * @param[in] sectorsize device logical sector size
 * @param[in] devsize size of whole device in LSN
 * @param[in] type selector of AVDP - first or second
 * @return  0 everything is ok
 *         -2 AVDP tag checksum failed
 *         -3 AVDP CRC failed
 *         -4 AVDP not found 
 */
int get_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize, avdp_type_e type) {
    int64_t position = 0;
    tag desc_tag;

    if(type == 0) {
        position = sectorsize*256; //First AVDP is on LSN=256
    } else if(type == 1) {
        position = devsize-sectorsize; //Second AVDP is on last LSN
    } else if(type == 2) {
        position = devsize-sectorsize-256*sectorsize; //Third AVDP can be at last LSN-256
    } else {
        position = sectorsize*512; //Unclosed disc have AVDP at sector 512
        type = 0; //Save it to FIRST_AVDP positon
    }

    dbg("DevSize: %zu\n", devsize);
    dbg("Current position: %lx\n", position);

    disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP

    desc_tag = *(tag *)(dev+position);

    if(!checksum(desc_tag)) {
        err("Checksum failure at AVDP[%d]\n", type);
        return E_CHECKSUM;
    } else if(le16_to_cpu(desc_tag.tagIdent) != TAG_IDENT_AVDP) {
        err("AVDP not found at 0x%lx\n", position);
        return E_WRONGDESC;
    }

    memcpy(disc->udf_anchor[type], dev+position, sizeof(struct anchorVolDescPtr));

    if(crc(disc->udf_anchor[type], sizeof(struct anchorVolDescPtr))) {
        err("CRC error at AVDP[%d]\n", type);
        return E_CRC;
    }

    if(check_position(desc_tag, position/sectorsize)) {
        err("Position mismatch at AVDP[%d]\n", type);
        return E_POSITION;
    }

    msg("AVDP[%d] successfully loaded.\n", type);
    return 0;
}


/**
 * @brief Loads Volume Descriptor Sequence (VDS) and stores it at struct udf_disc
 * @param[in] dev pointer to device array
 * @param[out] disc VDS is stored in udf_disc structure
 * @param[in] sectorsize device logical sector size
 * @param[in] vds MAIN_VDS or RESERVE_VDS selector
 * @return 0 everything ok
 *         -3 found unknown tag
 *         -4 structure is already set
 */
int get_vds(uint8_t *dev, struct udf_disc *disc, int sectorsize, avdp_type_e avdp, vds_type_e vds, vds_sequence_t *seq) {
    uint8_t *position;
    int8_t counter = 0;
    tag descTag;

    // Go to first address of VDS
    switch(vds) {
        case MAIN_VDS:
            position = dev+sectorsize*(disc->udf_anchor[avdp]->mainVolDescSeqExt.extLocation);
            break;
        case RESERVE_VDS:
            position = dev+sectorsize*(disc->udf_anchor[avdp]->reserveVolDescSeqExt.extLocation);
            break;
    }
    dbg("Current position: %lx\n", position-dev);

    // Go thru descriptors until TagIdent is 0 or amout is too big to be real
    while(counter < VDS_STRUCT_AMOUNT) {

        // Read tag
        memcpy(&descTag, position, sizeof(descTag));

        dbg("Tag ID: %d\n", descTag.tagIdent);

        if(vds == MAIN_VDS) {
            seq->main[counter].tagIdent = descTag.tagIdent;
            seq->main[counter].tagLocation = (position-dev)/sectorsize;
        } else {
            seq->reserve[counter].tagIdent = descTag.tagIdent;
            seq->reserve[counter].tagLocation = (position-dev)/sectorsize;
        }

        counter++;

        // What kind of descriptor is that?
        switch(le16_to_cpu(descTag.tagIdent)) {
            case TAG_IDENT_PVD:
                if(disc->udf_pvd[vds] != 0) {
                    err("Structure PVD is already set. Probably error at tag or media\n");
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
                    return -4;
                }
                disc->udf_iuvd[vds] = malloc(sizeof(struct impUseVolDesc)); // Prepare memory
                memcpy(disc->udf_iuvd[vds], position, sizeof(struct impUseVolDesc)); 
                break;
            case TAG_IDENT_PD:
                if(disc->udf_pd[vds] != 0) {
                    err("Structure PD is already set. Probably error at tag or media\n");
                    return -4;
                }
                disc->udf_pd[vds] = malloc(sizeof(struct partitionDesc)); // Prepare memory
                memcpy(disc->udf_pd[vds], position, sizeof(struct partitionDesc)); 
                break;
            case TAG_IDENT_LVD:
                if(disc->udf_lvd[vds] != 0) {
                    err("Structure LVD is already set. Probably error at tag or media\n");
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
                    return -4;
                }
                disc->udf_td[vds] = malloc(sizeof(struct terminatingDesc)); // Prepare memory
                memcpy(disc->udf_td[vds], position, sizeof(struct terminatingDesc)); 
                // Found terminator, ending.
                return 0;
            case 0:
                // Found end of VDS, ending.
                return 0;
            default:
                // Unkown TAG
                fatal("Unknown TAG found at %p. Ending.\n", position);
                return -3;
        }

        position = position + sectorsize;
        dbg("New positon is 0x%lx\n", position-dev);
    }
    return 0;
}

uint64_t uuid_decoder(uint64_t uuid) {
    uint64_t result = 0;
    dbg("UUID: 0x%x\n", uuid);
    for(int i=0; i<sizeof(uint64_t); i++) {
        result += ((uuid >> i*4) & 0xF) * pow(10,i);
        dbg("r: %d, mask: 0x%08x, power: %f\n", result, ((uuid >> i*4) & 0xF), pow(10,i));
    }
    return result;
}

/**
 * @brief Loads Logical Volume Integrity Descriptor (LVID) and stores it at struct udf_disc
 * @param[in] dev pointer to device array
 * @param[out] disc LVID is stored in udf_disc structure
 * @param[in] sectorsize device logical sector size
 * @return 0 everything ok
 *         -4 structure is already set
 */
int get_lvid(uint8_t *dev, struct udf_disc *disc, int sectorsize, struct filesystemStats *stats) {
    if(disc->udf_lvid != 0) {
        err("Structure LVID is already set. Probably error at tag or media\n");
        return -4;
    }
    uint32_t loc = disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLocation; //FIXME MAIN_VDS should be verified first
    uint32_t len = disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLength; //FIXME same as previous
    dbg("LVID: loc: %d, len: %d\n", loc, len);

    struct logicalVolIntegrityDesc *lvid;
    lvid = (struct logicalVolIntegrityDesc *)(dev+loc*sectorsize);

    disc->udf_lvid = malloc(len);
    memcpy(disc->udf_lvid, dev+loc*sectorsize, len);
    dbg("LVID: lenOfImpUse: %d\n",disc->udf_lvid->lengthOfImpUse);
    //printf("LVID: freeSpaceTable: %d\n", disc->udf_lvid->freeSpaceTable[0]);
    //printf("LVID: sizeTable: %d\n", disc->udf_lvid->sizeTable[0]);
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

    return 0; 
}

uint8_t markUsedBlock(struct filesystemStats *stats, uint32_t lbn, uint32_t size) {
    if(lbn+size < stats->partitionNumOfBits) {
        uint32_t byte = 0;
        uint8_t bit = 0;

        dbg("Marked LBN %d with size %d\n", lbn, size);
        int i = 0;
        do {
            byte = lbn/8;
            bit = lbn%8;
            stats->actPartitionBitmap[byte] &= ~(1<<bit);
            lbn++;
            i++;
        } while(i < size);
        dbg("Last LBN: %d, Byte: %d, Bit: %d\n", lbn, byte, bit);
        dbg("Real size: %d\n", i);
        
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

    } else {
        err("MARKING USED BLOCK TO BITMAP FAILED\n");
        return -1;
    }
    return 0;
}

/**
 * @brief Loads File Set Descriptor and stores it at struct udf_disc
 * @param[in] dev pointer to device array
 * @param[out] disc FSD is stored in udf_disc structure
 * @param[in] sectorsize device logical sector size
 * @param[out] lbnlsn LBN starting offset
 * @return 0 everything ok
 *         -1 TD not found
 */
uint8_t get_fsd(uint8_t *dev, struct udf_disc *disc, int sectorsize, uint32_t *lbnlsn, struct filesystemStats * stats) {
    long_ad *lap;
    tag descTag;
    lap = (long_ad *)disc->udf_lvd[0]->logicalVolContentsUse; //FIXME use lela_to_cpu, but not on ptr to disc. Must store it on different place.
    lb_addr filesetblock = lelb_to_cpu(lap->extLocation);
    uint32_t filesetlen = lap->extLength;

    dbg("FSD at (%d, p%d)\n", 
            lap->extLocation.logicalBlockNum,
            lap->extLocation.partitionReferenceNum);

    //FIXME some images doesn't work (Apple for example) but works when I put there 257 as lsnBase...
    //uint32_t lsnBase = le32_to_cpu(disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLocation)+1; //FIXME MAIN_VDS should be verified first
    uint32_t lsnBase = 0;
    if(lap->extLocation.partitionReferenceNum == disc->udf_pd[MAIN_VDS]->partitionNumber)
        lsnBase = disc->udf_pd[MAIN_VDS]->partitionStartingLocation;
    else {
        return -1;
    }

    dbg("LSN base: %d\n", lsnBase);


    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[MAIN_VDS]->logicalBlockSize); //FIXME same as above

    dbg("LAP: length: %x, LBN: %x, PRN: %x\n", filesetlen, filesetblock.logicalBlockNum, filesetblock.partitionReferenceNum);
    dbg("LAP: LSN: %d\n", lsnBase/*+filesetblock.logicalBlockNum*/);

    disc->udf_fsd = malloc(sizeof(struct fileSetDesc));
    memcpy(disc->udf_fsd, dev+(lsnBase+filesetblock.logicalBlockNum)*lbSize, sizeof(struct fileSetDesc));

    if(le16_to_cpu(disc->udf_fsd->descTag.tagIdent) != TAG_IDENT_FSD) {
        err("Error identifiing FSD. Tag ID: 0x%x\n", disc->udf_fsd->descTag.tagIdent);
        free(disc->udf_fsd);
        return -1;
    }
    dbg("LogicVolIdent: %s\nFileSetIdent: %s\n", (disc->udf_fsd->logicalVolIdent), (disc->udf_fsd->fileSetIdent));
    stats->logicalVolIdent = disc->udf_fsd->logicalVolIdent;

    /*struct spaceBitmapDesc sbd;
      uint32_t counter = 1;
      memcpy(&descTag, dev+(lsnBase+filesetblock.logicalBlockNum+counter)*lbSize, sizeof(tag));
      if(descTag.tagIdent == TAG_IDENT_SBD) {
      sbd = *(struct spaceBitmapDesc *)((lsnBase+filesetblock.logicalBlockNum+counter)*lbSize);
      counter++;
      }

    //FIXME Maybe not needed. Investigate. 
    memcpy(&descTag, dev+(lsnBase+filesetblock.logicalBlockNum+counter)*lbSize, sizeof(tag));
    if(le16_to_cpu(descTag.tagIdent) != TAG_IDENT_TD) {
    fprintf(stderr, "Error loading FSD sequence. TE descriptor not found. LSN: %d, Desc ID: %x\n", lsnBase+filesetblock.logicalBlockNum+1, le16_to_cpu(descTag.tagIdent));
    //        free(disc->udf_fsd);
    //   return -1;
    } else {
    counter++;
    }*/

    *lbnlsn = lsnBase;
    return 0;
}

uint8_t inspect_fid(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn, uint32_t lsn, uint8_t *base, uint32_t *pos, struct filesystemStats *stats, uint32_t depth, vds_sequence_t *seq, uint8_t *status) {
    uint32_t flen, padding;
    uint32_t lsnBase = lbnlsn; 
    struct fileIdentDesc *fid = (struct fileIdentDesc *)(base + *pos);
    struct fileInfo info = {0};

    if (!checksum(fid->descTag)) {
        err("[inspect fid] FID checksum failed.\n");
       // return -4;
        warn("DISABLED ERROR RETURN\n");
    }
    if (le16_to_cpu(fid->descTag.tagIdent) == TAG_IDENT_FID) {
        dwarn("FID found (%d)\n",*pos);
        flen = 38 + le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent;
        padding = 4 * ((le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38 + 3)/4) - (le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38);

        if(crc(fid, flen + padding)) {
            err("FID CRC failed.\n");
            return -5;
        }
        dbg("FID: ImpUseLen: %d\n", fid->lengthOfImpUse);
        dbg("FID: FilenameLen: %d\n", fid->lengthFileIdent);
        if(fid->lengthFileIdent == 0) {
            dbg("ROOT directory\n");
        } else {
            dbg("%sFilename: %s\n", depth2str(depth), fid->fileIdent);
            info.filename = (char *)fid->fileIdent+1;
        }

        dbg("FileVersionNum: %d\n", fid->fileVersionNum);

        /*
        if(fid->fileCharacteristics & FID_FILE_CHAR_DIRECTORY) {
            stats->countNumOfDirs ++;
            warn("DIR++\n");
        } else {
            stats->countNumOfFiles ++;
        }
*/
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
                uint32_t uuid = (fid->icb).impUse[2];
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
                        struct fileEntry *fe = (struct fileEntry *)(dev + (fid->descTag.tagLocation + lbnlsn) * stats->blocksize);
                        struct extendedFileEntry *efe = (struct extendedFileEntry *)(dev + (fid->descTag.tagLocation + lbnlsn) * stats->blocksize);
                        if(efe->descTag.tagIdent == TAG_IDENT_EFE) {
                            efe->descTag.descCRC = calculate_crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs));
                            efe->descTag.tagChecksum = calculate_checksum(efe->descTag);
                        } else {
                            fe->descTag.descCRC = calculate_crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs));
                            fe->descTag.tagChecksum = calculate_checksum(fe->descTag);
                        }
                        imp("(%s) UUID was fixed.\n", info.filename);
                        *status |= 1;
                    }
                }
                dbg("ICB to follow.\n");
                *status |= get_file(dev, disc, lbnlsn, (fid->icb).extLocation.logicalBlockNum + lsnBase, stats, depth, uuid, info, seq);
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

    return 0;
}

void print_file_chunks(struct filesystemStats *stats) {
    file_t *file;

    list_first(&stats->allocationTable);
    
    uint32_t last = 0, first = 0;
    do {
        file = list_get(&stats->allocationTable);
        if(file == NULL)
            break;
        first = file->lsn;
        last = file->lsn + file->blocks;
        dbg("Used space from: %d to %d, blocks: %d\n", first, last, last-first);
    } while(list_next(&stats->allocationTable) == 0);
}

void incrementUsedSize(struct filesystemStats *stats, uint32_t increment, uint32_t position) {
    stats->usedSpace += increment;
    dwarn("INCREMENT to %d (%d)\n", stats->usedSpace, stats->usedSpace/stats->blocksize);
    markUsedBlock(stats, position, increment/stats->blocksize);
    /*file_t *previous, *file = malloc(sizeof(file_t));
    
    previous = list_get(&stats->allocationTable);
    uint32_t prevBlocks = 0, prevLsn = 0;
    if(previous != NULL) {
        prevBlocks = previous->blocks;
        prevLsn = previous->lsn;
    }
    
    file->lsn = position;
    file->blocks = increment/stats->blocksize;
   
    if(file->blocks != prevBlocks) 
        list_insert_first(&stats->allocationTable, file);
        */
}

uint8_t get_file(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn, uint32_t lsn, struct filesystemStats *stats, uint32_t depth, uint32_t uuid, struct fileInfo info, vds_sequence_t *seq ) {
    tag descTag;
    struct fileIdentDesc *fid;
    struct fileEntry *fe;
    struct extendedFileEntry *efe;
    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[MAIN_VDS]->logicalBlockSize); //FIXME MAIN_VDS should be verified first 
    uint32_t lsnBase = lbnlsn; 
    uint32_t flen, padding;
    uint8_t dir = 0;
    uint8_t status = 0;

    dwarn("\n(%d) ---------------------------------------------------\n", lsn);

    descTag = *(tag *)(dev+lbSize*lsn);
    if(!checksum(descTag)) {
        err("Tag checksum failed. Unable to continue.\n");
        return 4;
    }
    //memcpy(&descTag, dev+lbSize*lsn, sizeof(tag));
    //do {    
    //read(fd, file, sizeof(struct fileEntry));

    dbg("global FE increment.\n");
    dbg("usedSpace: %d\n", stats->usedSpace);
    incrementUsedSize(stats, lbSize, lsn-lbnlsn);
    dbg("usedSpace: %d\n", stats->usedSpace);
    switch(le16_to_cpu(descTag.tagIdent)) {
        case TAG_IDENT_SBD:
            dwarn("SBD found.\n");
            //FIXME Used for examination of used sectors
            status |= get_file(dev, disc, lbnlsn, lsn+1, stats, depth, uuid, info, seq); 
            break;
        case TAG_IDENT_EAHD:
            dwarn("EAHD found.\n");
            //FIXME WTF is that?
            status |= get_file(dev, disc, lbnlsn, lsn+1, stats, depth, uuid, info, seq); 
        case TAG_IDENT_FID:
            fatal("Never should get there.\n");
            exit(8);
        case TAG_IDENT_AED:
            dbg("\nAED, LSN: %d\n", lsn);
            break;
        case TAG_IDENT_FE:
        case TAG_IDENT_EFE:
            dir = 0;
            fe = (struct fileEntry *)(dev+lbSize*lsn);
            efe = (struct extendedFileEntry *)fe;
            uint8_t ext = 0;

            if(le16_to_cpu(descTag.tagIdent) == TAG_IDENT_EFE) {
                dwarn("[EFE]\n");
                if(crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs))) {
                    err("EFE CRC failed.\n");
                    //TODO add question about "Continue with caution. yes?"
                    return 4;
                }
                ext = 1;
            } else {
                if(crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs))) {
                    err("FE CRC failed.\n");
                    //TODO add question about "Continue with caution. yes?"
                    return 4;
                }
            }
            dbg("\nFE, LSN: %d, EntityID: %s ", lsn, fe->impIdent.ident);
            dbg("fileLinkCount: %d, LB recorded: %lu\n", fe->fileLinkCount, ext ? efe->logicalBlocksRecorded: fe->logicalBlocksRecorded);
            uint32_t lea = ext ? efe->lengthExtendedAttr : fe->lengthExtendedAttr;
            uint32_t lad =  ext ? efe->lengthAllocDescs : fe->lengthAllocDescs;
            dbg("LEA %d, LAD %d\n", lea, lad);
            
            info.fileType = fe->icbTag.fileType;
            info.permissions = fe->permissions;
            
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
                    //incrementUsedSize(stats, lbSize);
                    dir = 1;
                    break;  
                case ICBTAG_FILE_TYPE_REGULAR:
                    dbg("Filetype: REGULAR\n");
                    stats->countNumOfFiles ++;
//                    stats->usedSpace += lbSize;
                    break;  
                case ICBTAG_FILE_TYPE_BLOCK:
                    dbg("Filetype: BLOCK\n");
                    break;  
                case ICBTAG_FILE_TYPE_CHAR:
                    dbg("Filetype: CHAR\n");
                    break;  
                case ICBTAG_FILE_TYPE_EA:
                    dbg("Filetype: EA\n");
                    break;  
                case ICBTAG_FILE_TYPE_FIFO:
                    dbg("Filetype: FIFO\n");
                    break;  
                case ICBTAG_FILE_TYPE_SOCKET:
                    dbg("Filetype: SOCKET\n");
                    break;  
                case ICBTAG_FILE_TYPE_TE:
                    dbg("Filetype: TE\n");
                    break;  
                case ICBTAG_FILE_TYPE_SYMLINK:
                    dbg("Filetype: SYMLINK\n");
                    break;  
                case ICBTAG_FILE_TYPE_STREAMDIR:
                    dbg("Filetype: STRAMDIR\n");
                    //stats->usedSpace += lbSize;
                    break; 
                default:
                    dbg("Unknown filetype\n");
                    break; 
            }

            if(compare_timestamps(stats->LVIDtimestamp, ext ? efe->modificationTime : fe->modificationTime) < 0) {
                err("(%s) File timestamp is later than LVID timestamp. LVID need to be fixed.\n", info.filename);
                seq->lvid.error |= E_TIMESTAMP; 
            }
            info.modTime = ext ? efe->modificationTime : fe->modificationTime;


            uint64_t feUUID = (ext ? efe->uniqueID : fe->uniqueID);
            dbg("Unique ID: %d\n", (feUUID));
            //(stats->maxUUID < uuid) {
            //  stats->maxUUID = uuid;
            //  dwarn("New MAX UUID\n");
            //
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


            print_file_info(info, depth);

            uint8_t *allocDescs = (ext ? efe->allocDescs : fe->allocDescs) + lea; 
            if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT) {
                dbg("SHORT\n");
                short_ad *sad = (short_ad *)(allocDescs);
                dbg("ExtLen: %d, ExtLoc: %d\n", sad->extLength/lbSize, sad->extPosition+lsnBase);
                lsn = lsn + sad->extLength/lbSize;
                dbg("LSN: %d, ExtLocOrig: %d\n", lsn, sad->extPosition);
                
                dbg("usedSpace: %d\n", stats->usedSpace);
                uint32_t usedsize = (fe->informationLength%lbSize == 0 ? fe->informationLength : (fe->informationLength + lbSize - fe->informationLength%lbSize));
                if(dir == 0)
                    incrementUsedSize(stats, usedsize, sad->extPosition);
                dbg("usedSpace: %d\n", stats->usedSpace);
                dwarn("Size: %d, Blocks: %d\n", usedsize, usedsize/lbSize);
            } else if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG) {
                dbg("LONG\n");
                long_ad *lad = (long_ad *)(allocDescs);
                dbg("ExtLen: %d, ExtLoc: %d\n", lad->extLength/lbSize, lad->extLocation.logicalBlockNum+lsnBase);
                lsn = lsn + lad->extLength/lbSize;
                dbg("LSN: %d\n", lsn);
                
                dbg("usedSpace: %d\n", stats->usedSpace);
                uint32_t usedsize = (fe->informationLength%lbSize == 0 ? fe->informationLength : (fe->informationLength + lbSize - fe->informationLength%lbSize));
                if(dir == 0)
                    incrementUsedSize(stats, usedsize, lad->extLocation.logicalBlockNum);
                dbg("usedSpace: %d\n", stats->usedSpace);
                dwarn("Size: %d, Blocks: %d\n", usedsize, usedsize/lbSize);
            } else if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_EXTENDED) {
                err("Extended ICB in FE.\n");
            } else if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB) {
                
               /* dbg("usedSpace: %d\n", stats->usedSpace);
                uint32_t usedsize = (fe->informationLength%lbSize == 0 ? fe->informationLength : (fe->informationLength + lbSize - fe->informationLength%lbSize));
                if(dir == 0)
                    incrementUsedSize(stats, usedsize, lsn-lbnlsn+1);
                dbg("usedSpace: %d\n", stats->usedSpace);
                dwarn("Size: %d, Blocks: %d\n", usedsize, usedsize/lbSize);
                */
                dbg("AD in ICB\n");
                //stats->usedSpace -= lbSize;
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
                    dbg("efe: %p, POS: %d, descTag: %p\n",efe,  sizeof(struct extendedFileEntry) + efe->lengthExtendedAttr, descTag);
                } else {    
                    eahd = *(struct extendedAttrHeaderDesc *)(fe + sizeof(struct fileEntry) + fe->lengthExtendedAttr);
                    descTag = (tag *)((uint8_t *)(fe) + sizeof(struct fileEntry) + fe->lengthExtendedAttr); 
                    dbg("fe: %p, POS: %d, descTag: %p\n", fe, sizeof(struct fileEntry) + fe->lengthExtendedAttr, descTag);
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

                            for(uint32_t pos=0; ; ) {
                                if(inspect_fid(dev, disc, lbnlsn, lsn, base, &pos, stats, depth, seq, &status) != 0) {
                                    imp("FID inspection over\n");
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


            //TODO is it directory? If is, continue. Otherwise not.
            // We can assume that directory have one or more FID inside.
            // FE have inside long_ad/short_ad.
            if(dir) {
                /*for(int i=0; i<le32_to_cpu(fe->lengthAllocDescs); i+=8) {
                    for(int j=0; j<8; j++)
                        printf("%02x ", fe->allocDescs[i+j]);

                    printf("\n");
                }*/
                //printf("\n");
                if(ext) {
                    dbg("[EFE DIR] lengthExtendedAttr: %d\n", efe->lengthExtendedAttr);
                    for(uint32_t pos=0; pos < efe->lengthAllocDescs; ) {
                        if(inspect_fid(dev, disc, lbnlsn, lsn, efe->allocDescs + efe->lengthExtendedAttr, &pos, stats, depth+1, seq, &status) != 0) {
                            break;
                        }
                    }
                } else {
                    dbg("[FE DIR] lengthExtendedAttr: %d\n", fe->lengthExtendedAttr);
                    for(uint32_t pos=0; pos < fe->lengthAllocDescs; ) {
                        if(inspect_fid(dev, disc, lbnlsn, lsn, fe->allocDescs + fe->lengthExtendedAttr, &pos, stats, depth+1, seq, &status) != 0) {
                            break;
                        }
                    }
                }
            }
            break;  
        default:
            err("IDENT: %x, LSN: %d, addr: 0x%x\n", descTag.tagIdent, lsn, lsn*lbSize);
    }            
    return status;
}

uint8_t get_file_structure(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn, struct filesystemStats *stats, vds_sequence_t *seq ) {
    struct fileEntry *file;
    struct fileIdentDesc *fid;
    tag descTag;
    uint32_t lsn;

    uint8_t ptLength = 1;
    uint32_t extLoc;
    char *filename;
    uint16_t pos = 0;
    uint32_t lsnBase = lbnlsn; 
    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[MAIN_VDS]->logicalBlockSize); //FIXME MAIN_VDS should be verified first 
    // Go to ROOT ICB 
    lb_addr icbloc = lelb_to_cpu(disc->udf_fsd->rootDirectoryICB.extLocation); 

    //file = malloc(sizeof(struct fileEntry));
    //lseek64(fd, blocksize*(257+icbloc.logicalBlockNum), SEEK_SET);
    //read(fd, file, sizeof(struct fileEntry));
    lsn = icbloc.logicalBlockNum+lsnBase;
    dbg("ROOT LSN: %d\n", lsn);
    stats->usedSpace = (lsn-lsnBase)*le32_to_cpu(disc->udf_lvd[MAIN_VDS]->logicalBlockSize); //FIXME MAIN_VDS should be verified first
//uint8_t markUsedBlock(struct filesystemStats *stats, uint32_t lbn, uint32_t size) {
    markUsedBlock(stats, 0, lsn-lsnBase);
    dbg("Used space offset: %d\n", stats->usedSpace);
    //memcpy(file, dev+lbSize*lsn, sizeof(struct fileEntry));
    struct fileInfo info = {0};
    
    msg("\nMedium file tree\n----------------\n");
    return get_file(dev, disc, lbnlsn, lsn, stats, 0, 0, info, seq);
}

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

int verify_vds(struct udf_disc *disc, vds_sequence_t *map, vds_type_e vds, vds_sequence_t *seq) {
    //metadata_err_map_t map;    
    uint8_t *data;
    //uint16_t crc = 0;
    uint16_t offset = sizeof(tag);

    if(!checksum(disc->udf_pvd[vds]->descTag)) {
        err("Checksum failure at PVD[%d]\n", vds);
        //map->pvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_PVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_lvd[vds]->descTag)) {
        err("Checksum failure at LVD[%d]\n", vds);
        //map->lvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_LVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_pd[vds]->descTag)) {
        err("Checksum failure at PD[%d]\n", vds);
        //map->pd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_PD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_usd[vds]->descTag)) {
        err("Checksum failure at USD[%d]\n", vds);
        //map->usd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_USD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_iuvd[vds]->descTag)) {
        err("Checksum failure at IUVD[%d]\n", vds);
        //map->iuvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_IUVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_td[vds]->descTag)) {
        err("Checksum failure at TD[%d]\n", vds);
        //map->td[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_TD, vds, E_CHECKSUM);
    }

    if(check_position(disc->udf_pvd[vds]->descTag, get_tag_location(seq, TAG_IDENT_PVD, vds))) {
        err("Position failure at PVD[%d]\n", vds);
        //map->pvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_PVD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_lvd[vds]->descTag, get_tag_location(seq, TAG_IDENT_LVD, vds))) {
        err("Position failure at LVD[%d]\n", vds);
        //map->lvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_LVD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_pd[vds]->descTag, get_tag_location(seq, TAG_IDENT_PD, vds))) {
        err("Position failure at PD[%d]\n", vds);
        //map->pd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_PD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_usd[vds]->descTag, get_tag_location(seq, TAG_IDENT_USD, vds))) {
        err("Position failure at USD[%d]\n", vds);
        //map->usd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_USD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_iuvd[vds]->descTag, get_tag_location(seq, TAG_IDENT_IUVD, vds))) {
        err("Position failure at IUVD[%d]\n", vds);
        //map->iuvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_IUVD, vds, E_POSITION);
    }   
    if(check_position(disc->udf_td[vds]->descTag, get_tag_location(seq, TAG_IDENT_TD, vds))) {
        err("Position failure at TD[%d]\n", vds);
        //map->td[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_TD, vds, E_POSITION);
    }

    if(crc(disc->udf_pvd[vds], sizeof(struct primaryVolDesc))) {
        err("CRC error at PVD[%d]\n", vds);
        //map->pvd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_PVD, vds, E_CRC);
    }
    if(crc(disc->udf_lvd[vds], sizeof(struct logicalVolDesc)+disc->udf_lvd[vds]->mapTableLength)) {
        err("CRC error at LVD[%d]\n", vds);
        //map->lvd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_LVD, vds, E_CRC);
    }
    if(crc(disc->udf_pd[vds], sizeof(struct partitionDesc))) {
        err("CRC error at PD[%d]\n", vds);
        //map->pd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_PD, vds, E_CRC);
    }
    if(crc(disc->udf_usd[vds], sizeof(struct unallocSpaceDesc)+(disc->udf_usd[vds]->numAllocDescs)*sizeof(extent_ad))) {
        err("CRC error at USD[%d]\n", vds);
        //map->usd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_USD, vds, E_CRC);
    }
    if(crc(disc->udf_iuvd[vds], sizeof(struct impUseVolDesc))) {
        err("CRC error at IUVD[%d]\n", vds);
        //map->iuvd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_IUVD, vds, E_CRC);
    }
    if(crc(disc->udf_td[vds], sizeof(struct terminatingDesc))) {
        err("CRC error at TD[%d]\n", vds);
        //map->td[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_TD, vds, E_CRC);
    }

    return 0;
}

int copy_descriptor(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, uint32_t sourcePosition, uint32_t destinationPosition, size_t amount) {
    tag sourceDescTag, destinationDescTag;
    uint8_t *destArray;

    dbg("source: 0x%x, destination: 0x%x\n", sourcePosition, destinationPosition);

    sourceDescTag = *(tag *)(dev+sourcePosition*sectorsize);
    memcpy(&destinationDescTag, &sourceDescTag, sizeof(tag));
    destinationDescTag.tagLocation = destinationPosition;
    destinationDescTag.tagChecksum = calculate_checksum(destinationDescTag);

    dbg("srcChecksum: 0x%x, destChecksum: 0x%x\n", sourceDescTag.tagChecksum, destinationDescTag.tagChecksum);

    destArray = calloc(1, amount);
    memcpy(destArray, &destinationDescTag, sizeof(tag));
    memcpy(destArray+sizeof(tag), dev+sourcePosition*sectorsize+sizeof(tag), amount-sizeof(tag));

    memcpy(dev+destinationPosition*sectorsize, destArray, amount);

    free(destArray);

    return 0;
}

int write_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize,  avdp_type_e source, avdp_type_e target) {
    uint64_t sourcePosition = 0;
    uint64_t targetPosition = 0;
    tag desc_tag;
    avdp_type_e type = target;

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

    //uint8_t * ptr = memcpy(dev+position, disc->udf_anchor[source], sizeof(struct anchorVolDescPtr)); 
    //printf("ptr: %p\n", ptr);

    copy_descriptor(dev, disc, sectorsize, sourcePosition/sectorsize, targetPosition/sectorsize, sizeof(struct anchorVolDescPtr));

    free(disc->udf_anchor[type]);
    disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP

    desc_tag = *(tag *)(dev+targetPosition);

    if(!checksum(desc_tag)) {
        err("Checksum failure at AVDP[%d]\n", type);
        return -2;
    } else if(le16_to_cpu(desc_tag.tagIdent) != TAG_IDENT_AVDP) {
        err("AVDP not found at 0x%lx\n", targetPosition);
        return -4;
    }

    memcpy(disc->udf_anchor[type], dev+targetPosition, sizeof(struct anchorVolDescPtr));

    if(crc(disc->udf_anchor[type], sizeof(struct anchorVolDescPtr))) {
        err("CRC error at AVDP[%d]\n", type);
        return -3;
    }

    imp("AVDP[%d] successfully written.\n", type);
    return 0;
}

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

int fix_vds(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, avdp_type_e source, vds_sequence_t *seq, uint8_t interactive, uint8_t autofix) { 
    uint32_t position_main, position_reserve;
    int8_t counter = 0;
    tag descTag;
    uint8_t fix=0;

    // Go to first address of VDS
    position_main = (disc->udf_anchor[source]->mainVolDescSeqExt.extLocation);
    position_reserve = (disc->udf_anchor[source]->reserveVolDescSeqExt.extLocation);


    msg("\nVDS verification status\n-----------------------\n");

    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        if(seq->main[i].error != 0 && seq->reserve[i].error != 0) {
            //Both descriptors are broken
            //FIXME Deal with it somehow   
            err("[%d] Both descriptors are broken.\n",i);     
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
                copy_descriptor(dev, disc, sectorsize, position_reserve + i, position_main + i, sectorsize);
            } else {
                warn("[%i] %s is broken.\n", i,descriptor_name(seq->reserve[i].tagIdent));
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
                //memcpy(position_reserve + i*sectorsize, position_main + i*sectorsize, sectorsize);
                copy_descriptor(dev, disc, sectorsize, position_reserve + i, position_main + i, sectorsize);
            } else {
                warn("[%i] %s is broken.\n", i,descriptor_name(seq->main[i].tagIdent));
            }
            fix = 0;
        } else {
            msg("[%d] %s is fine. No fixing needed.\n", i, descriptor_name(seq->main[i].tagIdent));
        }
        if(seq->main[i].tagIdent == TAG_IDENT_TD)
            break;
    }


    return 0;
}

static const unsigned char BitsSetTable256[256] = 
{
#define B2(n) n,     n+1,     n+1,     n+2
#define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
        B6(0), B6(1), B6(1), B6(2)
};

int fix_pd(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, struct filesystemStats *stats) {
    //TODO complete bitmap correction
     
    struct partitionHeaderDesc *phd = (struct partitionHeaderDesc *)(disc->udf_pd[MAIN_VDS]->partitionContentsUse);
    dbg("[USD] UST pos: %d, len: %d\n", phd->unallocSpaceTable.extPosition, phd->unallocSpaceTable.extLength);
    dbg("[USD] USB pos: %d, len: %d\n", phd->unallocSpaceBitmap.extPosition, phd->unallocSpaceBitmap.extLength);
    dbg("[USD] FST pos: %d, len: %d\n", phd->freedSpaceTable.extPosition, phd->freedSpaceTable.extLength);
    dbg("[USD] FSB pos: %d, len: %d\n", phd->freedSpaceBitmap.extPosition, phd->freedSpaceBitmap.extLength);

    //TODO Only USB is handled now. 
    if(phd->unallocSpaceBitmap.extLength > 3) { //0,1,2,3 are special values ECMA 167r3 4/14.14.1.1
        uint32_t lsnBase = disc->udf_pd[MAIN_VDS]->partitionStartingLocation;       
        struct spaceBitmapDesc *sbd = (struct spaceBitmapDesc *)(dev + (lsnBase + phd->unallocSpaceBitmap.extPosition)*sectorsize);
        if(sbd->descTag.tagIdent != TAG_IDENT_SBD) {
            err("SBD not found\n");
            return -1;
        }
        dbg("[SBD] NumOfBits: %d\n", sbd->numOfBits);
        dbg("[SBD] NumOfBytes: %d\n", sbd->numOfBytes);
        
        dbg("Bitmap: %d, %p\n", (lsnBase + phd->unallocSpaceBitmap.extPosition), sbd->bitmap);
        memcpy(sbd->bitmap, stats->actPartitionBitmap, sbd->numOfBytes);
        dbg("MEMCPY DONE\n");
            
        //Recalculate CRC and checksum
        sbd->descTag.descCRC = calculate_crc(sbd, sizeof(struct spaceBitmapDesc));
        sbd->descTag.tagChecksum = calculate_checksum(sbd->descTag);
        imp("PD SBD recovery was successful.\n");
        return 0;
    }
    err("PD SBD recovery failed.\n");
    return 1; 
}

int get_pd(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, struct filesystemStats *stats, vds_sequence_t *seq) {
    struct partitionHeaderDesc *phd = (struct partitionHeaderDesc *)(disc->udf_pd[MAIN_VDS]->partitionContentsUse);
    dbg("[USD] UST pos: %d, len: %d\n", phd->unallocSpaceTable.extPosition, phd->unallocSpaceTable.extLength);
    dbg("[USD] USB pos: %d, len: %d\n", phd->unallocSpaceBitmap.extPosition, phd->unallocSpaceBitmap.extLength);
    dbg("[USD] FST pos: %d, len: %d\n", phd->freedSpaceTable.extPosition, phd->freedSpaceTable.extLength);
    dbg("[USD] FSB pos: %d, len: %d\n", phd->freedSpaceBitmap.extPosition, phd->freedSpaceBitmap.extLength);

    //TODO Only USB is handled now. 
    if(phd->unallocSpaceBitmap.extLength > 3) { //0,1,2,3 are special values ECMA 167r3 4/14.14.1.1
        uint32_t lsnBase = disc->udf_pd[MAIN_VDS]->partitionStartingLocation;      
        dbg("LSNBase: %d\n", lsnBase); 
        struct spaceBitmapDesc *sbd = (struct spaceBitmapDesc *)(dev + (lsnBase + phd->unallocSpaceBitmap.extPosition)*sectorsize);
        if(sbd->descTag.tagIdent != TAG_IDENT_SBD) {
            err("SBD not found\n");
            return -1;
        }
        if(!checksum(sbd->descTag)) {
            err("SBD checksum error. Continue with caution.\n");
            seq->pd.error |= E_CHECKSUM;
        }
        if(crc(sbd, sizeof(struct spaceBitmapDesc))) {
            err("SBD CRC error. Continue with caution.\n");
            seq->pd.error |= E_CRC; 
        }
        dbg("SBD is ok\n");
        dbg("[SBD] NumOfBits: %d\n", sbd->numOfBits);
        dbg("[SBD] NumOfBytes: %d\n", sbd->numOfBytes);
        dbg("Bitmap: %d, %p\n", (lsnBase + phd->unallocSpaceBitmap.extPosition), sbd->bitmap);

        //Create array for used/unused blocks counting
        stats->actPartitionBitmap = calloc(sbd->numOfBytes, 1);
        memset(stats->actPartitionBitmap, 0xff, sbd->numOfBytes);
        stats->partitionNumOfBytes = sbd->numOfBytes;
        stats->partitionNumOfBits = sbd->numOfBits;

        //Get actual bitmap statistics
        uint32_t usedBlocks = 0;
        uint32_t unusedBlocks = 0;
        uint8_t count = 0;
        uint8_t v = 0;
        for(int i=0; i<sbd->numOfBytes-1; i++) {
                v = sbd->bitmap[i];
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
        
        
        //dbg("Total Count: %d\n", totalcount);
        //usedBlocks -= ((usedBlocks + unusedBlocks)/8 - sbd->numOfBytes)*8;
        //unusedBlocks -= bitCorrection;
        stats->expUsedBlocks = usedBlocks;
        stats->expUnusedBlocks = unusedBlocks;
        stats->expPartitionBitmap = sbd->bitmap;
        //dbg("Total Count: %d\n", totalcount);
        dbg("Unused blocks: %d\n", unusedBlocks);
        dbg("Used Blocks: %d\n", usedBlocks);
        return 0;
    }
    return 1; 
}

int fix_lvid(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, struct filesystemStats *stats) {
    uint32_t loc = disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLocation; //FIXME MAIN_VDS should be verified first
    uint32_t len = disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLength; //FIXME same as previous
    uint16_t size = sizeof(struct logicalVolIntegrityDesc) + disc->udf_lvid->numOfPartitions*4*2 + disc->udf_lvid->lengthOfImpUse;
    dbg("LVID: loc: %d, len: %d, size: %d\n", loc, len, size);

    struct logicalVolIntegrityDesc *lvid = (struct logicalVolIntegrityDesc *)(dev+loc*sectorsize);
    struct impUseLVID *impUse = (struct impUseLVID *)((uint8_t *)(disc->udf_lvid) + sizeof(struct logicalVolIntegrityDesc) + 8*disc->udf_lvid->numOfPartitions); //this is because of ECMA 167r3, 3/24, fig 22
    
    // Fix PD too
    fix_pd(dev, disc, sectorsize, stats);
    
    // Fix files/dir amounts
    impUse->numOfFiles = stats->countNumOfFiles;
    impUse->numOfDirs = stats->countNumOfDirs;
    
    // Fix Next Unique ID by maximal found +1
    ((struct logicalVolHeaderDesc *)(disc->udf_lvid->logicalVolContentsUse))->uniqueID = stats->maxUUID+1;

    // Set recording date and time to now. 
    time_t t = time(NULL);
    struct tm tm = *gmtime(&t);
    timestamp *ts = &(disc->udf_lvid->recordingDateAndTime);
    ts->year = tm.tm_year + 1900;
    ts->month = tm.tm_mon + 1;
    ts->day = tm.tm_mday;
    ts->hour = tm.tm_hour;
    ts->minute = tm.tm_min;
    ts->second = tm.tm_sec;
    ts->centiseconds = 0;
    ts->hundredsOfMicroseconds = 0;
    ts->microseconds = 0;

    //int32_t usedSpaceDiff = stats->expUsedBlocks - stats->usedSpace/sectorsize;
    //dbg("Diff: %d\n", usedSpaceDiff);
    //dbg("Old Free Space: %d\n", disc->udf_lvid->freeSpaceTable[0]);
    //uint32_t newFreeSpace = disc->udf_lvid->freeSpaceTable[0] + usedSpaceDiff;
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

    imp("LVID recovery was successful.\n");
    return 0;
}

void test_list(void) {
    list_t list;

    uint8_t a = 5, b = 7, c = 10;
    uint8_t * d;

    list_init(&list);

    dbg("a: %p\n", &a);
    list_insert_first(&list, &a);
    dbg("b: %p\n", &b);
    list_insert_first(&list, &b);
    dbg("c: %p\n", &c);
    list_insert_first(&list, &c);

    d = list_get(&list);
    dbg("Actual: %p, %d\n", d, *d );
    list_next(&list);
    dbg("Go get\n");
    d = list_get(&list);
    dbg("Actual: %p, %d\n", d, *d );
    list_next(&list);
    dbg("Go get\n");
    d = list_get(&list);
    dbg("Actual: %p, %d\n", d, *d );
    list_next(&list);
    dbg("Go get\n");
    d = list_get(&list);
    dbg("Actual: %p\n", d);


    list_destoy(&list);

}
