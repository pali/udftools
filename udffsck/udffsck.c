
#include "udffsck.h"
#include "utils.h"
#include "libudffs.h"

uint8_t calculate_checksum(tag descTag) {
    uint8_t i;
    uint8_t tagChecksum = 0;

    for (i=0; i<16; i++)
        if (i != 4)
            tagChecksum += (uint8_t)(((char *)&(descTag))[i]);

    return tagChecksum;
}

int checksum(tag descTag) {
    return calculate_checksum(descTag) == descTag.tagChecksum;
}

int crc(void * restrict desc, uint16_t size) {
    uint8_t offset = sizeof(tag);
    tag *descTag = desc;
    uint16_t crc = 0;
    uint16_t calcCrc = udf_crc((uint8_t *)(desc) + offset, size - offset, crc);
    printf("Calc CRC: 0x%04x, TagCRC: 0x%04x\n", calcCrc, descTag->descCRC);
    return le16_to_cpu(descTag->descCRC) != calcCrc;
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

    printf("DevSize: %zu\n", devsize);
    printf("Current position: %lx\n", position);

    disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP

    desc_tag = *(tag *)(dev+position);

    if(!checksum(desc_tag)) {
        fprintf(stderr, "Checksum failure at AVDP[%d]\n", type);
        return -2;
    } else if(le16_to_cpu(desc_tag.tagIdent) != TAG_IDENT_AVDP) {
        fprintf(stderr, "AVDP not found at 0x%lx\n", position);
        return -4;
    }

    memcpy(disc->udf_anchor[type], dev+position, sizeof(struct anchorVolDescPtr));

    if(crc(disc->udf_anchor[type], sizeof(struct anchorVolDescPtr))) {
        printf("CRC error at AVDP[%d]\n", type);
        return -3;
    }

    printf("AVDP[%d] successfully loaded.\n", type);
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
    printf("Current position: %lx\n", position-dev);

    // Go thru descriptors until TagIdent is 0 or amout is too big to be real
    while(counter < VDS_STRUCT_AMOUNT) {

        // Read tag
        memcpy(&descTag, position, sizeof(descTag));

        printf("Tag ID: %d\n", descTag.tagIdent);

        if(vds == MAIN_VDS) {
            seq->main[counter].tagIdent = descTag.tagIdent;
        } else {
            seq->reserve[counter].tagIdent = descTag.tagIdent;
        }

        counter++;

        // What kind of descriptor is that?
        switch(le16_to_cpu(descTag.tagIdent)) {
            case TAG_IDENT_PVD:
                if(disc->udf_pvd[vds] != 0) {
                    fprintf(stderr, "Structure PVD is already set. Probably error at tag or media\n");
                    return -4;
                }
                disc->udf_pvd[vds] = malloc(sizeof(struct primaryVolDesc)); // Prepare memory
                memcpy(disc->udf_pvd[vds], position, sizeof(struct primaryVolDesc)); 
                printf("VolNum: %d\n", disc->udf_pvd[vds]->volDescSeqNum);
                printf("pVolNum: %d\n", disc->udf_pvd[vds]->primaryVolDescNum);
                printf("seqNum: %d\n", disc->udf_pvd[vds]->volSeqNum);
                printf("predLoc: %d\n", disc->udf_pvd[vds]->predecessorVolDescSeqLocation);
                break;
            case TAG_IDENT_IUVD:
                if(disc->udf_iuvd[vds] != 0) {
                    fprintf(stderr, "Structure IUVD is already set. Probably error at tag or media\n");
                    return -4;
                }
                disc->udf_iuvd[vds] = malloc(sizeof(struct impUseVolDesc)); // Prepare memory
                memcpy(disc->udf_iuvd[vds], position, sizeof(struct impUseVolDesc)); 
                break;
            case TAG_IDENT_PD:
                if(disc->udf_pd[vds] != 0) {
                    fprintf(stderr, "Structure PD is already set. Probably error at tag or media\n");
                    return -4;
                }
                disc->udf_pd[vds] = malloc(sizeof(struct partitionDesc)); // Prepare memory
                memcpy(disc->udf_pd[vds], position, sizeof(struct partitionDesc)); 
                break;
            case TAG_IDENT_LVD:
                if(disc->udf_lvd[vds] != 0) {
                    fprintf(stderr, "Structure LVD is already set. Probably error at tag or media\n");
                    return -4;
                }
                printf("LVD size: 0x%lx\n", sizeof(struct logicalVolDesc));

                struct logicalVolDesc *lvd;
                lvd = (struct logicalVolDesc *)(position);

                disc->udf_lvd[vds] = malloc(sizeof(struct logicalVolDesc)+lvd->mapTableLength); // Prepare memory
                memcpy(disc->udf_lvd[vds], position, sizeof(struct logicalVolDesc)+lvd->mapTableLength);
                printf("NumOfPartitionMaps: %d\n", disc->udf_lvd[vds]->numPartitionMaps);
                printf("MapTableLength: %d\n", disc->udf_lvd[vds]->mapTableLength);
                for(int i=0; i<le32_to_cpu(lvd->mapTableLength); i++) {
                    printf("[0x%02x] ", disc->udf_lvd[vds]->partitionMaps[i]);
                }
                printf("\n");
                break;
            case TAG_IDENT_USD:
                if(disc->udf_usd[vds] != 0) {
                    fprintf(stderr, "Structure USD is already set. Probably error at tag or media\n");
                    return -4;
                }

                struct unallocSpaceDesc *usd;
                usd = (struct unallocSpaceDesc *)(position);
                printf("VolDescNum: %d\n", usd->volDescSeqNum);
                printf("NumAllocDesc: %d\n", usd->numAllocDescs);

                disc->udf_usd[vds] = malloc(sizeof(struct unallocSpaceDesc)+(usd->numAllocDescs)*sizeof(extent_ad)); // Prepare memory
                memcpy(disc->udf_usd[vds], position, sizeof(struct unallocSpaceDesc)+(usd->numAllocDescs)*sizeof(extent_ad)); 
                break;
            case TAG_IDENT_TD:
                if(disc->udf_td[vds] != 0) {
                    fprintf(stderr, "Structure TD is already set. Probably error at tag or media\n");
                    return -4;
                }
                disc->udf_td[vds] = malloc(sizeof(struct terminatingDesc)); // Prepare memory
                memcpy(disc->udf_td[vds], position, sizeof(struct terminatingDesc)); 
                break;
            case 0:
                // Found end of VDS, ending.
                return 0;
            default:
                // Unkown TAG
                fprintf(stderr, "Unknown TAG found at %p. Ending.\n", position);
                return -3;
        }

        position = position + sectorsize;
        printf("New positon is 0x%lx\n", position-dev);
    }
    return 0;
}

