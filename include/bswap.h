/*
 * bswap.h
 *
 * Copyright (c) 2001-2002  Ben Fennema
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

#ifndef __BSWAP_H
#define __BSWAP_H

#include "config.h"

#include <stdint.h>
#include <sys/types.h>

#define constant_swap16(x) \
	((uint16_t)((((uint16_t)(x) & 0x00FFU) << 8) | \
		  (((uint16_t)(x) & 0xFF00U) >> 8)))
 
#define constant_swap32(x) \
	((uint32_t)((((uint32_t)(x) & 0x000000FFU) << 24) | \
		  (((uint32_t)(x) & 0x0000FF00U) <<  8) | \
		  (((uint32_t)(x) & 0x00FF0000U) >>  8) | \
		  (((uint32_t)(x) & 0xFF000000U) >> 24)))

#define constant_swap64(x) \
	((uint64_t)((((uint64_t)(x) & 0x00000000000000FFULL) << 56) | \
		  (((uint64_t)(x) & 0x000000000000FF00ULL) << 40) | \
		  (((uint64_t)(x) & 0x0000000000FF0000ULL) << 24) | \
		  (((uint64_t)(x) & 0x00000000FF000000ULL) <<  8) | \
		  (((uint64_t)(x) & 0x000000FF00000000ULL) >>  8) | \
		  (((uint64_t)(x) & 0x0000FF0000000000ULL) >> 24) | \
		  (((uint64_t)(x) & 0x00FF000000000000ULL) >> 40) | \
		  (((uint64_t)(x) & 0xFF00000000000000ULL) >> 56)))

static inline uint16_t swap16(uint16_t x)
{
	return ((uint16_t)((((uint16_t)(x) & 0x00FFU) << 8) | \
			   (((uint16_t)(x) & 0xFF00U) >> 8)));
}
 
static inline uint32_t swap32(uint32_t x)
{
	return ((uint32_t)((((uint32_t)(x) & 0x000000FFU) << 24) | \
			   (((uint32_t)(x) & 0x0000FF00U) <<  8) | \
			   (((uint32_t)(x) & 0x00FF0000U) >>  8) | \
			   (((uint32_t)(x) & 0xFF000000U) >> 24)));
}

static inline uint64_t swap64(uint64_t x)
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

#define constant_swap16p(x) \
	((uint16_t)(((*(uint16_t *)(x) & 0x00FFU) << 8) | \
		  ((*(uint16_t *)(x) & 0xFF00U) >> 8)))

#define constant_swap32p(x) \
	((uint32_t)(((*(uint32_t *)(x) & 0x000000FFU) << 24) | \
		  ((*(uint32_t *)(x) & 0x0000FF00U) <<  8) | \
		  ((*(uint32_t *)(x) & 0x00FF0000U) >>  8) | \
		  ((*(uint32_t *)(x) & 0xFF000000U) >> 24)))

#define constant_swap64p(x) \
	((uint64_t)(((*(uint64_t *)(x) & 0x00000000000000FFULL) << 56) | \
		  ((*(uint64_t *)(x) & 0x000000000000FF00ULL) << 40) | \
		  ((*(uint64_t *)(x) & 0x0000000000FF0000ULL) << 24) | \
		  ((*(uint64_t *)(x) & 0x00000000FF000000ULL) <<  8) | \
		  ((*(uint64_t *)(x) & 0x000000FF00000000ULL) >>  8) | \
		  ((*(uint64_t *)(x) & 0x0000FF0000000000ULL) >> 24) | \
		  ((*(uint64_t *)(x) & 0x00FF000000000000ULL) >> 40) | \
		  ((*(uint64_t *)(x) & 0xFF00000000000000ULL) >> 56)))


static inline uint16_t swap16p(uint16_t *x)
{
	return ((uint16_t)(((*(uint16_t *)(x) & 0x00FFU) << 8) | \
			   ((*(uint16_t *)(x) & 0xFF00U) >> 8)));
}

static inline uint32_t swap32p(uint32_t *x)
{
	return ((uint32_t)(((*(uint32_t *)(x) & 0x000000FFU) << 24) | \
			   ((*(uint32_t *)(x) & 0x0000FF00U) <<  8) | \
			   ((*(uint32_t *)(x) & 0x00FF0000U) >>  8) | \
			   ((*(uint32_t *)(x) & 0xFF000000U) >> 24)));
}

static inline uint64_t swap64p(uint64_t *x)
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

#ifdef WORDS_BIGENDIAN

#define le16_to_cpu(x) (__builtin_constant_p(x) ? \
			constant_swap16(x) : \
			swap16(x))
 
#define le32_to_cpu(x) (__builtin_constant_p(x) ? \
			constant_swap32(x) : \
			swap32(x))

#define le64_to_cpu(x) (__builtin_constant_p(x) ? \
			constant_swap64(x) : \
			swap64(x))


#define constant_le16_to_cpu(x) constant_swap16((x))
#define constant_le32_to_cpu(x) constant_swap32((x))
#define constant_le64_to_cpu(x) constant_swap64((x))

#define le16_to_cpup(x) (__builtin_constant_p(x) ? \
			constant_swap16p(x) : \
			swap16p(x))

#define le32_to_cpup(x) (__builtin_constant_p(x) ? \
			constant_swap32p(x) : \
			swap32p(x))

#define le64_to_cpup(x) (__builtin_constant_p(x) ? \
			constant_swap64p(x) : \
			swap64p(x))

#define constant_le16_to_cpup(x) constant_swap16p((x))
#define constant_le32_to_cpup(x) constant_swap32p((x))
#define constant_le64_to_cpup(x) constant_swap64p((x))

#define be16_to_cpu(x) ((uint16_t)(x))
#define be32_to_cpu(x) ((uint32_t)(x))
#define be64_to_cpu(x) ((uint64_t)(x))

#define constant_be16_to_cpu(x) ((uint16_t)(x))
#define constant_be32_to_cpu(x) ((uint32_t)(x))
#define constant_be64_to_cpu(x) ((uint64_t)(x))

#define be16_to_cpup(x) (*(uint16_t *)(x))
#define be32_to_cpup(x) (*(uint32_t *)(x))
#define be64_to_cpup(x) (*(uint64_t *)(x))

#define constant_be16_to_cpup(x) (*(uint16_t *)(x))
#define constant_be32_to_cpup(x) (*(uint32_t *)(x))
#define constant_be64_to_cpup(x) (*(uint64_t *)(x))

#else /* WORDS_BIGENDIAN */

