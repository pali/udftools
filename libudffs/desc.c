/*
 * desc.c
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

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include "libudffs.h"
#include "config.h"

inline struct impUseVolDescImpUse *query_iuvdiu(struct udf_disc *disc)
{
	return (struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse;
}

inline struct logicalVolIntegrityDescImpUse *query_lvidiu(struct udf_disc *disc)
{
	return (struct logicalVolIntegrityDescImpUse *)&(disc->udf_lvid->impUse[le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps) * 2 * sizeof(uint32_t)]);
}
