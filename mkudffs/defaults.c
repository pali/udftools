/*
 * defaults.c
 *
 * Copyright (c) 2001-2002  Ben Fennema
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
 * mkudffs default structure initializers
 */

#include "config.h"

#include "mkudffs.h"

int default_media[] = {
	[MEDIA_TYPE_HD] = DEFAULT_HD,
	[MEDIA_TYPE_DVD] = DEFAULT_DVD,
	[MEDIA_TYPE_DVDRAM] = DEFAULT_DVDRAM,
	[MEDIA_TYPE_DVDRW] = DEFAULT_DVDRW,
	[MEDIA_TYPE_DVDR] = DEFAULT_DVDR,
	[MEDIA_TYPE_WORM] = DEFAULT_WORM,
	[MEDIA_TYPE_MO] = DEFAULT_MO,
	[MEDIA_TYPE_CDRW] = DEFAULT_CDRW,
	[MEDIA_TYPE_CDR] = DEFAULT_CDR,
	[MEDIA_TYPE_CD] = DEFAULT_CD,
	[MEDIA_TYPE_BDR] = DEFAULT_BDR,
};

struct udf_sizing default_sizing[][UDF_ALLOC_TYPE_SIZE] =
{
//			align,	numSize,denomSize,minSize
	{ // Media 0 = HD, CDROM, DVDROM, DVDRAM
		{	1,	0,	1,	16	}, // VDS_SIZE		Volume Descriptor Set
		{	1,	0,	1,	1	}, // LVID_SIZE		Logical Volume Integrity Descriptor
		{	1,	0,	1,	0	}, // STABLE_SIZE	Sparing Table
		{	1,	0,	1,	0	}, // SSPACE_SIZE	Sparing Space
		{	1,	0,	1,	0	}, // PSPACE_SIZE	Partition Space
	},
	{ // Media 1 = WORM, MO
		{	1,	0,	1,	255	},
		{	1,	1,	50,	10	},
		{	1,	0,	1,	0	},
		{	1,	0,	1,	0	},
		{	1,	0,	1,	0	},
	},
	{ // Media 2 = CDRW
		{	32,	0,	1,	32	},
		{	32,	0,	1,	32	},
		{	32,	0,	1,	5	}, // 56+8*1024 bytes ~~ 5 blocks
		{	32,	0,	1,	1024	},
		{	32,	0,	1,	0	},
	},
	{ // Media 3 = CDR, BD-R
		{	32,	0,	1,	16	},
		{	32,	0,	1,	1	},
		{	32,	0,	1,	0	},
		{	32,	0,	1,	0	},
		{	32,	0,	1,	0	},
	},
	{ // Media 4 = DVDRW
		{       16,     0,      1,      16      },
		{       16,     0,      1,      16      },
		{       16,     0,      1,      5       }, // 56+8*1024 bytes ~~ 5 blocks
		{       16,     0,      1,      1024    },
		{       16,     0,      1,      0       },
	},
	{ // Media 5 = DVDR
		{	16,	0,	1,	16	},
		{	16,	0,	1,	1	},
		{	16,	0,	1,	0	},
		{	16,	0,	1,	0	},
		{	16,	0,	1,	0	},
	}
};

struct primaryVolDesc default_pvd =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_PVD),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct primaryVolDesc) - sizeof(tag)),
	},
	.volDescSeqNum = constant_cpu_to_le32(1),
	.primaryVolDescNum = constant_cpu_to_le32(0),
	.volIdent = "\x08" "LinuxUDF",
	.volSeqNum = constant_cpu_to_le16(1),
	.maxVolSeqNum = constant_cpu_to_le16(1),
	.interchangeLvl = constant_cpu_to_le16(3),
	.maxInterchangeLvl = constant_cpu_to_le16(3),
	.charSetList = constant_cpu_to_le32(CS0),
	.maxCharSetList = constant_cpu_to_le32(CS0),
	.volSetIdent = "\x08" "0000000000000000LinuxUDF",
	.descCharSet =
	{
		.charSetType = UDF_CHAR_SET_TYPE,
		.charSetInfo = UDF_CHAR_SET_INFO,
	},
	.explanatoryCharSet =
	{
		.charSetType = UDF_CHAR_SET_TYPE,
		.charSetInfo = UDF_CHAR_SET_INFO,
	},
	.appIdent =
	{
		.ident = UDF_ID_APPLICATION,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
	.impIdent =
	{
		.ident = UDF_ID_DEVELOPER,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX
		},
	},
	.flags = constant_cpu_to_le16(PVD_FLAGS_VSID_COMMON),
};

