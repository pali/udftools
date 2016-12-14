#ifndef __UDFFSCK_H__
#define __UDFFSCK_H__

#include <ecma_167.h>
#include <libudffs.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

typedef enum {
    FIRST_AVDP = 0,
    SECOND_AVDP,
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
    uint8_t lvid[2];
} metadata_err_map_t;

#define E_CHECKSUM 0b00000001
#define E_CRC      0b00000010

// Anchor volume descriptor points to Mvds and Rvds
int get_avdp(uint8_t *dev, struct udf_disc *disc, size_t sectorsize, size_t devsize, avdp_type_e type);

// Volume descriptor sequence
int get_vds(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds);
// Load all PVD descriptors into disc structure
int get_pvd(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds);

// Logical Volume Integrity Descriptor
int get_lvid();

int verify_vds(struct udf_disc *disc, vds_type_e vds);

uint8_t get_fsd(int fd, struct udf_disc *disc, int sectorsize);
uint8_t get_file_structure(const uint8_t *dev, const struct udf_disc *disc);
#endif //__UDFFSCK_H__