#define le16_to_cpu(x) ((uint16_t)(x))
#define le32_to_cpu(x) ((uint32_t)(x))
#define le64_to_cpu(x) ((uint64_t)(x))

#define constant_le16_to_cpu(x) ((uint16_t)(x))
#define constant_le32_to_cpu(x) ((uint32_t)(x))
#define constant_le64_to_cpu(x) ((uint64_t)(x))

#define le16_to_cpup(x) (*(uint16_t *)(x))
#define le32_to_cpup(x) (*(uint32_t *)(x))
#define le64_to_cpup(x) (*(uint64_t *)(x))

#define constant_le16_to_cpup(x) (*(uint16_t *)(x))
#define constant_le32_to_cpup(x) (*(uint32_t *)(x))
#define constant_le64_to_cpup(x) (*(uint64_t *)(x))

#define be16_to_cpu(x) (__builtin_constant_p(x) ? \
			constant_swap16(x) : \
			swap16(x))
 
#define be32_to_cpu(x) (__builtin_constant_p(x) ? \
			constant_swap32(x) : \
			swap32(x))

#define be64_to_cpu(x) (__builtin_constant_p(x) ? \
			constant_swap64(x) : \
			swap64(x))


#define constant_be16_to_cpu(x) constant_swap16((x))
#define constant_be32_to_cpu(x) constant_swap32((x))
#define constant_be64_to_cpu(x) constant_swap64((x))

#define be16_to_cpup(x) (__builtin_constant_p(x) ? \
			constant_swap16p(x) : \
			swap16p(x))

#define be32_to_cpup(x) (__builtin_constant_p(x) ? \
			constant_swap32p(x) : \
			swap32p(x))

#define be64_to_cpup(x) (__builtin_constant_p(x) ? \
			constant_swap64p(x) : \
			swap64p(x))

#define constant_be16_to_cpup(x) constant_swap16p((x))
#define constant_be32_to_cpup(x) constant_swap32p((x))
#define constant_be64_to_cpup(x) constant_swap64p((x))

#endif /* WORDS_BIGENDIAN */

#define cpu_to_le16(x) le16_to_cpu((x))
#define cpu_to_le32(x) le32_to_cpu((x))
#define cpu_to_le64(x) le64_to_cpu((x))

#define constant_cpu_to_le16(x) constant_le16_to_cpu((x))
#define constant_cpu_to_le32(x) constant_le32_to_cpu((x))
#define constant_cpu_to_le64(x) constant_le64_to_cpu((x))

#define cpu_to_le16p(x) le16_to_cpup((x))
#define cpu_to_le32p(x) le32_to_cpup((x))
#define cpu_to_le64p(x) le64_to_cpup((x))

#define constant_cpu_to_le16p(x) constant_le16_to_cpup((x))
#define constant_cpu_to_le32p(x) constant_le32_to_cpup((x))
#define constant_cpu_to_le64p(x) constant_le64_to_cpup((x))

#define cpu_to_be16(x) be16_to_cpu((x))
#define cpu_to_be32(x) be32_to_cpu((x))
#define cpu_to_be64(x) be64_to_cpu((x))

#define constant_cpu_to_be16(x) constant_be16_to_cpu((x))
#define constant_cpu_to_be32(x) constant_be32_to_cpu((x))
#define constant_cpu_to_be64(x) constant_be64_to_cpu((x))

#define cpu_to_be16p(x) be16_to_cpup((x))
#define cpu_to_be32p(x) be32_to_cpup((x))
#define cpu_to_be64p(x) be64_to_cpup((x))

#define constant_cpu_to_be16p(x) constant_be16_to_cpup((x))
#define constant_cpu_to_be32p(x) constant_be32_to_cpup((x))
#define constant_cpu_to_be64p(x) constant_be64_to_cpup((x))

#endif /* __BSWAP_H */
