/*
 * defaults.c
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

#include "../mkudffs/mkudffs.h"

struct fileEntry default_fe =
{
	descTag :
	{
		tagIdent : constant_cpu_to_le16(TAG_IDENT_LVID),
		descVersion : constant_cpu_to_le16(3),
		tagSerialNum : 1,
		descCRC : constant_cpu_to_le16(sizeof(struct primaryVolDesc) - sizeof(tag)),
	},
	icbTag :
	{
		strategyType : constant_cpu_to_le16(4),
		strategyParameter : constant_cpu_to_le16(0),
		numEntries : constant_cpu_to_le16(1),
		fileType : 0,
		flags : constant_cpu_to_le16(ICBTAG_FLAG_AD_IN_ICB),
	},
	permissions : constant_cpu_to_le32(FE_PERM_U_DELETE|FE_PERM_U_CHATTR|FE_PERM_U_READ|FE_PERM_U_WRITE|FE_PERM_U_EXEC|FE_PERM_G_READ|FE_PERM_G_EXEC|FE_PERM_O_READ|FE_PERM_O_EXEC),
	fileLinkCount : constant_cpu_to_le16(0),
	informationLength : constant_cpu_to_le64(0),
	logicalBlocksRecorded : constant_cpu_to_le64(0),
	impIdent :
	{
		ident : UDF_ID_DEVELOPER,
		identSuffix :
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
};

struct extendedFileEntry default_efe =
{
	descTag :
	{
		tagIdent : constant_cpu_to_le16(TAG_IDENT_LVID),
		descVersion : constant_cpu_to_le16(3),
		tagSerialNum : 1,
		descCRC : constant_cpu_to_le16(sizeof(struct primaryVolDesc) - sizeof(tag)),
	},
	icbTag :
	{
		strategyType : constant_cpu_to_le16(4),
		strategyParameter : constant_cpu_to_le16(0),
		numEntries : constant_cpu_to_le16(1),
		fileType : 0,
		flags : constant_cpu_to_le16(ICBTAG_FLAG_AD_IN_ICB),
	},
	permissions : constant_cpu_to_le32(FE_PERM_U_DELETE|FE_PERM_U_CHATTR|FE_PERM_U_READ|FE_PERM_U_WRITE|FE_PERM_U_EXEC|FE_PERM_G_READ|FE_PERM_G_EXEC|FE_PERM_O_READ|FE_PERM_O_EXEC),
	fileLinkCount : constant_cpu_to_le16(0),
	informationLength : constant_cpu_to_le64(0),
	objectSize : constant_cpu_to_le64(0),
	logicalBlocksRecorded : constant_cpu_to_le64(0),
	impIdent :
	{
		ident : UDF_ID_DEVELOPER,
		identSuffix :
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
};