/**
 * @brief Loads Logical Volume Integrity Descriptor (LVID) and stores it at struct udf_disc
 * @param[in] dev pointer to device array
 * @param[out] disc LVID is stored in udf_disc structure
 * @param[in] sectorsize device logical sector size
 * @return 0 everything ok
 *         -4 structure is already set
 */
int get_lvid(uint8_t *dev, struct udf_disc *disc, int sectorsize) {
    if(disc->udf_lvid != 0) {
        fprintf(stderr, "Structure LVID is already set. Probably error at tag or media\n");
        return -4;
    }
    uint32_t loc = disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLocation; //FIXME MAIN_VDS should be verified first
    uint32_t len = disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLength; //FIXME same as previous
    printf("LVID: loc: %d, len: %d\n", loc, len);

    struct logicalVolIntegrityDesc *lvid;
    lvid = (struct logicalVolIntegrityDesc *)(dev+loc*sectorsize);

    disc->udf_lvid = malloc(len);
    memcpy(disc->udf_lvid, dev+loc*sectorsize, len);
    printf("LVID: lenOfImpUse: %d\n",disc->udf_lvid->lengthOfImpUse);
    printf("LVID: freeSpaceTable: %d\n", disc->udf_lvid->freeSpaceTable[0]);
    printf("LVID: sizeTable: %d\n", disc->udf_lvid->sizeTable[0]);

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
uint8_t get_fsd(uint8_t *dev, struct udf_disc *disc, int sectorsize, uint32_t *lbnlsn) {
    long_ad *lap;
    tag descTag;
    lap = (long_ad *)disc->udf_lvd[0]->logicalVolContentsUse; //FIXME use lela_to_cpu, but not on ptr to disc. Must store it on different place.
    lb_addr filesetblock = lelb_to_cpu(lap->extLocation);
    uint32_t filesetlen = lap->extLength;


    //FIXME some images doesn't work (Apple for example) but works when I put there 257 as lsnBase...
    uint32_t lsnBase = le32_to_cpu(disc->udf_lvd[MAIN_VDS]->integritySeqExt.extLocation)+1; //FIXME MAIN_VDS should be verified first
    //uint32_t lsnBase = 256+1;



    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[MAIN_VDS]->logicalBlockSize); //FIXME same as above

    printf("LAP: length: %x, LBN: %x, PRN: %x\n", filesetlen, filesetblock.logicalBlockNum, filesetblock.partitionReferenceNum);
    printf("LAP: LSN: %d\n", lsnBase/*+filesetblock.logicalBlockNum*/);

    disc->udf_fsd = malloc(sizeof(struct fileSetDesc));
    memcpy(disc->udf_fsd, dev+(lsnBase+filesetblock.logicalBlockNum)*lbSize, sizeof(struct fileSetDesc));

    if(le16_to_cpu(disc->udf_fsd->descTag.tagIdent) != TAG_IDENT_FSD) {
        fprintf(stderr, "Error identifiing FSD. Tag ID: 0x%x\n", disc->udf_fsd->descTag.tagIdent);
        free(disc->udf_fsd);
        return -1;
    }
    printf("LogicVolIdent: %s\nFileSetIdent: %s\n", (disc->udf_fsd->logicalVolIdent), (disc->udf_fsd->fileSetIdent));
    *lbnlsn = lsnBase;

    //FIXME Maybe not needed. Investigate. 
    memcpy(&descTag, dev+(lsnBase+filesetblock.logicalBlockNum+1)*lbSize, sizeof(tag));
    if(le16_to_cpu(descTag.tagIdent) != TAG_IDENT_TD) {
        fprintf(stderr, "Error loading FSD sequence. TE descriptor not found. LSN: %d, Desc ID: %x\n", lsnBase+filesetblock.logicalBlockNum+1, le16_to_cpu(descTag.tagIdent));
        //        free(disc->udf_fsd);
        return -1;
    }

    return 0;
}