struct logicalVolDesc default_lvd =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_LVD),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct logicalVolDesc) - sizeof(tag)),
	},
	.volDescSeqNum = constant_cpu_to_le32(2),
	.descCharSet =
	{
		.charSetType = UDF_CHAR_SET_TYPE,
		.charSetInfo = UDF_CHAR_SET_INFO,
	},
	.logicalVolIdent = "\x08" "LinuxUDF",
	.logicalBlockSize = constant_cpu_to_le32(2048),
	.domainIdent =
	{
		.ident = UDF_ID_COMPLIANT,
		.identSuffix =
		{
			0x01,
			0x02,
			0x00,
		}
	},
	.impIdent =
	{
		.ident = UDF_ID_DEVELOPER,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	}
};

struct volDescPtr default_vdp =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_VDP),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct volDescPtr) - sizeof(tag)),
	},
	.volDescSeqNum = constant_cpu_to_le32(3),
};

struct impUseVolDescImpUse default_iuvdiu =
{
	.LVICharset =
	{
		.charSetType = UDF_CHAR_SET_TYPE,
		.charSetInfo = UDF_CHAR_SET_INFO
	},
	.logicalVolIdent = "\x08" "LinuxUDF",
	.LVInfo1 = "\x08" "Linux mkudffs " PACKAGE_VERSION,
	.LVInfo2 = "\x08" "Linux udftools " PACKAGE_VERSION,
	.LVInfo3 = "\x08" PACKAGE_URL,
	.impIdent =
	{
		.ident = UDF_ID_DEVELOPER,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
};
	

struct impUseVolDesc default_iuvd =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_IUVD),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct impUseVolDesc) - sizeof(tag)),
	},
	.volDescSeqNum = constant_cpu_to_le32(4),
	.impIdent =
	{
		.ident = UDF_ID_LV_INFO,
		.identSuffix =
		{
			0x01,
			0x02,
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
};

struct partitionDesc default_pd =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_PD),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct partitionDesc) - sizeof(tag)),
	},
	.volDescSeqNum = constant_cpu_to_le32(5),
	.partitionFlags = constant_cpu_to_le16(0x0001),
	.partitionContents =
	{
		.ident = PD_PARTITION_CONTENTS_NSR03,
	},
	.accessType = constant_cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE),
	.impIdent =
	{
		.ident = UDF_ID_DEVELOPER,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
};

struct unallocSpaceDesc default_usd =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_USD),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct unallocSpaceDesc) - sizeof(tag)),
	},
	.volDescSeqNum = constant_cpu_to_le32(6),
};

struct terminatingDesc default_td =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_TD),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct terminatingDesc) - sizeof(tag)),
	},
};

struct logicalVolIntegrityDesc default_lvid =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_LVID),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct logicalVolIntegrityDesc) - sizeof(tag)),
	},
	.integrityType = constant_cpu_to_le32(LVID_INTEGRITY_TYPE_CLOSE),
	.lengthOfImpUse = constant_cpu_to_le32(sizeof(struct logicalVolIntegrityDescImpUse)),
		
};

struct logicalVolIntegrityDescImpUse default_lvidiu =
{
	.impIdent =
	{
		.ident = UDF_ID_DEVELOPER,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX
		},
	},
	.minUDFReadRev = constant_cpu_to_le16(0x0201),
	.minUDFWriteRev = constant_cpu_to_le16(0x0201),
	.maxUDFWriteRev = constant_cpu_to_le16(0x0201),
};

struct sparingTable default_stable =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(0),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct sparingTable) - sizeof(tag)),
	},
	.sparingIdent =
	{
		.flags = 0,
		.ident = UDF_ID_SPARING,
		.identSuffix =
		{
			0x01,
			0x02,
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX
		},
	},
	.reallocationTableLen = constant_cpu_to_le16(0),
	.sequenceNum = constant_cpu_to_le32(0)
};

struct sparablePartitionMap default_sparmap =
{
	.partitionMapType = 2,
	.partitionMapLength = sizeof(struct sparablePartitionMap),
	.partIdent =
	{
		.flags = 0,
		.ident = UDF_ID_SPARABLE,
		.identSuffix =
		{
			0x01,
			0x02,
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX
		},
	},
	.volSeqNum = constant_cpu_to_le16(1),
	.packetLength = constant_cpu_to_le16(32)
};

struct virtualAllocationTable15 default_vat15 =
{
	.vatIdent =
	{
		.flags = 0,
		.ident = UDF_ID_ALLOC,
		.identSuffix =
		{
			0x01,
			0x02,
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX
		},
	},
	.previousVATICBLoc = constant_cpu_to_le32(0xFFFFFFFF)
};

struct virtualAllocationTable20 default_vat20 =
{
	.lengthHeader = constant_cpu_to_le16(152),
	.lengthImpUse = constant_cpu_to_le16(0),
	.logicalVolIdent = "\x08" "LinuxUDF",
	.previousVATICBLoc = constant_cpu_to_le32(0xFFFFFFFF),
	.minUDFReadRev = constant_cpu_to_le16(0x0201),
	.minUDFWriteRev = constant_cpu_to_le16(0x0201),
	.maxUDFWriteRev = constant_cpu_to_le16(0x0201)
};

