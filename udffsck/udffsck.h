#ifndef __UDFFSCK_H__
#define __UDFFSCK_H__

#include <ecma_167.h>
#include <ecma_119.h>
#include <libudffs.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

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
    uint8_t vrs[3];
    uint8_t anchor[3];
    uint8_t pvd[2];
    uint8_t lvd[2];
    uint8_t pd[2];
    uint8_t usd[2];
    uint8_t iuvd[2];
    uint8_t td[2];
    uint8_t lvid;
} metadata_err_map_t;

#define E_CHECKSUM 0b00000001
#define E_CRC      0b00000010

// Anchor volume descriptor points to Mvds and Rvds
int get_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize, avdp_type_e type);
int write_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize,  avdp_type_e source, avdp_type_e target);

// Volume descriptor sequence
int get_vds(uint8_t *dev, struct udf_disc *disc, int sectorsize, avdp_type_e avdp, vds_type_e vds);
int get_lvid(uint8_t *dev, struct udf_disc *disc, int sectorsize);
// Load all PVD descriptors into disc structure
//int get_pvd(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds);

// Logical Volume Integrity Descriptor
int get_lvid();

int verify_vds(struct udf_disc *disc, metadata_err_map_t *map, vds_type_e vds);

uint8_t get_fsd(uint8_t *dev, struct udf_disc *disc, int sectorsize, uint32_t *lbnlsn);
uint8_t get_file_structure(const uint8_t *dev, const struct udf_disc *disc, uint32_t lbnlsn);

uint8_t get_path_table(uint8_t *dev, uint16_t sectorsize, pathTableRec *table);

#endif //__UDFFSCK_H__
