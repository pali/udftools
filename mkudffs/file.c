/*
 * file.c
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

/**
 * @file
 * libudffs file and directory handling functions
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "libudffs.h"
#include "file.h"
#include "defaults.h"

/**
 * For a more detailed discussion of partition and extends see the comments
 * at the top of extent.c in this directory.
 *
 * An information control block (ICB) is an on-disc structure used to
 * store information about files such as allocation descriptors. The
 * structure of the ICB hierarchy can be quite complex for write-once
 * media which use various strategies of linked lists to preserve multiple
 * historial file versions. Strategy type 4 is normally used on rewritable
 * media where an ICB containing 'direct entries' can be updated as needed.
 *
 * Every file has a file entry/extended file entry (tag:FE/EFE) udf_descriptor
 * which is in the root of the ICB hierarchy for the file. For normal files the
 * FE/EFE can be thought of as the 'inode' for the file as it contains the
 * location of the on-disc extents of the file. Small amounts of data can be
 * recorded directly in the allocation descriptor area of the FE/EFE ICB if it
 * is deemed useful. This 'INICB' feature is used for directories with strategy
 * type 4.
 *
 * A file identifier descriptor (tag:FID) udf_descriptor is a 40+ byte structure
 * recorded in a directory file to describe the parent directory and any files
 * or subdirectories. The unnamed parent entry is always recorded first and the
 * parent of the root directory is the root directory, generally referred to
 * as '<root>'. All other files/directories must have a non-zero length name.
 * Along with the name and attributes of the file a FID contains the location
 * of the file FE/EFE 'inode' ICB. While not required, this may be recorded
 * adjacent to the file data on-disc for convenience.
 *
 * The notation tag:FE/EFE means a udf_descriptor with an ident of either
 * TAG_IDENT_FE or TAG_IDENT_EFE with the on-disc format FE/EFE structure
 * stored in the first udf_data item on the udf_data list of that udf_descriptor.
 * The 16-byte on-disc format tag will be at the beginning of that structure.
 * Additional information such as the FID structure for directory entries will
 * be stored in subsequent entries on the list.
 */

/**
 * @brief create an on-disc format tag for a udf_descriptor of a
 *        udf_extent with any space_type. For type:PSPACE the block
 *        number will be relative to to the first block of the partition,
 *        otherwise it will be relative to the first block of the media
 * @param disc the udf_disc
 * @param ext the udf_extent containing the udf_descriptor
 * @param desc the udf_descriptor
 * @param SerialNum the serial number to assign
 * @return the tag
 */
tag query_tag(struct udf_disc *disc, struct udf_extent *ext, struct udf_desc *desc, uint16_t SerialNum)
{
	tag ret;
	int i;
	struct udf_data *data;
	uint16_t crc = 0;
	int offset = sizeof(tag);

	ret.tagIdent = cpu_to_le16(desc->ident);
	if (disc->udf_rev >= 0x0200)
		ret.descVersion = cpu_to_le16(3);
	else
		ret.descVersion = cpu_to_le16(2);
	ret.tagChecksum = 0;
	ret.reserved = 0;
	ret.tagSerialNum = cpu_to_le16(SerialNum);
	ret.descCRCLength = cpu_to_le16(desc->length - sizeof(tag));
	data = desc->data;
	while (data != NULL)
	{
		crc = udf_crc((uint8_t *)data->buffer + offset, data->length - offset, crc);
		offset = 0;
		data = data->next;
	}
	ret.descCRC = cpu_to_le16(crc);
	if (ext->space_type & PSPACE)
		ret.tagLocation = cpu_to_le32(desc->offset);
	else
		ret.tagLocation = cpu_to_le32(ext->start + desc->offset);
	for (i=0; i<16; i++)
		if (i != 4)
			ret.tagChecksum += (uint8_t)(((char *)&ret)[i]);

	return ret;
}

/**
 * @brief create an on-disc format tag from udf_descriptor components
 * @param disc the udf_disc
 * @param Ident the tag:Ident to assign
 * @param SerialNum the serial number to assign
 * @param Location the block number of the on-disc tag:Ident udf_descriptor
 * @param data the udf_data list head
 * @param skip the skip data length
 * @param length the summed data length
 * @return the tag
 */
tag udf_query_tag(struct udf_disc *disc, uint16_t Ident, uint16_t SerialNum, uint32_t Location, struct udf_data *data, uint32_t skip, uint32_t length)
{
	tag ret;
	int i;
	uint16_t crc = 0;
	uint32_t clength;
	uint32_t offset = sizeof(tag);

	ret.tagIdent = cpu_to_le16(Ident);
	if (disc->udf_rev >= 0x0200)
		ret.descVersion = cpu_to_le16(3);
	else
		ret.descVersion = cpu_to_le16(2);
	ret.tagChecksum = 0;
	ret.reserved = 0;
	ret.tagSerialNum = cpu_to_le16(SerialNum);
	ret.descCRCLength = cpu_to_le16(length - sizeof(tag));
	while (data != NULL && length)
	{
		if ((clength = data->length) > length)
			clength = length;
		crc = udf_crc((uint8_t *)data->buffer + skip + offset, clength - offset, crc);
		length -= clength;
		offset = 0;
		skip = 0;
		data = data->next;
	}
	ret.descCRC = cpu_to_le16(crc);
	ret.tagLocation = cpu_to_le32(Location);
	for (i=0; i<16; i++)
		if (i != 4)
			ret.tagChecksum += (uint8_t)(((char *)&ret)[i]);

	return ret;
}

