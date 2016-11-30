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

// Anchor volume descriptor points to Mvds and Rvds
int get_avdp(int fd, struct udf_disc *disc, int sectorsize, avdp_type_e type);

// Volume descriptor sequence
int get_vds(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds);
// Load all PVD descriptors into disc structure
int get_pvd(int fd, struct udf_disc *disc, int sectorsize, vds_type_e vds);

// Logical Volume Integrity Descriptor
int get_lvid();

int verify_vds(struct udf_disc *disc, vds_type_e vds);

#endif //__UDFFSCK_H__