uint8_t get_file(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn, uint32_t lsn) {
    tag descTag;
    struct fileIdentDesc *fid;
    struct fileEntry *fe;
    struct extendedFileEntry *efe;
    uint32_t lbSize = le32_to_cpu(disc->udf_lvd[MAIN_VDS]->logicalBlockSize); //FIXME MAIN_VDS should be verified first 
    uint32_t lsnBase = lbnlsn; 
    uint32_t flen, padding;

    descTag = *(tag *)(dev+lbSize*lsn);
    if(!checksum(descTag)) {
        fprintf(stderr, "Tag checksum failed. Unable to continue.\n");
        return -2;
    }
    //memcpy(&descTag, dev+lbSize*lsn, sizeof(tag));
    //do {    
    //read(fd, file, sizeof(struct fileEntry));

    switch(le16_to_cpu(descTag.tagIdent)) {
        case TAG_IDENT_FID:
            fprintf(stderr, "Never should get there.\n");
            exit(-43);
        case TAG_IDENT_AED:
            printf("\nAED, LSN: %d\n", lsn);
            break;
        case TAG_IDENT_FE:
            fe = (struct fileEntry *)(dev+lbSize*lsn);
            if(crc(fe, sizeof(struct fileEntry) + le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs))) {
                fprintf(stderr, "FE CRC failed.\n");
                return -3;
            }
            printf("\nFE, LSN: %d, EntityID: %s ", lsn, fe->impIdent.ident);
            printf("fileLinkCount: %d, LB recorded: %lu\n", fe->fileLinkCount, fe->logicalBlocksRecorded);
            printf("LEA %d, LAD %d\n", fe->lengthExtendedAttr, fe->lengthAllocDescs);
            if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT) {
                printf("SHORT\n");
                short_ad *sad = (short_ad *)(fe->allocDescs);
                printf("ExtLen: %d, ExtLoc: %d\n", sad->extLength/lbSize, sad->extPosition+lsnBase);
                lsn = lsn + sad->extLength/lbSize;
            } else if((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG) {
                printf("LONG\n");
                long_ad *lad = (long_ad *)(fe->allocDescs);
                printf("ExtLen: %d, ExtLoc: %d\n", lad->extLength/lbSize, lad->extLocation.logicalBlockNum+lsnBase);
                lsn = lsn + lad->extLength/lbSize;
                printf("LSN: %d\n", lsn);
            }
            for(int i=0; i<le32_to_cpu(fe->lengthAllocDescs); i+=8) {
                for(int j=0; j<8; j++)
                    printf("%02x ", fe->allocDescs[i+j]);

                printf("\n");
            }
            printf("\n");

            //TODO is it directory? If is, continue. Otherwise not.
            // We can assume that directory have one or more FID inside.
            // FE have inside long_ad/short_ad.
            if(fe->lengthAllocDescs >= sizeof(struct fileIdentDesc)) {
                for(uint32_t pos=0; pos<fe->lengthAllocDescs; ) {
                    fid = (struct fileIdentDesc *)(fe->allocDescs + pos);
                    if (!checksum(fid->descTag)) {
                        fprintf(stderr, "FID checksum failed.\n");
                        return -4;
                    }
                    if (le16_to_cpu(fid->descTag.tagIdent) == TAG_IDENT_FID) {
                        printf("FID found.\n");
                        flen = 38 + le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent;
                        padding = 4 * ((le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38 + 3)/4) - (le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38);

                        if(crc(fid, flen + padding)) {
                            fprintf(stderr, "FID CRC failed.\n");
                            return -5;
                        }
                        printf("FID: ImpUseLen: %d\n", fid->lengthOfImpUse);
                        printf("FID: FilenameLen: %d\n", fid->lengthFileIdent);
                        if(fid->lengthFileIdent == 0) {
                            printf("ROOT directory\n");
                        } else {
                            printf("Filename: %s\n", fid->fileIdent+fid->lengthOfImpUse);
                        }

                        printf("ICB: LSN: %d, length: %d\n", fid->icb.extLocation.logicalBlockNum + lsnBase, fid->icb.extLength);
                        printf("ROOT ICB: LSN: %d\n", disc->udf_fsd->rootDirectoryICB.extLocation.logicalBlockNum + lsnBase);
                        printf("Actual LSN: %d\n", lsn);

                        if(pos == 0) {
                            printf("Parent. Not Following this one\n");
                        }else if(fid->icb.extLocation.logicalBlockNum + lsnBase == lsn) {
                            printf("Self. Not following this one\n");
                        } else if(fid->icb.extLocation.logicalBlockNum + lsnBase == disc->udf_fsd->rootDirectoryICB.extLocation.logicalBlockNum + lsnBase) {
                            printf("ROOT. Not following this one.\n");
                        } else {
                            printf("ICB to follow.\n");
                            get_file(dev, disc, lbnlsn, lela_to_cpu(fid->icb).extLocation.logicalBlockNum + lsnBase);
                            printf("Return from ICB\n"); 
                        }
                        printf("FLen: %d, padding: %d\n", flen, padding);
                        pos = pos + flen + padding;
                        printf("\n");
                    } else {
                        printf("Ident: %x\n", le16_to_cpu(fid->descTag.tagIdent));
                        break;
                    }
                }
            }
            break;  
        case TAG_IDENT_EFE:
            fe = 0;
            printf("EFE, LSN: %d\n", lsn);
            efe = (struct extendedFileEntry *)(dev+lbSize*lsn); 
            if(crc(efe, sizeof(struct extendedFileEntry) + le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs))) {
                fprintf(stderr, "FE CRC failed.\n");
                return -3;
            }
            printf("\nEFE, LSN: %d, EntityID: %s ", lsn, efe->impIdent.ident);
            printf("fileLinkCount: %d, LB recorded: %lu\n", le16_to_cpu(efe->fileLinkCount), le64_to_cpu(efe->logicalBlocksRecorded));
            printf("LEA %d, LAD %d\n", le32_to_cpu(efe->lengthExtendedAttr), le32_to_cpu(efe->lengthAllocDescs));
            if((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT) {
                printf("SHORT\n");
                short_ad *sad = (short_ad *)(efe->allocDescs);
                printf("ExtLen: %d, ExtLoc: %d\n", sad->extLength/lbSize, sad->extPosition+lsnBase);
                lsn = lsn + sad->extLength/lbSize;
            } else if((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG) {
                printf("LONG\n");
                long_ad *lad = (long_ad *)(efe->allocDescs);
                printf("ExtLen: %d, ExtLoc: %d\n", lad->extLength/lbSize, lad->extLocation.logicalBlockNum+lsnBase);
                lsn = lsn + lad->extLength/lbSize;
                printf("LSN: %d\n", lsn);
            }
            for(int i=0; i<le32_to_cpu(efe->lengthAllocDescs); i+=8) {
                for(int j=0; j<8; j++)
                    printf("%02x ", efe->allocDescs[i+j]);

                printf("\n");
            }
            printf("\n");

            for(uint32_t pos=0; le32_to_cpu(pos<efe->lengthAllocDescs); ) {
                fid = (struct fileIdentDesc *)(efe->allocDescs + pos);
                if (!checksum(fid->descTag)) {
                    fprintf(stderr, "FID checksum failed.\n");
                    return -4;
                }
                if (le16_to_cpu(fid->descTag.tagIdent) == TAG_IDENT_FID) {
                    printf("FID found.\n");
                    flen = 38 + le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent;
                    padding = 4 * ((le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38 + 3)/4) - (le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38);

                    if(crc(fid, flen + padding)) {
                        fprintf(stderr, "FID CRC failed.\n");
                        return -5;
                    }
                    printf("FID: ImpUseLen: %d\n", le16_to_cpu(fid->lengthOfImpUse));
                    printf("FID: FilenameLen: %d\n", fid->lengthFileIdent);
                    if(fid->lengthFileIdent == 0) {
                        printf("ROOT directory\n");
                    } else {
                        printf("Filename: %s\n", fid->fileIdent+le16_to_cpu(fid->lengthOfImpUse));
                    }

                    printf("ICB: LSN: %d, length: %d\n", fid->icb.extLocation.logicalBlockNum + lsnBase, fid->icb.extLength);
                    if(lela_to_cpu(fid->icb).extLocation.logicalBlockNum + lsnBase == lsn) {
                        printf("Self. Not following this one\n");
                    } else if(fid->lengthFileIdent == 0) {
                        printf("We are not going back to ROOT.\n");
                    } else {
                        printf("ICB to follow.\n");
                        get_file(dev, disc, lbnlsn, fid->icb.extLocation.logicalBlockNum + lsnBase);
                        printf("Return from ICB\n"); 
                    }
                    uint32_t flen = 38 + le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent;
                    uint16_t padding = 4 * ((le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38 + 3)/4) - (le16_to_cpu(fid->lengthOfImpUse) + fid->lengthFileIdent + 38);
                    printf("FLen: %d, padding: %d\n", flen, padding);
                    pos = pos + flen + padding;
                    printf("\n");
                } else {
                    printf("Ident: %x\n", fid->descTag.tagIdent);
                    break;
                }
            }
            break;

        default:
            printf("\nIDENT: %x, LSN: %d, addr: 0x%x\n", descTag.tagIdent, lsn, lsn*lbSize);
            /*do{
              ptLength = *(uint8_t *)(dev+lbn*blocksize+pos);
              extLoc = *(uint32_t *)(dev+lbn*blocksize+2+pos);
              filename = (char *)(dev+lbn*blocksize+8+pos);
              printf("extLoc LBN: %d, filename: %s\n", extLoc, filename);
              pos += ptLength + 8 + ptLength%2;
              } while(ptLength > 0);*/
    }            
    //     lsn = lsn + 1;
    //     memcpy(&descTag, dev+lbSize*lsn, sizeof(tag));

    // } while(descTag.tagIdent != 0 );
    return 0;
}