/**
 * @brief append a udf_data item containing a FID in the payload to the
 *        udf_data list for a directory tag:FE/EFE udf_descriptor
 * @param disc the udf_disc
 * @param pspace the type:PSPACE udf_extent for on-disc allocations
 * @param desc the file tag:FE/EFE udf_descriptor (for offset)
 * @param parent the directory tag:FE/EFE udf_descriptor
 * @param data the udf_data item containing the FID
 * @return the block number of the on-disc tag:FID udf_desciptor
 */
int insert_desc(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *desc, struct udf_desc *parent, struct udf_data *data)
{
	uint32_t block = 0;

	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe = (struct extendedFileEntry *)parent->data->buffer;

		if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB)
		{
			block = parent->offset;
			append_data(parent, data);
			efe->lengthAllocDescs = cpu_to_le32(le32_to_cpu(efe->lengthAllocDescs) + data->length);
		}
		else
		{
			struct udf_desc *fiddesc = NULL;

			if (le32_to_cpu(efe->lengthAllocDescs) == 0)
			{
				block = udf_alloc_blocks(disc, pspace, desc->offset, 1);
				fiddesc = set_desc(pspace, TAG_IDENT_FID, block, data->length, data);
				if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
				{
					short_ad *sad;

					parent->length += sizeof(short_ad);
					parent->data->length += sizeof(short_ad);
					parent->data->buffer = realloc(parent->data->buffer, parent->length);
					efe = (struct extendedFileEntry *)parent->data->buffer;
					sad = (short_ad *)&efe->extendedAttrAndAllocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs)];
					sad->extPosition = cpu_to_le32(block);
					sad->extLength = cpu_to_le32(data->length);
					efe->lengthAllocDescs = cpu_to_le32(sizeof(short_ad));
				}
				else if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG)
				{
					long_ad *lad;

					parent->length += sizeof(long_ad);
					parent->data->length += sizeof(long_ad);
					parent->data->buffer = realloc(parent->data->buffer, parent->length);
					efe = (struct extendedFileEntry *)parent->data->buffer;
					lad = (long_ad *)&efe->extendedAttrAndAllocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs)];
					lad->extLocation.logicalBlockNum = cpu_to_le32(block);
					lad->extLocation.partitionReferenceNum = cpu_to_le16(0);
					lad->extLength = cpu_to_le32(data->length);
					efe->lengthAllocDescs = cpu_to_le32(sizeof(long_ad));
				}
				efe->logicalBlocksRecorded = cpu_to_le32(1);
			}
			else
			{
				if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
				{
					short_ad *sad;

					sad = (short_ad *)&efe->extendedAttrAndAllocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs) - sizeof(short_ad)];
					fiddesc = find_desc(pspace, le32_to_cpu(sad->extPosition));
					block = fiddesc->offset;
					append_data(fiddesc, data);
					sad->extLength = cpu_to_le32(le32_to_cpu(sad->extLength) + data->length);
				}
				else if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG)
				{
					long_ad *lad;

					lad = (long_ad *)&efe->extendedAttrAndAllocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs) - sizeof(long_ad)];
					fiddesc = find_desc(pspace, le32_to_cpu(lad->extLocation.logicalBlockNum));
					block = fiddesc->offset;
					append_data(fiddesc, data);
					lad->extLength = cpu_to_le32(le32_to_cpu(lad->extLength) + data->length);
				}
			}
		}
	}
	else
	{
		struct fileEntry *fe = (struct fileEntry *)parent->data->buffer;

		if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB)
		{
			block = parent->offset;
			append_data(parent, data);
			fe->lengthAllocDescs = cpu_to_le32(le32_to_cpu(fe->lengthAllocDescs) + data->length);
		}
		else
		{
			struct udf_desc *fiddesc = NULL;

			if (le32_to_cpu(fe->lengthAllocDescs) == 0)
			{
				block = udf_alloc_blocks(disc, pspace, desc->offset, 1);
				fiddesc = set_desc(pspace, TAG_IDENT_FID, block, data->length, data);
				if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
				{
					short_ad *sad;

					parent->length += sizeof(short_ad);
					parent->data->length += sizeof(short_ad);
					parent->data->buffer = realloc(parent->data->buffer, parent->length);
					fe = (struct fileEntry *)parent->data->buffer;
					sad = (short_ad *)&fe->extendedAttrAndAllocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs)];
					sad->extPosition = cpu_to_le32(block);
					sad->extLength = cpu_to_le32(data->length);
					fe->lengthAllocDescs = cpu_to_le32(sizeof(short_ad));
				}
				else if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG)
				{
					long_ad *lad;

					parent->length += sizeof(long_ad);
					parent->data->length += sizeof(long_ad);
					parent->data->buffer = realloc(parent->data->buffer, parent->length);
					fe = (struct fileEntry *)parent->data->buffer;
					lad = (long_ad *)&fe->extendedAttrAndAllocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs)];
					lad->extLocation.logicalBlockNum = cpu_to_le32(block);
					lad->extLocation.partitionReferenceNum = cpu_to_le16(0);
					lad->extLength = cpu_to_le32(data->length);
					fe->lengthAllocDescs = cpu_to_le32(sizeof(long_ad));
				}
				fe->logicalBlocksRecorded = cpu_to_le32(1);
			}
			else
			{
				if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
				{
					short_ad *sad;

					sad = (short_ad *)&fe->extendedAttrAndAllocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs) - sizeof(short_ad)];
					fiddesc = find_desc(pspace, le32_to_cpu(sad->extPosition));
					block = fiddesc->offset;
					append_data(fiddesc, data);
					sad->extLength = cpu_to_le32(le32_to_cpu(sad->extLength) + data->length);
				}
				else if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG)
				{
					long_ad *lad;

					lad = (long_ad *)&fe->extendedAttrAndAllocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs) - sizeof(long_ad)];
					fiddesc = find_desc(pspace, le32_to_cpu(lad->extLocation.logicalBlockNum));
					block = fiddesc->offset;
					append_data(fiddesc, data);
					lad->extLength = cpu_to_le32(le32_to_cpu(lad->extLength) + data->length);
				}
			}
		}
	}
	return block;
}

