
#include "udffsck.h"
#include "utils.h"
#include "libudffs.h"

int get_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize, avdp_type_e type) {
    int64_t position = 0;
    tag *desc_tag;
    
    printf("Error: %s\n", strerror(errno));
    //printf("FD: 0x%x\n", fd);
    if(type == FIRST_AVDP)
        //position = udf_lseek64(fd, sectorsize*256, SEEK_SET); // Seek to AVDP point
        position = sectorsize*256;
    else if(type == SECOND_AVDP) {
        fprintf(stderr, "FIXME! Seeking to First AVDP instead of Second\n");
        //position = udf_lseek64(fd, sectorsize*256, SEEK_SET);
        position = sectorsize*devsize-sectorsize;
    } else {
        fprintf(stderr, "Unknown AVDP type. Exiting.\n");
        return -1;
    }

    printf("Error: %s\n", strerror(errno));
    printf("Current position: %x\n", position);
    
    disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP
    
    printf("sizeof anchor: %d\n", sizeof(struct anchorVolDescPtr));

    //read(fd, disc->udf_anchor[type], sizeof(struct anchorVolDescPtr)); // Load data
    memcpy(disc->udf_anchor[type], dev+position, sizeof(struct anchorVolDescPtr));
    printf("Error: %s\n", strerror(errno));
    printf("Current position: %x\n", position);
    printf("desc_tag ptr: %p\n", disc->udf_anchor[type]->descTag);
    printf("AVDP: TagIdent: %x\n", disc->udf_anchor[type]->descTag.tagIdent);

    return 0;
}

