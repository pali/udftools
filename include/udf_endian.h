/*
 * udf_endian.h
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

#ifndef __UDF_ENDIAN_H
#define __UDF_ENDIAN_H

#include "bswap.h"

static inline lb_addr lelb_to_cpu(lb_addr in)
{
	lb_addr out;
	out.logicalBlockNum = le32_to_cpu(in.logicalBlockNum);
	out.partitionReferenceNum = le16_to_cpu(in.partitionReferenceNum);
	return out;
}

static inline lb_addr cpu_to_lelb(lb_addr in)
{
	lb_addr out;
	out.logicalBlockNum = cpu_to_le32(in.logicalBlockNum);
	out.partitionReferenceNum = cpu_to_le16(in.partitionReferenceNum);
	return out;
}

static inline timestamp lets_to_cpu(timestamp in)
{
	timestamp out;
	memcpy(&out, &in, sizeof(timestamp));
	out.typeAndTimezone = le16_to_cpu(in.typeAndTimezone);
	out.year = le16_to_cpu(in.year);
	return out;
}

static inline short_ad lesa_to_cpu(short_ad in)
{
	short_ad out;
	out.extLength = le32_to_cpu(in.extLength);
	out.extPosition = le32_to_cpu(in.extPosition);
	return out;
}

static inline short_ad cpu_to_lesa(short_ad in)
{
	short_ad out;
	out.extLength = cpu_to_le32(in.extLength);
	out.extPosition = cpu_to_le32(in.extPosition);
	return out;
}

static inline long_ad lela_to_cpu(long_ad in)
{
	long_ad out;
	out.extLength = le32_to_cpu(in.extLength);
	out.extLocation = lelb_to_cpu(in.extLocation);
	return out;
}

static inline long_ad cpu_to_lela(long_ad in)
{
	long_ad out;
	out.extLength = cpu_to_le32(in.extLength);
	out.extLocation = cpu_to_lelb(in.extLocation);
	return out;
}

static inline extent_ad leea_to_cpu(extent_ad in)
{
	extent_ad out;
	out.extLength = le32_to_cpu(in.extLength);
	out.extLocation = le32_to_cpu(in.extLocation);
	return out;
}

static inline timestamp cpu_to_lets(timestamp in)
{
	timestamp out;
	memcpy(&out, &in, sizeof(timestamp));
	out.typeAndTimezone = cpu_to_le16(in.typeAndTimezone);
	out.year = cpu_to_le16(in.year);
	return out;
}

#endif /* __UDF_ENDIAN_H */
