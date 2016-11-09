#ifndef __UDFFSCK_H__
#define __UDFFSCK_H__

#include <ecma_167.h>
#include <libudffs.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>


#define PRIMARY_AVDP 0
#define SECONDARY_AVDP 1
#define MAIN_VDS 0
#define RESERVE_VDS 1

int get_pvd(int fd, struct udf_disc *disc, int sectorsize);
int get_avdp(int fd, struct udf_disc *disc, int sectorsize, int avdp);
int get_vds(int fd, struct udf_disc *disc, int sectorsize, int vds);

#endif //__UDFFSCK_H__