/**
 * @brief append a udf_data list to a (hidden) VAT file ???
 * @param disc the udf_disc
 * @param pspace the type:PSPACE udf_extent for on-disc allocations
 * @param desc the file tag:FE/EFE udf_descriptor of a VAT file
 * @param data the udf_data list head
 * @return void
 */
void insert_data(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *desc, struct udf_data *data)
{
	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe = (struct extendedFileEntry *)desc->data->buffer;

		if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB)
		{
			append_data(desc, data);
			efe->lengthAllocDescs = cpu_to_le32(le32_to_cpu(efe->lengthAllocDescs) + data->length);
			efe->informationLength = cpu_to_le64(le64_to_cpu(efe->informationLength) + data->length);
			efe->objectSize = cpu_to_le64(le64_to_cpu(efe->objectSize) + data->length);
		}
		else
		{
			fprintf(stderr, "%s: Error: Cannot insert data when inicb is not used\n", appname);
			exit(1);
		}
	}
	else
	{
		struct fileEntry *fe = (struct fileEntry *)desc->data->buffer;

		if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB)
		{
			append_data(desc, data);
			fe->lengthAllocDescs = cpu_to_le32(le32_to_cpu(fe->lengthAllocDescs) + data->length);
			fe->informationLength = cpu_to_le64(le64_to_cpu(fe->informationLength) + data->length);
		}
		else
		{
			fprintf(stderr, "%s: Error: Cannot insert data when inicb is not used\n", appname);
			exit(1);
		}
	}

	*(tag *)desc->data->buffer = query_tag(disc, pspace, desc, 1);
}

/**
 * @brief helper function to compute tag:FID udf_descriptor size and padding
 *        to a multiple of 4 bytes
 * @param length the length of the file name in bytes
 * @return the length of the required memory allocation
 */
uint32_t compute_ident_length(uint32_t length)
{
	return length + (4 - (length % 4)) %4;
}

/**
 * @brief create a FID and add it to a directory then increment the file
 *        link count
 * @param disc the udf_disc
 * @param pspace the type:PSPACE udf_extent for on-disc allocations
 * @param desc the file tag:FE/EFE udf_descriptor
 * @param parent the directory tag:FE/EFE udf_descriptor
 * @param name the file name in OSTA Compressed Unicode format (d-characters)
 * @param length the length of the file name in bytes
 * @param filechar the file characteristics
 * @return void
 */
void insert_fid(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *desc, struct udf_desc *parent, const dchars *name, uint8_t length, uint8_t filechar)
{
	struct udf_data *data;
	struct fileIdentDesc *fid;
	struct allocDescImpUse *adiu;
	int ilength = compute_ident_length(sizeof(struct fileIdentDesc) + length);
	int offset;
	uint64_t uniqueID;
	uint32_t uniqueID_le32;

	data = alloc_data(NULL, ilength);
	fid = data->buffer;

	offset = insert_desc(disc, pspace, desc, parent, data);
	fid->descTag.tagLocation = cpu_to_le32(offset);

	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe = (struct extendedFileEntry *)desc->data->buffer;

		efe->fileLinkCount = cpu_to_le16(le16_to_cpu(efe->fileLinkCount) + 1);
		uniqueID = le64_to_cpu(efe->uniqueID);

		efe = (struct extendedFileEntry *)parent->data->buffer;

		if (disc->flags & FLAG_STRATEGY4096)
			fid->icb.extLength = cpu_to_le32(disc->blocksize * 2);
		else
			fid->icb.extLength = cpu_to_le32(disc->blocksize);
		fid->icb.extLocation.logicalBlockNum = cpu_to_le32(desc->offset);
		if (disc->flags & FLAG_VAT)
			fid->icb.extLocation.partitionReferenceNum = cpu_to_le16(1);
		else
			fid->icb.extLocation.partitionReferenceNum = cpu_to_le16(0);

		uniqueID_le32 = cpu_to_le32(uniqueID & 0x00000000FFFFFFFFUL);
		adiu = (struct allocDescImpUse *)fid->icb.impUse;
		memcpy(adiu->impUse, &uniqueID_le32, sizeof(uniqueID_le32));

		fid->fileVersionNum = cpu_to_le16(1);
		fid->fileCharacteristics = filechar;
		fid->lengthFileIdent = length;
		fid->lengthOfImpUse = cpu_to_le16(0);
		memcpy(fid->impUseAndFileIdent, name, length);
		fid->descTag = udf_query_tag(disc, TAG_IDENT_FID, 1, le32_to_cpu(fid->descTag.tagLocation), data, 0, ilength);

		efe->informationLength = cpu_to_le64(le64_to_cpu(efe->informationLength) + ilength);
		efe->objectSize = cpu_to_le64(le64_to_cpu(efe->objectSize) + ilength);
	}
	else
	{
		struct fileEntry *fe = (struct fileEntry *)desc->data->buffer;

		fe->fileLinkCount = cpu_to_le16(le16_to_cpu(fe->fileLinkCount) + 1);
		uniqueID = le64_to_cpu(fe->uniqueID);

		fe = (struct fileEntry *)parent->data->buffer;

		if (disc->flags & FLAG_STRATEGY4096)
			fid->icb.extLength = cpu_to_le32(disc->blocksize * 2);
		else
			fid->icb.extLength = cpu_to_le32(disc->blocksize);
		fid->icb.extLocation.logicalBlockNum = cpu_to_le32(desc->offset);
		if (disc->flags & FLAG_VAT)
			fid->icb.extLocation.partitionReferenceNum = cpu_to_le16(1);
		else
			fid->icb.extLocation.partitionReferenceNum = cpu_to_le16(0);

		uniqueID_le32 = cpu_to_le32(uniqueID & 0x00000000FFFFFFFFUL);
		adiu = (struct allocDescImpUse *)fid->icb.impUse;
		memcpy(adiu->impUse, &uniqueID_le32, sizeof(uniqueID_le32));

		fid->fileVersionNum = cpu_to_le16(1);
		fid->fileCharacteristics = filechar;
		fid->lengthFileIdent = length;
		fid->lengthOfImpUse = cpu_to_le16(0);
		memcpy(fid->impUseAndFileIdent, name, length);
		fid->descTag = udf_query_tag(disc, TAG_IDENT_FID, 1, le32_to_cpu(fid->descTag.tagLocation), data, 0, ilength);
		fe->informationLength = cpu_to_le64(le64_to_cpu(fe->informationLength) + ilength);
	}
	*(tag *)desc->data->buffer = query_tag(disc, pspace, desc, 1);
	*(tag *)parent->data->buffer = query_tag(disc, pspace, parent, 1);
}

