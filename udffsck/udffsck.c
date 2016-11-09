
#include "udffsck.h"
#include "utils.h"

int get_pvd(int fd, struct udf_disc *disc, int sectorsize, vds_type_e type) {
    int64_t position = 0;
    switch(type) {
        case MAIN_VDS:
            position = udf_lseek64(fd, sectorsize*(disc->udf_anchor[0]->mainVolDescSeqExt.extLocation), SEEK_SET);
            break;
        case RESERVE_VDS: //FIXME not mainVol... but reserve...
            position = udf_lseek64(fd, sectorsize*(disc->udf_anchor[0]->mainVolDescSeqExt.extLocation), SEEK_SET);
            break;
    }
    printf("Current position: %x\n", position);
    
    disc->udf_pvd[type] = malloc(sizeof(struct primaryVolDesc)); 
    read(fd, disc->udf_pvd[type], sizeof(struct primaryVolDesc));
    printf("PVD: TagIdent: %x\n", disc->udf_pvd[type]->descTag.tagIdent);
}

int get_avdp(int fd, struct udf_disc *disc, int sectorsize, avdp_type_e type) {
    int64_t position = 0;
    tag *desc_tag;
    
    printf("Error: %s\n", strerror(errno));
    printf("FD: 0x%x\n", fd);
    if(type == FIRST_AVDP)
        position = udf_lseek64(fd, sectorsize*256, SEEK_SET); // Seek to AVDP point
    else if(type == SECOND_AVDP)
        position = udf_lseek64(fd, sectorsize*256, SEEK_SET); //FIXME seek to last LSN
    else {
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


int get_vds(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds) {
    return 0;
}
