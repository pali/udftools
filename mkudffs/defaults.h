/*
 * defaults.h
 *
 * Copyright (c) 2001-2002  Ben Fennema
 * Copyright (c) 2016-2017  Pali Rohár <pali.rohar@gmail.com>
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

#ifndef _DEFAULTS_H
#define _DEFAULTS_H 1

extern int default_media[];
extern struct udf_sizing default_sizing[][UDF_ALLOC_TYPE_SIZE];

extern struct primaryVolDesc default_pvd;
extern struct logicalVolDesc default_lvd;
extern struct volDescPtr default_vdp;
extern struct impUseVolDescImpUse default_iuvdiu;
extern struct impUseVolDesc default_iuvd;
extern struct partitionDesc default_pd;
extern struct unallocSpaceDesc default_usd;
extern struct terminatingDesc default_td;
extern struct logicalVolIntegrityDesc default_lvid;
extern struct logicalVolIntegrityDescImpUse default_lvidiu;
extern struct sparingTable default_stable;
extern struct sparablePartitionMap default_sparmap;
extern struct virtualAllocationTable15 default_vat15;
extern struct virtualAllocationTable20 default_vat20;
extern struct virtualPartitionMap default_virtmap;
extern struct fileSetDesc default_fsd;
extern struct fileEntry default_fe;
extern struct extendedFileEntry default_efe;

extern struct mbr default_mbr;

#endif /* _DEFAULTS_H */
