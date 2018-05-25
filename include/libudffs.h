/*
 * libudffs.h
 *
 * Copyright (c) 2001-2002  Ben Fennema <bfennema@falcon.csc.calpoly.edu>
 * Copyright (c) 2014-2017  Pali Rohár <pali.rohar@gmail.com>
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

/**
 * @file
 * libudffs defines and structure definitions
 */

#ifndef __LIBUDFFS_H
#define __LIBUDFFS_H

#include "ecma_167.h"
#include "osta_udf.h"
#include "udf_endian.h"

#define FLAG_FREED_BITMAP		0x00000001
#define FLAG_FREED_TABLE		0x00000002
#define FLAG_UNALLOC_BITMAP		0x00000004
#define FLAG_UNALLOC_TABLE		0x00000008
#define FLAG_SPACE_BITMAP		(FLAG_FREED_BITMAP|FLAG_UNALLOC_BITMAP)
#define FLAG_SPACE_TABLE		(FLAG_FREED_TABLE|FLAG_UNALLOC_TABLE)
#define FLAG_SPACE			(FLAG_SPACE_BITMAP|FLAG_SPACE_TABLE)

#define FLAG_LOCALE			0x00000010
#define FLAG_UNICODE8			0x00000020
#define FLAG_UNICODE16			0x00000040
#define FLAG_UTF8			0x00000080
#define FLAG_CHARSET			(FLAG_LOCALE|FLAG_UNICODE8|FLAG_UNICODE16|FLAG_UTF8)

#define FLAG_STRATEGY4096		0x00000100
#define FLAG_BLANK_TERMINAL		0x00000200

#define FLAG_MIN_300_BLOCKS		0x00000400
#define FLAG_CLOSED			0x00000800
#define FLAG_VAT			0x00001000

#define FLAG_EFE			0x00002000

#define FLAG_NO_WRITE			0x00004000

#define FLAG_BOOTAREA_PRESERVE		0x00010000
#define FLAG_BOOTAREA_ERASE		0x00020000
#define FLAG_BOOTAREA_MBR		0x00040000
#define FLAG_BOOTAREA_MASK		(FLAG_BOOTAREA_PRESERVE|FLAG_BOOTAREA_ERASE|FLAG_BOOTAREA_MBR)

struct udf_extent;
struct udf_desc;
struct udf_data;

enum udf_space_type
{
	RESERVED	= 0x0001,	/* Reserved Space */
	VRS		= 0x0002,	/* Volume Recognition Sequence */
	ANCHOR		= 0x0004,	/* Anchor */
	MVDS		= 0x0008,	/* Main Volume Descriptor Sequence */
	RVDS		= 0x0010,	/* Reserve Volume Descriptor Sequence */
	LVID		= 0x0020,	/* Logical Volume Integrity Descriptor */
	STABLE		= 0x0040,	/* Sparing Table */
	SSPACE		= 0x0080,	/* Sparing Space */
	PSPACE		= 0x0100,	/* Partition Space */
	USPACE		= 0x0200,	/* Unallocated Space */
	BAD		= 0x0400,	/* Bad Blocks */
	MBR		= 0x0800,	/* MBR Boot Area */
	UDF_SPACE_TYPE_SIZE = 12,
};

struct udf_sizing
{
	uint32_t	align;
	uint32_t	numSize;
	uint32_t	denomSize;
	uint32_t	minSize;
};

enum udf_alloc_type
{
	VDS_SIZE,
	LVID_SIZE,
	STABLE_SIZE,
	SSPACE_SIZE,
	PSPACE_SIZE,
	UDF_ALLOC_TYPE_SIZE,
};

struct udf_disc
{
	uint16_t			udf_rev;
	uint16_t			udf_write_rev;
	uint32_t			blocksize;
	uint32_t			blocks;
	uint32_t			flags;
	unsigned int			blkssz;
	uint64_t			blksize;
	uint32_t			num_files;
	uint32_t			num_dirs;
	uint32_t			free_space_blocks;
	uint32_t			total_space_blocks;

	uint32_t			uid;
	uint32_t			gid;
	uint16_t			mode;

	struct udf_sizing		sizing[UDF_ALLOC_TYPE_SIZE];

