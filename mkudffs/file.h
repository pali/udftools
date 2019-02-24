/*
 * libudffs.h
 *
 * Copyright (c) 2001-2002  Ben Fennema
 * Copyright (c) 2016-2018  Pali Rohár <pali.rohar@gmail.com>
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

#ifndef __FILE_H
#define __FILE_H

#include "libudffs.h"

tag query_tag(struct udf_disc *, struct udf_extent *, struct udf_desc *, uint16_t);
extern tag udf_query_tag(struct udf_disc *, uint16_t, uint16_t, uint32_t, struct udf_data *, uint32_t, uint32_t);
extern struct udf_desc *udf_create(struct udf_disc *, struct udf_extent *, const dchars *, uint8_t, uint32_t, struct udf_desc *, uint8_t, uint8_t, uint16_t);
extern struct udf_desc *udf_mkdir(struct udf_disc *, struct udf_extent *, const dchars *, uint8_t, uint32_t, struct udf_desc *);
extern void insert_data(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *desc, struct udf_data *data);
extern void insert_fid(struct udf_disc *, struct udf_extent *, struct udf_desc *, struct udf_desc *, const dchars *, uint8_t, uint8_t);
extern void insert_ea(struct udf_disc *disc, struct udf_desc *desc, struct genericFormat *ea, uint32_t length);
extern int udf_alloc_blocks(struct udf_disc *, struct udf_extent *, uint32_t, uint32_t);

static inline void clear_bits(uint8_t *bitmap, uint32_t offset, uint64_t length)
{
	for (;length>0;length--)
	{
		bitmap[(length+offset-1)/8] &= ~(1 << ((offset+length-1)%8));
	}
}

static inline struct impUseVolDescImpUse *query_iuvdiu(struct udf_disc *disc)
{
	return (struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse;
}

static inline struct logicalVolIntegrityDescImpUse *query_lvidiu(struct udf_disc *disc)
{
	return (struct logicalVolIntegrityDescImpUse *)&(disc->udf_lvid->data[le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps) * 2 * sizeof(uint32_t)]);
}

#endif /* __FILE_H */