uint8_t get_file_structure(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn) {
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
    lsn = icbloc.logicalBlockNum+lsnBase-1;
    printf("ROOT LSN: %d\n", lsn);
    //memcpy(file, dev+lbSize*lsn, sizeof(struct fileEntry));

    return get_file(dev, disc, lbnlsn, lsn);
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

int verify_vds(struct udf_disc *disc, metadata_err_map_t *map, vds_type_e vds, vds_sequence_t *seq) {
    //metadata_err_map_t map;    
    uint8_t *data;
    //uint16_t crc = 0;
    uint16_t offset = sizeof(tag);

    if(!checksum(disc->udf_pvd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at PVD[%d]\n", vds);
        //map->pvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_PVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_lvd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at LVD[%d]\n", vds);
        //map->lvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_LVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_pd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at PD[%d]\n", vds);
        //map->pd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_PD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_usd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at USD[%d]\n", vds);
        //map->usd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_USD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_iuvd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at IUVD[%d]\n", vds);
        //map->iuvd[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_IUVD, vds, E_CHECKSUM);
    }   
    if(!checksum(disc->udf_td[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at TD[%d]\n", vds);
        //map->td[vds] |= E_CHECKSUM;
        append_error(seq, TAG_IDENT_TD, vds, E_CHECKSUM);
    }

    if(crc(disc->udf_pvd[vds], sizeof(struct primaryVolDesc))) {
        printf("CRC error at PVD[%d]\n", vds);
        //map->pvd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_PVD, vds, E_CRC);
    }
    if(crc(disc->udf_lvd[vds], sizeof(struct logicalVolDesc)+disc->udf_lvd[vds]->mapTableLength)) {
        printf("CRC error at LVD[%d]\n", vds);
        //map->lvd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_LVD, vds, E_CRC);
    }
    if(crc(disc->udf_pd[vds], sizeof(struct partitionDesc))) {
        printf("CRC error at PD[%d]\n", vds);
        //map->pd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_PD, vds, E_CRC);
    }
    if(crc(disc->udf_usd[vds], sizeof(struct unallocSpaceDesc)+(disc->udf_usd[vds]->numAllocDescs)*sizeof(extent_ad))) {
        printf("CRC error at USD[%d]\n", vds);
        //map->usd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_USD, vds, E_CRC);
    }
    if(crc(disc->udf_iuvd[vds], sizeof(struct impUseVolDesc))) {
        printf("CRC error at IUVD[%d]\n", vds);
        //map->iuvd[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_IUVD, vds, E_CRC);
    }
    if(crc(disc->udf_td[vds], sizeof(struct terminatingDesc))) {
        printf("CRC error at TD[%d]\n", vds);
        //map->td[vds] |= E_CRC;
        append_error(seq, TAG_IDENT_TD, vds, E_CRC);
    }

    return 0;
}


int write_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize,  avdp_type_e source, avdp_type_e target) {
    int64_t position = 0;
    tag desc_tag;
    avdp_type_e type = target;

    // Taget type to determine position on media
    if(type == 0) {
        position = sectorsize*256; //First AVDP is on LSN=256
    } else if(type == 1) {
        position = devsize-sectorsize; //Second AVDP is on last LSN
    } else if(type == 2) {
        position = devsize-sectorsize-256*sectorsize; //Third AVDP can be at last LSN-256
    } else {
        position = sectorsize*512; //Unclosed disc have AVDP at sector 512
        type = FIRST_AVDP; //Save it to FIRST_AVDP positon
    }

    printf("DevSize: %zu\n", devsize);
    printf("Current position: %lx\n", position);

    uint8_t * ptr = memcpy(dev+position, disc->udf_anchor[source], sizeof(struct anchorVolDescPtr)); 
    printf("ptr: %p\n", ptr);

    free(disc->udf_anchor[type]);
    disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP

    desc_tag = *(tag *)(dev+position);

    if(!checksum(desc_tag)) {
        fprintf(stderr, "Checksum failure at AVDP[%d]\n", type);
        return -2;
    } else if(le16_to_cpu(desc_tag.tagIdent) != TAG_IDENT_AVDP) {
        fprintf(stderr, "AVDP not found at 0x%lx\n", position);
        return -4;
    }

    memcpy(disc->udf_anchor[type], dev+position, sizeof(struct anchorVolDescPtr));
    if(crc(disc->udf_anchor[type], sizeof(struct anchorVolDescPtr))) {
        printf("CRC error at AVDP[%d]\n", type);
        return -3;
    }

    printf("AVDP[%d] successfully written.\n", type);
    return 0;
}

int fix_vds(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, avdp_type_e source, vds_sequence_t *seq) { 
    uint8_t *position_main, *position_reserve;
    int8_t counter = 0;
    tag descTag;

    // Go to first address of VDS
    position_main = dev+sectorsize*(disc->udf_anchor[source]->mainVolDescSeqExt.extLocation);
    position_reserve = dev+sectorsize*(disc->udf_anchor[source]->reserveVolDescSeqExt.extLocation);


    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        if(seq->main[i].error != 0 && seq->reserve[i].error != 0) {
            //Both descriptors are broken
            //FIXME Deal with it somehow        
        } else if(seq->main[i].error != 0) {
            //Copy Reserve -> Main
            memcpy(position_main + (i+1)*sectorsize, position_reserve + (i+1)*sectorsize, sectorsize);
        } else if(seq->reserve[i].error != 0) {
            //Copy Main -> Reserve
            memcpy(position_reserve + (i+1)*sectorsize, position_main + (i+1)*sectorsize, sectorsize);
        } else {
           //Shloud not happen. Die.
           printf("Logical paradox. Should not ever happend. Exiting.\n");
           exit(8);
        }
    }


    return 0;
}
