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

#include "config.h"

#include <sys/types.h>
#include <string.h>

#ifdef HAVE_SYS_ISA_DEFS_H
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __PDP_ENDIAN 3412

#ifdef _LITTLE_ENDIAN
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#ifdef _BIG_ENDIAN
#define __BYTE_ORDER __BIG_ENDIAN
#endif
#endif

#if __BYTE_ORDER == 0

#error "__BYTE_ORDER must be defined"

#elif __BYTE_ORDER == __BIG_ENDIAN

#define le16_to_cpu(x) (__builtin_constant_p(x) ? \
	((uint16_t)((((uint16_t)(x) & 0x00FFU) << 8) | \
		  (((uint16_t)(x) & 0xFF00U) >> 8))) : \
	__le16_to_cpu(x))
 
#define le32_to_cpu(x) (__builtin_constant_p(x) ? \
	((uint32_t)((((uint32_t)(x) & 0x000000FFU) << 24) | \
		  (((uint32_t)(x) & 0x0000FF00U) <<  8) | \
		  (((uint32_t)(x) & 0x00FF0000U) >>  8) | \
		  (((uint32_t)(x) & 0xFF000000U) >> 24))) : \
	__le32_to_cpu(x))

#define le64_to_cpu(x) (__builtin_constant_p(x) ? \
	((uint64_t)((((uint64_t)(x) & 0x00000000000000FFULL) << 56) | \
		  (((uint64_t)(x) & 0x000000000000FF00ULL) << 40) | \
		  (((uint64_t)(x) & 0x0000000000FF0000ULL) << 24) | \
		  (((uint64_t)(x) & 0x00000000FF000000ULL) <<  8) | \
		  (((uint64_t)(x) & 0x000000FF00000000ULL) >>  8) | \
		  (((uint64_t)(x) & 0x0000FF0000000000ULL) >> 24) | \
		  (((uint64_t)(x) & 0x00FF000000000000ULL) >> 40) | \
		  (((uint64_t)(x) & 0xFF00000000000000ULL) >> 56))) : \
	__le64_to_cpu(x))

static inline uint16_t __le16_to_cpu(uint16_t x)
{
	return ((uint16_t)((((uint16_t)(x) & 0x00FFU) << 8) | \
			   (((uint16_t)(x) & 0xFF00U) >> 8)));
}
 
static inline uint32_t __le32_to_cpu(uint32_t x)
{
	return ((uint32_t)((((uint32_t)(x) & 0x000000FFU) << 24) | \
			   (((uint32_t)(x) & 0x0000FF00U) <<  8) | \
			   (((uint32_t)(x) & 0x00FF0000U) >>  8) | \
			   (((uint32_t)(x) & 0xFF000000U) >> 24)));
}

static inline uint64_t __le64_to_cpu(uint64_t x)
{
	return ((uint64_t)((((uint64_t)(x) & 0x00000000000000FFULL) << 56) | \
			   (((uint64_t)(x) & 0x000000000000FF00ULL) << 40) | \
			   (((uint64_t)(x) & 0x0000000000FF0000ULL) << 24) | \
			   (((uint64_t)(x) & 0x00000000FF000000ULL) <<  8) | \
			   (((uint64_t)(x) & 0x000000FF00000000ULL) >>  8) | \
			   (((uint64_t)(x) & 0x0000FF0000000000ULL) >> 24) | \
			   (((uint64_t)(x) & 0x00FF000000000000ULL) >> 40) | \
			   (((uint64_t)(x) & 0xFF00000000000000ULL) >> 56)));
}

#define cpu_to_le16(x) (le16_to_cpu(x))
#define cpu_to_le32(x) (le32_to_cpu(x))
#define cpu_to_le64(x) (le64_to_cpu(x))

#define le16_to_cpup(x) (__builtin_constant_p(x) ? \
	((uint16_t)(((*(uint16_t *)(x) & 0x00FFU) << 8) | \
		  ((*(uint16_t *)(x) & 0xFF00U) >> 8))) : \
	__le16_to_cpup(x))

#define le32_to_cpup(x) (__builtin_constant_p(x) ? \
	((uint32_t)(((*(uint32_t *)(x) & 0x000000FFU) << 24) | \
		  ((*(uint32_t *)(x) & 0x0000FF00U) <<  8) | \
		  ((*(uint32_t *)(x) & 0x00FF0000U) >>  8) | \
		  ((*(uint32_t *)(x) & 0xFF000000U) >> 24))) : \
	__le32_to_cpup(x))

#define le64_to_cpup(x) (__builtin_constant_p(x) ? \
	((uint64_t)(((*(uint64_t *)(x) & 0x00000000000000FFULL) << 56) | \
		  ((*(uint64_t *)(x) & 0x000000000000FF00ULL) << 40) | \
		  ((*(uint64_t *)(x) & 0x0000000000FF0000ULL) << 24) | \
		  ((*(uint64_t *)(x) & 0x00000000FF000000ULL) <<  8) | \
		  ((*(uint64_t *)(x) & 0x000000FF00000000ULL) >>  8) | \
		  ((*(uint64_t *)(x) & 0x0000FF0000000000ULL) >> 24) | \
		  ((*(uint64_t *)(x) & 0x00FF000000000000ULL) >> 40) | \
		  ((*(uint64_t *)(x) & 0xFF00000000000000ULL) >> 56))) : \
	__le64_to_cpup(x))

static inline uint16_t __le16_to_cpup(uint16_t *x)
{
	return ((uint16_t)(((*(uint16_t *)(x) & 0x00FFU) << 8) | \
			   ((*(uint16_t *)(x) & 0xFF00U) >> 8)));
}

static inline uint32_t __le32_to_cpup(uint32_t *x)
{
	return ((uint32_t)(((*(uint32_t *)(x) & 0x000000FFU) << 24) | \
			   ((*(uint32_t *)(x) & 0x0000FF00U) <<  8) | \
			   ((*(uint32_t *)(x) & 0x00FF0000U) >>  8) | \
			   ((*(uint32_t *)(x) & 0xFF000000U) >> 24)));
}

static inline uint64_t __le64_to_cpup(uint64_t *x)
{
	return ((uint64_t)(((*(uint64_t *)(x) & 0x00000000000000FFULL) << 56) | \
			   ((*(uint64_t *)(x) & 0x000000000000FF00ULL) << 40) | \
			   ((*(uint64_t *)(x) & 0x0000000000FF0000ULL) << 24) | \
			   ((*(uint64_t *)(x) & 0x00000000FF000000ULL) <<  8) | \
			   ((*(uint64_t *)(x) & 0x000000FF00000000ULL) >>  8) | \
			   ((*(uint64_t *)(x) & 0x0000FF0000000000ULL) >> 24) | \
			   ((*(uint64_t *)(x) & 0x00FF000000000000ULL) >> 40) | \
			   ((*(uint64_t *)(x) & 0xFF00000000000000ULL) >> 56)));
}

#define cpu_to_le16p(x) (le16_to_cpup(x))
#define cpu_to_le32p(x) (le32_to_cpup(x))
#define cpu_to_le64p(x) (le64_to_cpup(x))

#else /* __BYTE_ORDER == __LITTLE_ENDIAN */

#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)

#define le16_to_cpup(x) (*(x))
#define le32_to_cpup(x) (*(x))
#define le64_to_cpup(x) (*(x))
#define cpu_to_le16p(x) (*(x))
#define cpu_to_le32p(x) (*(x))
#define cpu_to_le64p(x) (*(x))

#endif /* __BYTE_ORDER == 0 */

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
