/*
 * mkudffs.h
 *
 * Copyright (c) 2001-2002  Ben Fennema <bfennema@falcon.csc.calpoly.edu>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __MKUDFFS_H
#define __MKUDFFS_H

#include "ecma_167.h"
#include "osta_udf.h"
#include "udf_endian.h"
#include "libudffs.h"

#define CS0				0x00000001
#define UDF_ID_APPLICATION		"*Linux mkudffs"
#define MKUDFFS_VERSION			"1.0.0b2"

#define EMAIL_STRING			"<linux_udf@hpesjro.fc.hp.com>"

#define UDFFS_DATE			"2002/02/09"
#define UDFFS_VERSION			"1.0.0-cvs"

#define DEFAULT_HD	0
#define DEFAULT_DVD	0
#define DEFAULT_DVDRAM	0
#define DEFAULT_WORM	1
#define DEFAULT_MO	1
#define DEFAULT_CDRW	2
#define DEFAULT_CDR	3
#define DEFAULT_DVDRW	4

#ifndef NAME_MAX
#define NAME_MAX	255
#endif

extern char *udf_space_type_str[UDF_SPACE_TYPE_SIZE];

void udf_init_disc(struct udf_disc *);
int udf_set_version(struct udf_disc *, int);
void split_space(struct udf_disc *);
void dump_space(struct udf_disc *);
int write_disc(struct udf_disc *);
void setup_vrs(struct udf_disc *);
void setup_anchor(struct udf_disc *);
void setup_partition(struct udf_disc *);
int setup_space(struct udf_disc *, struct udf_extent *, uint32_t);
int setup_fileset(struct udf_disc *, struct udf_extent *);
int setup_root(struct udf_disc *, struct udf_extent *);
void calc_space(struct udf_disc *, struct udf_extent *);
void setup_vds(struct udf_disc *);
void setup_pvd(struct udf_disc *, struct udf_extent *, struct udf_extent *, uint32_t);
void setup_lvd(struct udf_disc *, struct udf_extent *, struct udf_extent *, struct udf_extent *, uint32_t);
void setup_pd(struct udf_disc *, struct udf_extent *, struct udf_extent *, uint32_t);
void setup_usd(struct udf_disc *, struct udf_extent *, struct udf_extent *, uint32_t);
void setup_iuvd(struct udf_disc *, struct udf_extent *, struct udf_extent *, uint32_t);
void setup_td(struct udf_disc *, struct udf_extent *, struct udf_extent *, uint32_t);
void setup_lvid(struct udf_disc *, struct udf_extent *);
void setup_stable(struct udf_disc *, struct udf_extent *[4], struct udf_extent *);
void setup_vat(struct udf_disc *, struct udf_extent *);
void add_type1_partition(struct udf_disc *, uint16_t);
void add_type2_sparable_partition(struct udf_disc *, uint16_t, uint8_t, uint16_t);
void add_type2_virtual_partition(struct udf_disc *, uint16_t);
struct sparablePartitionMap *find_type2_sparable_partition(struct udf_disc *, uint16_t);

#endif /* __MKUDFFS_H */