void insert_ea(struct udf_disc *disc, struct udf_desc *desc, struct genericFormat *ea, uint32_t length)
{
	struct extendedAttrHeaderDesc *ea_hdr;
	struct extendedFileEntry *efe = NULL;
	struct fileEntry *fe = NULL;
	uint8_t *extendedAttr;
	uint32_t lengthExtendedAttr;
	uint32_t location = 0;

#define UPDATE_PTR                                                            \
	do {                                                                  \
		if (disc->flags & FLAG_EFE)                                   \
		{                                                             \
			efe = (struct extendedFileEntry *)desc->data->buffer; \
			lengthExtendedAttr = efe->lengthExtendedAttr;         \
			extendedAttr = efe->extendedAttrAndAllocDescs;        \
		}                                                             \
		else                                                          \
		{                                                             \
			fe = (struct fileEntry *)desc->data->buffer;          \
			lengthExtendedAttr = fe->lengthExtendedAttr;          \
			extendedAttr = fe->extendedAttrAndAllocDescs;         \
		}                                                             \
		ea_hdr = (struct extendedAttrHeaderDesc *)extendedAttr;       \
	} while ( 0 )

	UPDATE_PTR;

	if (le32_to_cpu(lengthExtendedAttr) && le32_to_cpu(ea_hdr->impAttrLocation) > le32_to_cpu(lengthExtendedAttr))
		ea_hdr->impAttrLocation = cpu_to_le32(0xFFFFFFFF);
	if (le32_to_cpu(lengthExtendedAttr) && le32_to_cpu(ea_hdr->appAttrLocation) > le32_to_cpu(lengthExtendedAttr))
		ea_hdr->appAttrLocation = cpu_to_le32(0xFFFFFFFF);

	if (!le32_to_cpu(lengthExtendedAttr))
	{
		desc->length += sizeof(*ea_hdr);
		desc->data->length += sizeof(*ea_hdr);
		desc->data->buffer = realloc(desc->data->buffer, desc->data->length);

		UPDATE_PTR;

		if (disc->flags & FLAG_EFE)
		{
			lengthExtendedAttr = efe->lengthExtendedAttr = cpu_to_le32(sizeof(*ea_hdr));
			if (le32_to_cpu(efe->lengthAllocDescs))
				memmove(&efe->extendedAttrAndAllocDescs[sizeof(*ea_hdr)], efe->extendedAttrAndAllocDescs, le32_to_cpu(efe->lengthAllocDescs));
		}
		else
		{
			lengthExtendedAttr = fe->lengthExtendedAttr = cpu_to_le32(sizeof(*ea_hdr));
			if (le32_to_cpu(fe->lengthAllocDescs))
				memmove(&fe->extendedAttrAndAllocDescs[sizeof(*ea_hdr)], fe->extendedAttrAndAllocDescs, le32_to_cpu(fe->lengthAllocDescs));
		}

		ea_hdr->impAttrLocation = cpu_to_le32(0xFFFFFFFF);
		ea_hdr->appAttrLocation = cpu_to_le32(0xFFFFFFFF);
	}

	if (le32_to_cpu(ea->attrType) < 2048)
	{
		desc->length += length;
		desc->data->length += length;
		desc->data->buffer = realloc(desc->data->buffer, desc->data->length);

		UPDATE_PTR;

		if (le32_to_cpu(ea_hdr->appAttrLocation) != 0xFFFFFFFF)
		{
			location = le32_to_cpu(ea_hdr->appAttrLocation);
			memmove(&extendedAttr[location+length], &extendedAttr[location], le32_to_cpu(lengthExtendedAttr) - location);
			ea_hdr->appAttrLocation = cpu_to_le32(location+length);
		}

		if (le32_to_cpu(ea_hdr->impAttrLocation) != 0xFFFFFFFF)
		{
			location = le32_to_cpu(ea_hdr->impAttrLocation);
			if (le32_to_cpu(ea_hdr->appAttrLocation) == 0xFFFFFFFF)
				memmove(&extendedAttr[location+length], &extendedAttr[location], le32_to_cpu(lengthExtendedAttr) - location);
			else
				memmove(&extendedAttr[location+length], &extendedAttr[location], le32_to_cpu(ea_hdr->appAttrLocation) - location);
			ea_hdr->impAttrLocation = cpu_to_le32(location+length);
		}
		else if (le32_to_cpu(ea_hdr->appAttrLocation) == 0xFFFFFFFF)
		{
			location = le32_to_cpu(lengthExtendedAttr);
		}

		memcpy(&extendedAttr[location], ea, length);
		lengthExtendedAttr = cpu_to_le32(le32_to_cpu(lengthExtendedAttr) + length);
		if (disc->flags & FLAG_EFE)
			efe->lengthExtendedAttr = lengthExtendedAttr;
		else
			fe->lengthExtendedAttr = lengthExtendedAttr;
	}
	else if (le32_to_cpu(ea->attrType) < 65536)
	{
		desc->length += length;
		desc->data->length += length;
		desc->data->buffer = realloc(desc->data->buffer, desc->data->length);

		UPDATE_PTR;

		if (le32_to_cpu(ea_hdr->appAttrLocation) != 0xFFFFFFFF)
		{
			location = le32_to_cpu(ea_hdr->appAttrLocation);
			memmove(&extendedAttr[location+length], &extendedAttr[location], le32_to_cpu(lengthExtendedAttr) - location);
			ea_hdr->appAttrLocation = cpu_to_le32(location+length);
		}
		else
		{
			location = le32_to_cpu(lengthExtendedAttr);
		}

		if (le32_to_cpu(ea_hdr->impAttrLocation) == 0xFFFFFFFF)
		{
			if (le32_to_cpu(ea_hdr->appAttrLocation) == 0xFFFFFFFF)
				ea_hdr->impAttrLocation = lengthExtendedAttr;
			else
				ea_hdr->impAttrLocation = ea_hdr->appAttrLocation;
		}

		memcpy(&extendedAttr[location], ea, length);
		lengthExtendedAttr = cpu_to_le32(le32_to_cpu(lengthExtendedAttr) + length);
		if (disc->flags & FLAG_EFE)
			efe->lengthExtendedAttr = lengthExtendedAttr;
		else
			fe->lengthExtendedAttr = lengthExtendedAttr;
	}
	else
	{
		desc->length += length;
		desc->data->length += length;
		desc->data->buffer = realloc(desc->data->buffer, desc->data->length);

		UPDATE_PTR;

		if (le32_to_cpu(ea_hdr->appAttrLocation) == 0xFFFFFFFF)
			ea_hdr->appAttrLocation = lengthExtendedAttr;

		memcpy(&extendedAttr[le32_to_cpu(lengthExtendedAttr)], ea, length);
		lengthExtendedAttr = cpu_to_le32(le32_to_cpu(lengthExtendedAttr) + length);
		if (disc->flags & FLAG_EFE)
			efe->lengthExtendedAttr = lengthExtendedAttr;
		else
			fe->lengthExtendedAttr = lengthExtendedAttr;
	}

	/* UDF-1.50: 3.3.4.1 Extended Attribute Header Descriptor
	 * If the attributes associated with the location fields highlighted above do not exist,
	 * then the value of the location field shall point to the byte after the extended
	 * attribute space. */
	if (disc->udf_rev < 0x0200)
	{
		if (le32_to_cpu(lengthExtendedAttr) && le32_to_cpu(ea_hdr->impAttrLocation) == 0xFFFFFFFF)
			ea_hdr->impAttrLocation = lengthExtendedAttr;
		if (le32_to_cpu(lengthExtendedAttr) && le32_to_cpu(ea_hdr->appAttrLocation) == 0xFFFFFFFF)
			ea_hdr->appAttrLocation = lengthExtendedAttr;
	}

	ea_hdr->descTag = udf_query_tag(disc, TAG_IDENT_EAHD, 1, desc->offset, desc->data, (disc->flags & FLAG_EFE) ? sizeof(*efe) : sizeof(*fe), sizeof(*ea_hdr));

#undef UPDATE_PTR
}