	int				(*write)(struct udf_disc *, struct udf_extent *);
	void				*write_data;

	struct volStructDesc		*udf_vrs[3];
	struct anchorVolDescPtr		*udf_anchor[3];
	struct primaryVolDesc		*udf_pvd[2];
	struct logicalVolDesc		*udf_lvd[2];
	struct partitionDesc		*udf_pd[2];
	struct partitionDesc		*udf_pd2[2];
	struct unallocSpaceDesc		*udf_usd[2];
	struct impUseVolDesc		*udf_iuvd[2];
	struct terminatingDesc		*udf_td[2];
	struct logicalVolIntegrityDesc	*udf_lvid;

	struct sparingTable		*udf_stable[4];

	uint32_t			vat_block;
	uint32_t			*vat;
	uint64_t			vat_entries;

	struct fileSetDesc		*udf_fsd;

	struct udf_extent		*head;
	struct udf_extent		*tail;
};

struct udf_extent
{
	enum udf_space_type		space_type;
	uint32_t			start;
	uint32_t			blocks;

	struct udf_desc			*head;
	struct udf_desc			*tail;

	struct udf_extent		*next;
	struct udf_extent		*prev;
};

struct udf_desc
{
	uint16_t			ident;
	uint32_t			offset;
	uint64_t			length;
	struct udf_data			*data;

	struct udf_desc			*next;
	struct udf_desc			*prev;
};

struct udf_data
{
	uint64_t			length;
	void				*buffer;
	struct udf_data			*next;
	struct udf_data			*prev;
};

#define MBR_PARTITION_NOT_BOOTABLE	0x00
#define MBR_PARTITION_TYPE_IFS		0x07 /* Installable File System (IFS), see: https://serverfault.com/a/829172 */

struct mbr_partition
{
	uint8_t				boot_indicator;
	uint8_t				starting_chs[3];
	uint8_t				partition_type;
	uint8_t				ending_chs[3];
	uint32_t			starting_lba;
	uint32_t			size_in_lba;
} __attribute__ ((packed));

#define MBR_BOOT_SIGNATURE		0xAA55

struct mbr
{
	unsigned char			boot_code[440];
	uint32_t			disk_signature;
	uint16_t			unknown;
	struct mbr_partition		partitions[4];
	uint16_t			boot_signature;
} __attribute__ ((packed));

/* crc.c */
extern uint16_t udf_crc(uint8_t *, uint32_t, uint16_t);

/* extent.c */
struct udf_extent *next_extent(struct udf_extent *, enum udf_space_type);
uint32_t next_extent_size(struct udf_disc *, struct udf_extent *, enum udf_space_type, uint32_t, uint32_t);
uint32_t find_next_extent_size(struct udf_disc *, uint32_t, enum udf_space_type, uint32_t, uint32_t);
struct udf_extent *prev_extent(struct udf_extent *, enum udf_space_type);
uint32_t prev_extent_size(struct udf_extent *, enum udf_space_type, uint32_t, uint32_t);
struct udf_extent *find_extent(struct udf_disc *, uint32_t);
struct udf_extent *set_extent(struct udf_disc *, enum udf_space_type, uint32_t,uint32_t);
struct udf_desc *next_desc(struct udf_desc *, uint16_t);
struct udf_desc *find_desc(struct udf_extent *, uint32_t);
struct udf_desc *set_desc(struct udf_extent *, uint16_t, uint32_t, uint32_t, struct udf_data *);
void append_data(struct udf_desc *, struct udf_data *);
struct udf_data *alloc_data(void *, int);

/* unicode.c */
extern size_t decode_utf8(const dchars *, char *, size_t, size_t);
extern size_t encode_utf8(dchars *, const char *, size_t);
extern size_t decode_locale(const dchars *, char *, size_t, size_t);
extern size_t encode_locale(dchars *, const char *, size_t);
extern size_t decode_string(struct udf_disc *, const dstring *, char *, size_t, size_t);
extern size_t encode_string(struct udf_disc *, dstring *, const char *, size_t);

/* misc.c */
extern const char *appname;
size_t gen_uuid_from_vol_set_ident(char[17], const dstring *, size_t);

#endif /* __LIBUDFFS_H */
