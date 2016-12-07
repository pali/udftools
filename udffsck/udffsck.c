
#include "udffsck.h"
#include "utils.h"
#include "libudffs.h"

int get_avdp(int fd, struct udf_disc *disc, int sectorsize, avdp_type_e type) {
    int64_t position = 0;
    tag *desc_tag;
    
    printf("Error: %s\n", strerror(errno));
    printf("FD: 0x%x\n", fd);
    if(type == FIRST_AVDP)
        position = udf_lseek64(fd, sectorsize*256, SEEK_SET); // Seek to AVDP point
    else if(type == SECOND_AVDP) {
        fprintf(stderr, "FIXME! Seeking to First AVDP instead of Second\n");
        position = udf_lseek64(fd, sectorsize*256, SEEK_SET); //FIXME seek to last LSN
    } else {
        fprintf(stderr, "Unknown AVDP type. Exiting.\n");
        return -1;
    }

    printf("Error: %s\n", strerror(errno));
    printf("Current position: %x\n", position);
    
    disc->udf_anchor[type] = malloc(sizeof(struct anchorVolDescPtr)); // Prepare memory for AVDP
    
    printf("sizeof anchor: %d\n", sizeof(struct anchorVolDescPtr));

    read(fd, disc->udf_anchor[type], sizeof(struct anchorVolDescPtr)); // Load data
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
                position = udf_lseek64(fd, 0x200 - sizeof(struct primaryVolDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_IUVD:
                if(disc->udf_iuvd[vds] != 0) {
                    fprintf(stderr, "Structure IUVD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_iuvd[vds] = malloc(sizeof(struct impUseVolDesc)); // Prepare memory
                position += read(fd, disc->udf_iuvd[vds], sizeof(struct impUseVolDesc)); // Load data
                position = udf_lseek64(fd, 0x200 - sizeof(struct impUseVolDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_PD:
                if(disc->udf_pd[vds] != 0) {
                    fprintf(stderr, "Structure PD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_pd[vds] = malloc(sizeof(struct partitionDesc)); // Prepare memory
                position += read(fd, disc->udf_pd[vds], sizeof(struct partitionDesc)); // Load data
                position = udf_lseek64(fd, 0x200 - sizeof(struct partitionDesc), SEEK_CUR);
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
                position = udf_lseek64(fd, 0x200 - sizeof(struct logicalVolDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_USD:
                if(disc->udf_usd[vds] != 0) {
                    fprintf(stderr, "Structure USD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_usd[vds] = malloc(sizeof(struct unallocSpaceDesc)); // Prepare memory
                position += read(fd, disc->udf_usd[vds], sizeof(struct unallocSpaceDesc)); // Load data
                position = udf_lseek64(fd, 0x200 - sizeof(struct unallocSpaceDesc), SEEK_CUR);
                printf("New positon is %p\n", position);
                break;
            case TAG_IDENT_TD:
                if(disc->udf_td[vds] != 0) {
                    fprintf(stderr, "Structure TD is already set. Probably error at tag or media\n");
                    exit(-4);
                }
                disc->udf_td[vds] = malloc(sizeof(struct terminatingDesc)); // Prepare memory
                position += read(fd, disc->udf_td[vds], sizeof(struct terminatingDesc)); // Load data
                position = udf_lseek64(fd, 0x200 - sizeof(struct terminatingDesc), SEEK_CUR);
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