/**
 * @brief create a file tag:FE/EFE udf_descriptor and add the file to a directory
 * @param disc the udf_disc
 * @param pspace the type:PSPACE udf_extent for on-disc allocations
 * @param name the file name in OSTA Compressed Unicode format (d-characters)
 * @param length the length of the file name in bytes
 * @param offset the starting block number to search for on-disc allocations
 * @param parent the directory tag:FE/EFE udf_descriptor
 * @param filechar file characteristics
 * @param filetype the file type
 * @param flags the file flags
 * @return the in-memory address of file tag:FE/EFE udf_descriptor
 */
struct udf_desc *udf_create(struct udf_disc *disc, struct udf_extent *pspace, const dchars *name, uint8_t length, uint32_t offset, struct udf_desc *parent, uint8_t filechar, uint8_t filetype, uint16_t flags)
{
	struct udf_desc *desc;

	if (disc->flags & FLAG_STRATEGY4096)
		offset = udf_alloc_blocks(disc, pspace, offset, 2);
	else
		offset = udf_alloc_blocks(disc, pspace, offset, 1);

	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe;
		uint64_t uniqueID_le64;

		desc = set_desc(pspace, TAG_IDENT_EFE, offset, sizeof(struct extendedFileEntry), NULL);
		efe = (struct extendedFileEntry *)desc->data->buffer;
		memcpy(efe, &default_efe, sizeof(struct extendedFileEntry));
		memcpy(&efe->accessTime, &disc->udf_pvd[0]->recordingDateAndTime, sizeof(timestamp));
		memcpy(&efe->modificationTime, &efe->accessTime, sizeof(timestamp));
		memcpy(&efe->attrTime, &efe->accessTime, sizeof(timestamp));
		memcpy(&efe->createTime, &efe->accessTime, sizeof(timestamp));
		if (filetype == ICBTAG_FILE_TYPE_STREAMDIR ||
		    flags & ICBTAG_FLAG_STREAM)
			efe->uniqueID = cpu_to_le64(0);
		else
		{
			memcpy(&efe->uniqueID, disc->udf_lvid->logicalVolContentsUse, sizeof(efe->uniqueID));
			if (!(le64_to_cpu(efe->uniqueID) & 0x00000000FFFFFFFFUL))
				uniqueID_le64 = cpu_to_le64(le64_to_cpu(efe->uniqueID) + 16);
			else
				uniqueID_le64 = cpu_to_le64(le64_to_cpu(efe->uniqueID) + 1);
			memcpy(disc->udf_lvid->logicalVolContentsUse, &uniqueID_le64, sizeof(uniqueID_le64));
		}
		if (disc->flags & FLAG_STRATEGY4096)
		{
			efe->icbTag.strategyType = cpu_to_le16(4096);
			efe->icbTag.strategyParameter = cpu_to_le16(1);
			efe->icbTag.numEntries = cpu_to_le16(2);
		}
		efe->icbTag.fileType = filetype;
		efe->icbTag.flags = cpu_to_le16(le16_to_cpu(efe->icbTag.flags) | flags);
		if (parent)
		{
//			efe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(parent->offset); // for strategy type != 4
			efe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			efe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			insert_fid(disc, pspace, desc, parent, name, length, filechar);
		}
		else
		{
			efe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			efe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			if (filetype == ICBTAG_FILE_TYPE_DIRECTORY) // root directory
			{
				efe->uid = cpu_to_le32(disc->uid);
				efe->gid = cpu_to_le32(disc->gid);
				efe->permissions = cpu_to_le32(
					((disc->mode & S_IRWXU) << 4) |
					((disc->mode & S_IRWXG) << 2) |
					((disc->mode & S_IRWXO) << 0) |
					((disc->mode & S_IWUSR) ? FE_PERM_U_CHATTR : 0) |
					((disc->mode & S_IWGRP) ? FE_PERM_G_CHATTR : 0) |
					((disc->mode & S_IWOTH) ? FE_PERM_O_CHATTR : 0) |
					0 // Do not allow deleting root directory
				);
			}
		}
		if (filetype == ICBTAG_FILE_TYPE_DIRECTORY)
			query_lvidiu(disc)->numDirs = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numDirs)+1);
		else if (filetype != ICBTAG_FILE_TYPE_STREAMDIR && filetype != ICBTAG_FILE_TYPE_VAT20 && filetype != ICBTAG_FILE_TYPE_UNDEF && !(flags & ICBTAG_FLAG_STREAM))
			query_lvidiu(disc)->numFiles = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numFiles)+1);
	}
	else
	{
		struct fileEntry *fe;
		uint64_t uniqueID_le64;

		desc = set_desc(pspace, TAG_IDENT_FE, offset, sizeof(struct fileEntry), NULL);
		fe = (struct fileEntry *)desc->data->buffer;
		memcpy(fe, &default_fe, sizeof(struct fileEntry));
		memcpy(&fe->accessTime, &disc->udf_pvd[0]->recordingDateAndTime, sizeof(timestamp));
		memcpy(&fe->modificationTime, &fe->accessTime, sizeof(timestamp));
		memcpy(&fe->attrTime, &fe->accessTime, sizeof(timestamp));
		if (filetype == ICBTAG_FILE_TYPE_STREAMDIR ||
		    flags & ICBTAG_FLAG_STREAM)
			fe->uniqueID = cpu_to_le64(0);
		else
		{
			memcpy(&fe->uniqueID, disc->udf_lvid->logicalVolContentsUse, sizeof(fe->uniqueID));
			if (!(le64_to_cpu(fe->uniqueID) & 0x00000000FFFFFFFFUL))
				uniqueID_le64 = cpu_to_le64(le64_to_cpu(fe->uniqueID) + 16);
			else
				uniqueID_le64 = cpu_to_le64(le64_to_cpu(fe->uniqueID) + 1);
			memcpy(disc->udf_lvid->logicalVolContentsUse, &uniqueID_le64, sizeof(uniqueID_le64));
		}
		if (disc->flags & FLAG_STRATEGY4096)
		{
			fe->icbTag.strategyType = cpu_to_le16(4096);
			fe->icbTag.strategyParameter = cpu_to_le16(1);
			fe->icbTag.numEntries = cpu_to_le16(2);
		}
		fe->icbTag.fileType = filetype;
		fe->icbTag.flags = cpu_to_le16(le16_to_cpu(fe->icbTag.flags) | flags);
		if (parent)
		{
//			fe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(parent->offset); // for strategy type != 4
			fe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			fe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			insert_fid(disc, pspace, desc, parent, name, length, filechar);
		}
		else
		{
			fe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			fe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			if (filetype == ICBTAG_FILE_TYPE_DIRECTORY) // root directory
			{
				fe->uid = cpu_to_le32(disc->uid);
				fe->gid = cpu_to_le32(disc->gid);
				fe->permissions = cpu_to_le32(
					((disc->mode & S_IRWXU) << 4) |
					((disc->mode & S_IRWXG) << 2) |
					((disc->mode & S_IRWXO) << 0) |
					((disc->mode & S_IWUSR) ? FE_PERM_U_CHATTR : 0) |
					((disc->mode & S_IWGRP) ? FE_PERM_G_CHATTR : 0) |
					((disc->mode & S_IWOTH) ? FE_PERM_O_CHATTR : 0) |
					0 // Do not allow deleting root directory
				);
			}
		}
		if (filetype == ICBTAG_FILE_TYPE_DIRECTORY)
			query_lvidiu(disc)->numDirs = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numDirs)+1);
		else if (filetype != ICBTAG_FILE_TYPE_STREAMDIR && filetype != ICBTAG_FILE_TYPE_VAT20 && filetype != ICBTAG_FILE_TYPE_UNDEF && !(flags & ICBTAG_FLAG_STREAM))
			query_lvidiu(disc)->numFiles = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numFiles)+1);
	}

	return desc;
}

