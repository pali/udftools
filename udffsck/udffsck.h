#ifndef __UDFFSCK_H__
#define __UDFFSCK_H__

#include <ecma_167.h>
#include <ecma_119.h>
#include <libudffs.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#define VDS_STRUCT_AMOUNT 8 //FIXME Move to somewhere else, not keep it here.

typedef enum {
    FIRST_AVDP = 0,
    SECOND_AVDP,
    THIRD_AVDP,
} avdp_type_e;

typedef enum {
    MAIN_VDS = 0,
    RESERVE_VDS,
} vds_type_e;

typedef struct {
    uint16_t tagIdent;
    uint32_t tagLocation;
    uint8_t error;
} metadata_t;

typedef struct {
    metadata_t anchor[3];
    metadata_t main[VDS_STRUCT_AMOUNT];
    metadata_t reserve[VDS_STRUCT_AMOUNT];   
    metadata_t lvid; 
} vds_sequence_t;

struct filesystemStats {
    uint32_t expNumOfFiles;
    uint32_t countNumOfFiles;
    uint32_t expNumOfDirs;
    uint32_t countNumOfDirs;
    uint16_t minUDFReadRev;
    uint16_t minUDFWriteRev;
    uint16_t maxUDFWriteRev;
    uint64_t usedSpace;
};

// Implementation Use for Logical Volume Integrity Descriptor (ECMA 167r3 TODO, UDF 2.2.6.4)
struct impUseLVID {
    regid impID;
    uint32_t numOfFiles;
    uint32_t numOfDirs;
    uint16_t minUDFReadRev;
    uint16_t minUDFWriteRev;
    uint16_t maxUDFWriteRev;
    uint8_t  impUse[0]; 
} __attribute__ ((packed));

typedef struct {
    uint8_t vrs[3];
 //   uint8_t anchor[3];
/*    uint8_t pvd[2];
    uint8_t lvd[2];
    uint8_t pd[2];
    uint8_t usd[2];
    uint8_t iuvd[2];
    uint8_t td[2];*/
    uint8_t lvid;
} metadata_err_map_t;

#define E_CHECKSUM  0b00000001
#define E_CRC       0b00000010
#define E_POSITION  0b00000100
#define E_WRONGDESC 0b00001000

// Anchor volume descriptor points to Mvds and Rvds
int get_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize, avdp_type_e type);
int write_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize,  avdp_type_e source, avdp_type_e target);

// Volume descriptor sequence
int get_vds(uint8_t *dev, struct udf_disc *disc, int sectorsize, avdp_type_e avdp, vds_type_e vds, vds_sequence_t *seq);
int get_lvid(uint8_t *dev, struct udf_disc *disc, int sectorsize, struct filesystemStats *stats);
// Load all PVD descriptors into disc structure
//int get_pvd(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds);


int verify_vds(struct udf_disc *disc, vds_sequence_t *map, vds_type_e vds, vds_sequence_t *seq);

uint8_t get_fsd(uint8_t *dev, struct udf_disc *disc, int sectorsize, uint32_t *lbnlsn);
uint8_t get_file_structure(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn, struct filesystemStats *stats);

uint8_t get_path_table(uint8_t *dev, uint16_t sectorsize, pathTableRec *table);
int fix_vds(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, avdp_type_e source, vds_sequence_t *seq, uint8_t interactive, uint8_t autofix); 

int copy_descriptor(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, uint32_t sourcePosition, uint32_t destinationPosition, size_t amount);
#endif //__UDFFSCK_H__
