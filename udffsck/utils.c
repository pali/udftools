#include "utils.h"


int64_t udf_lseek64(int fd, int64_t offset, int whence) {
#if defined(HAVE_LSEEK64)
	return lseek64(fd, offset, whence);
#elif defined(HAVE_LLSEEK)
	return llseek(fd, offset, whence);
#else
	return lseek(fd, offset, whence);
#endif
}


void read_tag(tag id) {
    printf("\tIdentification Tag\n"
           "\t==================\n");
    printf("\tID: %d (", id.tagIdent);
    switch(id.tagIdent) {
        case TAG_IDENT_PVD:
            printf("PVD");
            break;
        case TAG_IDENT_AVDP:
            printf("AVDP");
            break;
        case TAG_IDENT_VDP:
            printf("VDP");
            break;
        case TAG_IDENT_IUVD:
            printf("IUVD");
            break;
        case TAG_IDENT_PD:
            printf("PD");
            break;
        case TAG_IDENT_LVD:
            printf("LVD");
            break;
        case TAG_IDENT_USD:
            printf("USD");
            break;
        case TAG_IDENT_TD:
            printf("TD");
            break;    
        case TAG_IDENT_LVID:
            printf("LVID");
            break;
    }
    printf(")\n");
    printf("\tVersion: %d\n", id.descVersion);
    printf("\tChecksum: 0x%x\n", id.tagChecksum);
    printf("\tSerial Number: 0x%x\n", id.tagSerialNum);
    printf("\tDescriptor CRC: 0x%x, Length: %d\n", id.descCRC, id.descCRCLength);
    printf("\tTag Location: 0x%x\n", id.tagLocation);
}

int print_disc(struct udf_disc *disc) {
    printf("UDF Metadata Overview\n"
           "=====================\n");
    printf("UDF revision: %d\n", disc->udf_rev);
    printf("Disc blocksize: %d\n", disc->blocksize);
    printf("Disc blocksize bits: %d\n", disc->blocksize_bits);
    printf("Flags: %X\n\n", disc->flags);


    printf("AVDP\n"
           "----\n");
    for(int i=0; i<3; i++) {
        printf("[%d]\n", i);
        if(disc->udf_anchor[i] != 0) {
            read_tag(disc->udf_anchor[i]->descTag);
        }
    }
    
    printf("PVD\n"
           "---\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_pvd[i] != 0) {
            read_tag(disc->udf_pvd[i]->descTag);
        }
    }

    printf("LVD\n"
           "---\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_lvd[i] != 0) {
            read_tag(disc->udf_lvd[i]->descTag);
        }
    }

    printf("PD\n"
           "--\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_pd[i] != 0) {
            read_tag(disc->udf_pd[i]->descTag);
        }
    }

    printf("USD\n"
           "---\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_usd[i] != 0) {
            read_tag(disc->udf_usd[i]->descTag);
        }
    }

    printf("IUVD\n"
           "----\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_iuvd[i] != 0) {
            read_tag(disc->udf_iuvd[i]->descTag);
        }
    }

    printf("TD\n"
           "--\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_td[i] != 0) {
            read_tag(disc->udf_td[i]->descTag);
        }
    }
}