/**
 * @brief create a directory tag:FE/EFE udf_descriptor and add the directory to
 *        a parent directory
 * @param disc the udf_disc
 * @param pspace the type:PSPACE udf_extent for on-disc allocations
 * @param name the file name in OSTA Compressed Unicode format (d-characters)
 * @param length the length of the file name in bytes
 * @param offset the starting block number to search for on-disc allocations
 * @param parent the parent directory tag:FE/EFE udf_descriptor
 * @return the in-memory address of the directory tag:FE/EFE udf_descriptor
 */
struct udf_desc *udf_mkdir(struct udf_disc *disc, struct udf_extent *pspace, const dchars *name, uint8_t length, uint32_t offset, struct udf_desc *parent)
{
	struct udf_desc *desc = udf_create(disc, pspace, name, length, offset, parent, FID_FILE_CHAR_DIRECTORY, ICBTAG_FILE_TYPE_DIRECTORY, 0);

	if (!parent)
		parent = desc; // the root directory is it's own parent
	insert_fid(disc, pspace, parent, desc, NULL, 0, FID_FILE_CHAR_DIRECTORY | FID_FILE_CHAR_PARENT); // directory parent back links are unnamed

	return desc;
}

#define BITS_PER_LONG 32

#define leBPL_to_cpup(x) leNUM_to_cpup(BITS_PER_LONG, x)
#define leNUM_to_cpup(x,y) xleNUM_to_cpup(x,y)
#define xleNUM_to_cpup(x,y) (le ## x ## _to_cpup(y))
#define uintBPL uint(BITS_PER_LONG)
#define uint(x) xuint(x)
#define xuint(x) uint ## x ## _t