#define VDS_STRUCT_AMOUNT 9 //FIXME Move to somewhere else, not keep it here.
int get_vds(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds) {
    int64_t position = 0;
    int8_t counter = 0;
    tag descTag;

    // Go to first address of VDS 
    switch(vds) {
        case MAIN_VDS:
            position = udf_lseek64(fd, sectorsize*(disc->udf_anchor[0]->mainVolDescSeqExt.extLocation), SEEK_SET);
            break;
        case RESERVE_VDS:
            position = udf_lseek64(fd, sectorsize*(disc->udf_anchor[0]->reserveVolDescSeqExt.extLocation), SEEK_SET);
            break;
    }
    printf("Current position: %x\n", position);
    
    // Go thru descriptors until TagIdent is 0 or amout is too big to be real
    while(counter < VDS_STRUCT_AMOUNT) {
        counter++;

        // Read tag
        read(fd, &descTag, sizeof(descTag));    

        printf("Tag ID: %d\n", descTag.tagIdent);
        udf_lseek64(fd, -sizeof(descTag), SEEK_CUR); // Go back where descriptor started
        
        // What kind of descriptor is that?
        switch(descTag.tagIdent) {
            case TAG_IDENT_PVD:
                if(disc->udf_pvd[vds] != 0) {
                    fprintf(stderr, "Structure PVD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_pvd[vds] = malloc(sizeof(struct primaryVolDesc)); // Prepare memory
                position += read(fd, disc->udf_pvd[vds], sizeof(struct primaryVolDesc)); // Load data
                position = udf_lseek64(fd, sectorsize - sizeof(struct primaryVolDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_IUVD:
                if(disc->udf_iuvd[vds] != 0) {
                    fprintf(stderr, "Structure IUVD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_iuvd[vds] = malloc(sizeof(struct impUseVolDesc)); // Prepare memory
                position += read(fd, disc->udf_iuvd[vds], sizeof(struct impUseVolDesc)); // Load data
                position = udf_lseek64(fd, sectorsize - sizeof(struct impUseVolDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_PD:
                if(disc->udf_pd[vds] != 0) {
                    fprintf(stderr, "Structure PD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_pd[vds] = malloc(sizeof(struct partitionDesc)); // Prepare memory
                position += read(fd, disc->udf_pd[vds], sizeof(struct partitionDesc)); // Load data
                position = udf_lseek64(fd, sectorsize - sizeof(struct partitionDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_LVD:
                if(disc->udf_lvd[vds] != 0) {
                    fprintf(stderr, "Structure LVD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                printf("LVD size: %p\n", sizeof(struct logicalVolDesc));
                disc->udf_lvd[vds] = malloc(sizeof(struct logicalVolDesc)); // Prepare memory
                position += read(fd, disc->udf_lvd[vds], sizeof(struct logicalVolDesc)); // Load data
                position = udf_lseek64(fd, sectorsize - sizeof(struct logicalVolDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_USD:
                if(disc->udf_usd[vds] != 0) {
                    fprintf(stderr, "Structure USD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_usd[vds] = malloc(sizeof(struct unallocSpaceDesc)); // Prepare memory
                position += read(fd, disc->udf_usd[vds], sizeof(struct unallocSpaceDesc)); // Load data
                position = udf_lseek64(fd, sectorsize - sizeof(struct unallocSpaceDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_TD:
                if(disc->udf_td[vds] != 0) {
                    fprintf(stderr, "Structure TD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_td[vds] = malloc(sizeof(struct terminatingDesc)); // Prepare memory
                position += read(fd, disc->udf_td[vds], sizeof(struct terminatingDesc)); // Load data
                position = udf_lseek64(fd, sectorsize - sizeof(struct terminatingDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case 0:
                // Found end of VDS, ending.
                return 0;
            default:
                // Unkown TAG
                fprintf(stderr, "Unknown TAG found at %p. Ending.\n", position);
                exit(-3);
        }
    }
    return 0;
}

uint8_t get_fsd(int fd, struct udf_disc *disc, int sectorsize) {
    long_ad *lap;
    tag descTag;
    lap = (long_ad *)disc->udf_lvd[0]->logicalVolContentsUse;
    lb_addr filesetblock = lap->extLocation;
    uint32_t filesetlen = lap->extLength;
    disc->udf_fsd = malloc(sizeof(struct fileSetDesc));
    udf_lseek64(fd, 257*sectorsize, SEEK_SET);
    read(fd, disc->udf_fsd, sizeof(struct fileSetDesc));
    
    if(disc->udf_fsd->descTag.tagIdent != TAG_IDENT_FSD) {
        fprintf(stderr, "Error identifiing FSD.\n");
        free(disc->udf_fsd);
        return -1;
    }
    printf("LAP: length: %x, LBN: %x, PRN: %x\n", filesetlen, filesetblock.logicalBlockNum, filesetblock.partitionReferenceNum);
    
    udf_lseek64(fd, 258*sectorsize, SEEK_SET);
    read(fd, &descTag, sizeof(tag));
    if(descTag.tagIdent != TAG_IDENT_TD) {
        fprintf(stderr, "Error loading FSD sequence. TE descriptor not found. Desc ID: %x\n", descTag.tagIdent);
        free(disc->udf_fsd);
        return -2;
    }

    return 0;
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
    return calculate_checksum(descTag) == descTag.tagChecksum;
}

int crc(void * desc, uint16_t size) {
    uint8_t offset = sizeof(tag);
    tag *descTag = desc;
    uint16_t crc = 0;
    return descTag->descCRC != udf_crc((uint8_t *)(desc) + offset, size - offset, crc);
}

int verify_vds(struct udf_disc *disc, vds_type_e vds) {
    metadata_err_map_t map;    
    uint8_t *data;
    //uint16_t crc = 0;
    uint16_t offset = sizeof(tag);

    if(!checksum(disc->udf_pvd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at PVD[%d]\n", vds);
        map.pvd[vds] |= E_CHECKSUM;
    }   
    if(!checksum(disc->udf_lvd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at LVD[%d]\n", vds);
        map.lvd[vds] |= E_CHECKSUM;
    }   
    if(!checksum(disc->udf_pd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at PD[%d]\n", vds);
        map.pd[vds] |= E_CHECKSUM;
    }   
    if(!checksum(disc->udf_usd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at USD[%d]\n", vds);
        map.usd[vds] |= E_CHECKSUM;
    }   
    if(!checksum(disc->udf_iuvd[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at IUVD[%d]\n", vds);
        map.iuvd[vds] |= E_CHECKSUM;
    }   
    if(!checksum(disc->udf_td[vds]->descTag)) {
        fprintf(stderr, "Checksum failure at TD[%d]\n", vds);
        map.td[vds] |= E_CHECKSUM;
    }

    if(crc(disc->udf_pvd[vds], sizeof(struct primaryVolDesc))) {
        printf("CRC error at PVD[%d]\n", vds);
        map.pvd[vds] |= E_CRC;
    }
    if(crc(disc->udf_lvd[vds], sizeof(struct logicalVolDesc))) {
        printf("CRC error at LVD[%d]\n", vds);
        map.lvd[vds] |= E_CRC;
    }
    if(crc(disc->udf_pd[vds], sizeof(struct partitionDesc))) {
        printf("CRC error at PD[%d]\n", vds);
        map.pd[vds] |= E_CRC;
    }
    if(crc(disc->udf_usd[vds], sizeof(struct unallocSpaceDesc))) {
        printf("CRC error at USD[%d]\n", vds);
        map.usd[vds] |= E_CRC;
    }
    if(crc(disc->udf_iuvd[vds], sizeof(struct impUseVolDesc))) {
        printf("CRC error at IUVD[%d]\n", vds);
        map.iuvd[vds] |= E_CRC;
    }
    if(crc(disc->udf_td[vds], sizeof(struct terminatingDesc))) {
        printf("CRC error at TD[%d]\n", vds);
        map.td[vds] |= E_CRC;
    }

    return 0;
}


uint8_t get_file_structure(const uint8_t *dev, const struct udf_disc *disc) {
    uint16_t blocksize = disc->udf_lvd[0]->logicalBlockSize;
    struct fileEntry *file;
    tag descTag;
    uint32_t lbn;
    // Go to ROOT ICB 
    lb_addr icbloc = disc->udf_fsd->rootDirectoryICB.extLocation; 
    
    file = malloc(sizeof(struct fileEntry));
    //lseek64(fd, blocksize*(257+icbloc.logicalBlockNum), SEEK_SET);
    //read(fd, file, sizeof(struct fileEntry));
    lbn = 257+icbloc.logicalBlockNum;
    memcpy(file, dev+blocksize*lbn, sizeof(struct fileEntry));
    printf("ROOT ICB IDENT: %x\n", file->descTag.tagIdent);
    //printf("NumEntries: %d\n", file->icbTag.numEntries);
/* Tag Identifier (ECMA 167r3 4/7.2.1) 
#define TAG_IDENT_FSD			0x0100
#define TAG_IDENT_FID			0x0101
#define TAG_IDENT_AED			0x0102
#define TAG_IDENT_IE			0x0103
#define TAG_IDENT_TE			0x0104
#define TAG_IDENT_FE			0x0105
#define TAG_IDENT_EAHD			0x0106
#define TAG_IDENT_USE			0x0107
#define TAG_IDENT_SBD			0x0108
#define TAG_IDENT_PIE			0x0109
#define TAG_IDENT_EFE			0x010A*/
    memcpy(&descTag, dev+blocksize*lbn, sizeof(tag));
    while(descTag.tagIdent != 0 ) {
        //read(fd, file, sizeof(struct fileEntry));
        lbn = lbn + 1;
        memcpy(&descTag, dev+blocksize*lbn, sizeof(tag));
        
        switch(descTag.tagIdent) {
            case TAG_IDENT_FID:
                printf("FID, LSN: %d\n", lbn);
                break;
            case TAG_IDENT_AED:
                printf("AED, LSN: %d\n", lbn);
                break;
            case TAG_IDENT_FE:
                printf("FE, LSN: %d\n", lbn);
                break;  
            case TAG_IDENT_EAHD:
                printf("EAHD, LSN: %d\n", lbn);
                break;

            default:
                printf("IDENT: %x, LSN: %d, addr: 0x%x\n", descTag.tagIdent, lbn, lbn*blocksize);
                
        }
        //lseek64(fd, 259*blocksize+blocksize*(ff+1), SEEK_SET);
    }

    //printf("ICB LBN: %x\n", icbloc.logicalBlockNum);
    return 0;
}