struct virtualPartitionMap default_virtmap =
{
	.partitionMapType = 2,
	.partitionMapLength = sizeof(struct virtualPartitionMap),
	.partIdent =
	{
		.flags = 0,
		.ident = UDF_ID_VIRTUAL,
		.identSuffix =
		{
			0x01,
			0x02,
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX
		},
	},
	.volSeqNum = constant_cpu_to_le16(1)
};

struct fileSetDesc default_fsd =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_FSD),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct fileSetDesc) - sizeof(tag)),
	},
	.interchangeLvl = constant_cpu_to_le16(3),
	.maxInterchangeLvl = constant_cpu_to_le16(3),
	.charSetList = constant_cpu_to_le32(CS0),
	.maxCharSetList = constant_cpu_to_le32(CS0),
	.logicalVolIdentCharSet =
	{
		.charSetType = UDF_CHAR_SET_TYPE,
		.charSetInfo = UDF_CHAR_SET_INFO,
	},
	.logicalVolIdent = "\x08" "LinuxUDF",
	.fileSetCharSet =
	{
		.charSetType = UDF_CHAR_SET_TYPE,
		.charSetInfo = UDF_CHAR_SET_INFO,
	},
	.fileSetIdent = "\x08" "LinuxUDF",
	.copyrightFileIdent = "\x08" "Copyright",
	.abstractFileIdent = "\x08" "Abstract",
	.domainIdent =
	{
		.ident = UDF_ID_COMPLIANT,
		.identSuffix =
		{
			0x01,
			0x02,
			0x00,
		},
	}
};

struct fileEntry default_fe =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_FE),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct fileEntry) - sizeof(tag)),
	},
	.icbTag =
	{
		.strategyType = constant_cpu_to_le16(4),
		.strategyParameter = constant_cpu_to_le16(0),
		.numEntries = constant_cpu_to_le16(1),
		.fileType = 0,
		.flags = constant_cpu_to_le16(ICBTAG_FLAG_AD_IN_ICB),
	},
	.permissions = constant_cpu_to_le32(FE_PERM_U_DELETE|FE_PERM_U_CHATTR|FE_PERM_U_READ|FE_PERM_U_WRITE|FE_PERM_U_EXEC|FE_PERM_G_READ|FE_PERM_G_EXEC|FE_PERM_O_READ|FE_PERM_O_EXEC),
	.fileLinkCount = constant_cpu_to_le16(0),
	.informationLength = constant_cpu_to_le64(0),
	.logicalBlocksRecorded = constant_cpu_to_le64(0),
	.checkpoint = constant_cpu_to_le32(1),
	.impIdent =
	{
		.ident = UDF_ID_DEVELOPER,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
};

struct extendedFileEntry default_efe =
{
	.descTag =
	{
		.tagIdent = constant_cpu_to_le16(TAG_IDENT_EFE),
		.descVersion = constant_cpu_to_le16(3),
		.tagSerialNum = constant_cpu_to_le16(1),
		.descCRC = constant_cpu_to_le16(sizeof(struct extendedFileEntry) - sizeof(tag)),
	},
	.icbTag =
	{
		.strategyType = constant_cpu_to_le16(4),
		.strategyParameter = constant_cpu_to_le16(0),
		.numEntries = constant_cpu_to_le16(1),
		.fileType = 0,
		.flags = constant_cpu_to_le16(ICBTAG_FLAG_AD_IN_ICB),
	},
	.permissions = constant_cpu_to_le32(FE_PERM_U_DELETE|FE_PERM_U_CHATTR|FE_PERM_U_READ|FE_PERM_U_WRITE|FE_PERM_U_EXEC|FE_PERM_G_READ|FE_PERM_G_EXEC|FE_PERM_O_READ|FE_PERM_O_EXEC),
	.fileLinkCount = constant_cpu_to_le16(0),
	.informationLength = constant_cpu_to_le64(0),
	.objectSize = constant_cpu_to_le64(0),
	.logicalBlocksRecorded = constant_cpu_to_le64(0),
	.checkpoint = constant_cpu_to_le32(1),
	.impIdent =
	{
		.ident = UDF_ID_DEVELOPER,
		.identSuffix =
		{
			UDF_OS_CLASS_UNIX,
			UDF_OS_ID_LINUX,
		},
	},
};

struct mbr default_mbr =
{
	.boot_code = "",
	.disk_signature = constant_cpu_to_le32(0x0),
	.unknown = 0x0,
	.partitions =
	{
		{
			.boot_indicator = MBR_PARTITION_NOT_BOOTABLE,
			.starting_chs = { 0, 0, 0 },
			.partition_type = MBR_PARTITION_TYPE_IFS,
			.ending_chs = { 0, 0, 0 },
			.starting_lba = constant_cpu_to_le32(0),
			.size_in_lba = constant_cpu_to_le32(0),
		},
	},
	.boot_signature = constant_cpu_to_le16(MBR_BOOT_SIGNATURE),
};