/**
 * @brief utility function to find the first zero bit in an unsigned long
 * @param word the unsigned long to search
 * @return the 0 based bit position from the lsb or BITS_PER_LONG
 */
static inline unsigned long ffz(unsigned long word)
{
	unsigned long result;

	result = 0;
	while (word & 1)
	{
		result ++;
		word >>= 1;
	}

	return result;
}

/**
 * @brief find the first one bit in a space bitmap
 * @param addr the in-memory address of the space bitmap
 * @param size the size of the space bitmap in bits
 * @param offset the starting bit position for the search
 * @return the 0 based bit position or size
 */
static inline unsigned long udf_find_next_one_bit (void * addr, unsigned long size, unsigned long offset)
{
	uintBPL * p = ((uintBPL *) addr) + (offset / BITS_PER_LONG);
	uintBPL result = offset & ~(BITS_PER_LONG-1);
	uintBPL tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= (BITS_PER_LONG-1);
	if (offset)
	{
		tmp = leBPL_to_cpup(p++);
		tmp &= ~0UL << offset;
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1))
	{
		if ((tmp = leBPL_to_cpup(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = 0;
	memcpy(&tmp, p, (size+7)/8);
	tmp = leBPL_to_cpup(&tmp);
found_first:
	tmp &= ~0UL >> (BITS_PER_LONG-size);
	tmp |= (1UL << size);
found_middle:
	return result + ffz(~tmp);
}

/**
 * @brief find the first zero bit in a space bitmap
 * @param addr the in-memory address of the space bitmap
 * @param size the size of the space bitmap in bits
 * @param offset the starting bit position for the search
 * @return the 0 based bit position or size
 */
static inline unsigned long udf_find_next_zero_bit(void * addr, unsigned long size, unsigned long offset)
{
	uintBPL * p = ((uintBPL *) addr) + (offset / BITS_PER_LONG);
	uintBPL result = offset & ~(BITS_PER_LONG-1);
	uintBPL tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= (BITS_PER_LONG-1);
	if (offset)
	{
		tmp = leBPL_to_cpup(p++);
		tmp |= (~0UL >> (BITS_PER_LONG-offset));
		if (size < BITS_PER_LONG)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1))
	{
		if (~(tmp = leBPL_to_cpup(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = 0;
	memcpy(&tmp, p, (size+7)/8);
	tmp = leBPL_to_cpup(&tmp);
found_first:
	tmp |= (~0UL << size);
	if (tmp == (uintBPL)~0UL)	/* Are any bits zero? */
		return result + size; /* Nope. */
found_middle:
	return result + ffz(tmp);
}

/**
 * @brief allocate an aligned space bitmap on-disc
 * @param disc the udf disc
 * @param bitmap the space bitmap tag:USB/FSB udf_descriptor
 * @param start the starting block number to search for on-disc allocations
 * @param blocks the number of blocks in the space bitmap
 * @return the starting block number of the on-disc aligned space bitmap
 */
int udf_alloc_bitmap_blocks(struct udf_disc *disc, struct udf_desc *bitmap, uint32_t start, uint32_t blocks)
{
	uint32_t alignment = disc->sizing[PSPACE_SIZE].align;
	struct spaceBitmapDesc *sbd = (struct spaceBitmapDesc *)bitmap->data->buffer;
	uint32_t end;

	do
	{
		start = ((start + alignment - 1) / alignment) * alignment;
		if (start + blocks > le32_to_cpu(sbd->numOfBits))
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		if (sbd->bitmap[start/8] & (1 << (start%8)))
		{
			end = udf_find_next_zero_bit(sbd->bitmap, le32_to_cpu(sbd->numOfBits), start);
		}
		else
			start = end = udf_find_next_one_bit(sbd->bitmap, le32_to_cpu(sbd->numOfBits), start);
	} while ((end - start) < blocks);

	clear_bits(sbd->bitmap, start, blocks);
	return start;
}

/**
 * @brief allocate a space table on-disc
 * @param disc the udf_disc
 * @param table the space table tag:USE/FSE udf_descriptor
 * @param start the starting block offset for the allocation search
 * @param blocks the number of blocks in the space table
 * @return the starting block number of the on-disc space table
 */
int udf_alloc_table_blocks(struct udf_disc *disc, struct udf_desc *table, uint32_t start, uint32_t blocks)
{
	uint32_t alignment = disc->sizing[PSPACE_SIZE].align;
	struct unallocSpaceEntry *use = (struct unallocSpaceEntry *)table->data->buffer;
	uint32_t end, offset = 0;
	short_ad *sad;

	do
	{
		if (offset >= le32_to_cpu(use->lengthAllocDescs))
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		sad = (short_ad *)&use->allocDescs[offset];
		if (start < le32_to_cpu(sad->extPosition))
			start = le32_to_cpu(sad->extPosition);
		start = ((start + alignment - 1) / alignment) * alignment;
		end = le32_to_cpu(sad->extPosition) + ((le32_to_cpu(sad->extLength) & EXT_LENGTH_MASK) / disc->blocksize);
		if (start > end)
			start = end;
		offset += sizeof(short_ad);
	} while ((end - start) < blocks);

	if (start == le32_to_cpu(sad->extPosition) && start + blocks == end)
	{
		/* deleted extent */
		memmove(&use->allocDescs[offset-sizeof(short_ad)],
			&use->allocDescs[offset],
			le32_to_cpu(use->lengthAllocDescs) - offset);
		use->lengthAllocDescs = cpu_to_le32(le32_to_cpu(use->lengthAllocDescs) - sizeof(short_ad));
		memset(&use->allocDescs[le32_to_cpu(use->lengthAllocDescs)], 0x00, sizeof(short_ad));
	}
	else if (start == le32_to_cpu(sad->extPosition))
	{
		sad->extPosition = cpu_to_le32(start + blocks);
		sad->extLength = cpu_to_le32(le32_to_cpu(sad->extLength) - blocks * disc->blocksize);
	}
	else if (start + blocks == end)
	{
		sad->extLength = cpu_to_le32(le32_to_cpu(sad->extLength) - blocks * disc->blocksize);
	}
	else
	{
		memmove(&use->allocDescs[offset+sizeof(short_ad)],
			&use->allocDescs[offset],
			le32_to_cpu(use->lengthAllocDescs) - offset);
		sad->extLength = cpu_to_le32(EXT_NOT_RECORDED_ALLOCATED | (start - le32_to_cpu(sad->extPosition)) * disc->blocksize);
		sad = (short_ad *)&use->allocDescs[offset];
		sad->extPosition = cpu_to_le32(start+blocks);
		sad->extLength = cpu_to_le32(EXT_NOT_RECORDED_ALLOCATED | (end - start - blocks) * disc->blocksize);
		use->lengthAllocDescs = cpu_to_le32(le32_to_cpu(use->lengthAllocDescs) + sizeof(short_ad));
	}
	use->descTag = udf_query_tag(disc, TAG_IDENT_USE, 1, table->offset, table->data, 0, sizeof(struct unallocSpaceEntry) + le32_to_cpu(use->lengthAllocDescs));
	return start;
}

/**
 * @brief allocate blocks on-disc
 * @param disc the udf_disc
 * @param start the starting block offset for the allocation search
 * @param blocks the number of blocks to allocate
 * @return the starting block number of the on-disc allocation
 */
int udf_alloc_blocks(struct udf_disc *disc, struct udf_extent *pspace, uint32_t start, uint32_t blocks)
{
	struct udf_desc *desc;
	struct partitionHeaderDesc *phd = (struct partitionHeaderDesc *)disc->udf_pd[0]->partitionContentsUse;
	uint32_t value;

	memcpy(&value, &disc->udf_lvid->data[sizeof(uint32_t)*0], sizeof(value));
	value = cpu_to_le32(le32_to_cpu(value) - blocks);
	memcpy(&disc->udf_lvid->data[sizeof(uint32_t)*0], &value, sizeof(value));

	if (disc->flags & FLAG_FREED_BITMAP)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->freedSpaceBitmap.extPosition));
		return udf_alloc_bitmap_blocks(disc, desc, start, blocks);
	}
	else if (disc->flags & FLAG_FREED_TABLE)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->freedSpaceTable.extPosition));
		return udf_alloc_table_blocks(disc, desc, start, blocks);
	}
	else if (disc->flags & FLAG_UNALLOC_BITMAP)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->unallocSpaceBitmap.extPosition));
		return udf_alloc_bitmap_blocks(disc, desc, start, blocks);
	}
	else if (disc->flags & FLAG_UNALLOC_TABLE)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->unallocSpaceTable.extPosition));
		return udf_alloc_table_blocks(disc, desc, start, blocks);
	}
	else if (disc->flags & FLAG_VAT)
	{
		uint32_t offset = 0, length = 0, i = 0;
		if (pspace->tail)
		{
			offset = pspace->tail->offset;
			length = (pspace->tail->length + disc->blocksize - 1) / disc->blocksize;
		}
		if (offset + length > start)
			start = offset + length;
		if (start >= pspace->blocks)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		for (i = 0; i < blocks; ++i)
			disc->vat[disc->vat_entries++] = cpu_to_le32(start+i);
		return start;
	}
	else
		return 0;
}
