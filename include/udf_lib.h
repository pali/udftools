/*
 * udf_lib.h
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

#ifndef _UDF_LIB_H
#define _UDF_LIB_H 1

#include "ecma_167.h"
#include "osta_udf.h"

#define UDF_EXTENT_LENGTH_MASK	0x3FFFFFFF
#define UDF_EXTENT_FLAG_MASK	0xC0000000

#define UDF_NAME_PAD		4
#define UDF_NAME_LEN		255
#define UDF_PATH_LEN		1023

struct ustr
{
	uint8_t		u_cmpID;
	uint8_t		u_name[UDF_NAME_LEN];
	uint8_t		u_len;
};

#endif /* _UDF_LIB_H */
