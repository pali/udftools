/*
 * file.c
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

#include "libudffs.h"
#include "defaults.h"
#include "config.h"

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
		crc = udf_crc(data->buffer + offset, data->length - offset, crc);
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

tag udf_query_tag(struct udf_disc *disc, uint16_t Ident, uint16_t SerialNum, uint32_t Location, struct udf_data *data, uint16_t length)
{
	tag ret;
	int i;
	uint16_t crc = 0;
	int offset = sizeof(tag);
	int clength;

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
		crc = udf_crc(data->buffer + offset, clength - offset, crc);
		length -= clength;
		offset = 0;
		data = data->next;
	}
	ret.descCRC = cpu_to_le16(crc);
	ret.tagLocation = cpu_to_le32(Location);
	for (i=0; i<16; i++)
		if (i != 4)
			ret.tagChecksum += (uint8_t)(((char *)&ret)[i]);

	return ret;
}

int insert_desc(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *desc, struct udf_desc *parent, struct udf_data *data)
{
	uint32_t block = 0;

	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe;

		efe = (struct extendedFileEntry *)parent->data->buffer;

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
				fiddesc = set_desc(disc, pspace, TAG_IDENT_FID, block, data->length, data);
				if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
				{
					short_ad *sad;

					parent->length += sizeof(short_ad);
					parent->data->length += sizeof(short_ad);
					parent->data->buffer = realloc(parent->data->buffer, parent->length);
					efe = (struct extendedFileEntry *)parent->data->buffer;
					sad = (short_ad *)&efe->allocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs)];
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
					lad = (long_ad *)&efe->allocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs)];
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

					sad = (short_ad *)&efe->allocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs) - sizeof(short_ad)];
					fiddesc = find_desc(pspace, le32_to_cpu(sad->extPosition));
					block = fiddesc->offset;
					append_data(fiddesc, data);
					sad->extLength = cpu_to_le32(le32_to_cpu(sad->extLength) + data->length);
				}
				else if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG)
				{
					long_ad *lad;

					lad = (long_ad *)&efe->allocDescs[le32_to_cpu(efe->lengthExtendedAttr) + le32_to_cpu(efe->lengthAllocDescs) - sizeof(long_ad)];
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
		struct fileEntry *fe;

		fe = (struct fileEntry *)parent->data->buffer;

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
				fiddesc = set_desc(disc, pspace, TAG_IDENT_FID, block, data->length, data);
				if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
				{
					short_ad *sad;

					parent->length += sizeof(short_ad);
					parent->data->length += sizeof(short_ad);
					parent->data->buffer = realloc(parent->data->buffer, parent->length);
					fe = (struct fileEntry *)parent->data->buffer;
					sad = (short_ad *)&fe->allocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs)];
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
					lad = (long_ad *)&fe->allocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs)];
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

					sad = (short_ad *)&fe->allocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs) - sizeof(short_ad)];
					fiddesc = find_desc(pspace, le32_to_cpu(sad->extPosition));
					block = fiddesc->offset;
					append_data(fiddesc, data);
					sad->extLength = cpu_to_le32(le32_to_cpu(sad->extLength) + data->length);
				}
				else if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG)
				{
					long_ad *lad;

					lad = (long_ad *)&fe->allocDescs[le32_to_cpu(fe->lengthExtendedAttr) + le32_to_cpu(fe->lengthAllocDescs) - sizeof(long_ad)];
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

void insert_data(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *desc, struct udf_data *data)
{
	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe;

		efe = (struct extendedFileEntry *)desc->data->buffer;

		if ((le16_to_cpu(efe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB)
		{
			append_data(desc, data);
			efe->lengthAllocDescs = cpu_to_le32(le32_to_cpu(efe->lengthAllocDescs) + data->length);
			efe->informationLength = cpu_to_le64(le64_to_cpu(efe->informationLength) + data->length);
			efe->objectSize = cpu_to_le64(le64_to_cpu(efe->objectSize) + data->length);
		}
	}
	else
	{
		struct fileEntry *fe;

		fe = (struct fileEntry *)desc->data->buffer;

		if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB)
		{
			append_data(desc, data);
			fe->lengthAllocDescs = cpu_to_le32(le32_to_cpu(fe->lengthAllocDescs) + data->length);
			fe->informationLength = cpu_to_le64(le64_to_cpu(fe->informationLength) + data->length);
		}
	}

	*(tag *)desc->data->buffer = query_tag(disc, pspace, desc, 1);
}

uint32_t compute_ident_length(uint32_t length)
{
	return length + (4 - (length % 4)) %4;
}

void insert_fid(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *desc, struct udf_desc *parent, uint8_t *name, uint8_t length, uint8_t fc)
{
	struct udf_data *data;
	struct fileIdentDesc *fid;
	int ilength = compute_ident_length(sizeof(struct fileIdentDesc) + length);
	int offset;
	uint64_t uniqueID;
	
	data = alloc_data(NULL, ilength);
	fid = data->buffer;

	offset = insert_desc(disc, pspace, desc, parent, data);
	fid->descTag.tagLocation = cpu_to_le32(offset);

	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe;

		efe = (struct extendedFileEntry *)desc->data->buffer;
		efe->fileLinkCount = cpu_to_le16(le16_to_cpu(efe->fileLinkCount) + 1);
		uniqueID = le64_to_cpu(efe->uniqueID);

		efe = (struct extendedFileEntry *)parent->data->buffer;

		if (disc->flags & FLAG_STRATEGY4096)
			fid->icb.extLength = cpu_to_le32(disc->blocksize * 2);
		else
			fid->icb.extLength = cpu_to_le32(disc->blocksize);
		fid->icb.extLocation.logicalBlockNum = cpu_to_le32(desc->offset);
		fid->icb.extLocation.partitionReferenceNum = cpu_to_le16(0);
		*(uint32_t *)((struct allocDescImpUse *)fid->icb.impUse)->impUse = cpu_to_le32(uniqueID & 0x00000000FFFFFFFFUL);
		fid->fileVersionNum = cpu_to_le16(1);
		fid->fileCharacteristics = fc;
		fid->lengthFileIdent = length;
		fid->lengthOfImpUse = cpu_to_le16(0);
		memcpy(fid->fileIdent, name, length);
		fid->descTag = udf_query_tag(disc, TAG_IDENT_FID, 1, le32_to_cpu(fid->descTag.tagLocation), data, ilength);

		efe->informationLength = cpu_to_le64(le64_to_cpu(efe->informationLength) + ilength);
		efe->objectSize = cpu_to_le64(le64_to_cpu(efe->objectSize) + ilength);
	}
	else
	{
		struct fileEntry *fe;

		fe = (struct fileEntry *)desc->data->buffer;
		fe->fileLinkCount = cpu_to_le16(le16_to_cpu(fe->fileLinkCount) + 1);
		uniqueID = le64_to_cpu(fe->uniqueID);

		fe = (struct fileEntry *)parent->data->buffer;

		if (disc->flags & FLAG_STRATEGY4096)
			fid->icb.extLength = cpu_to_le32(disc->blocksize * 2);
		else
			fid->icb.extLength = cpu_to_le32(disc->blocksize);
		fid->icb.extLocation.logicalBlockNum = cpu_to_le32(desc->offset);
		fid->icb.extLocation.partitionReferenceNum = cpu_to_le16(0);
		*(uint32_t *)((struct allocDescImpUse *)fid->icb.impUse)->impUse = cpu_to_le32(uniqueID & 0x00000000FFFFFFFFUL);
		fid->fileVersionNum = cpu_to_le16(1);
		fid->fileCharacteristics = fc;
		fid->lengthFileIdent = length;
		fid->lengthOfImpUse = cpu_to_le16(0);
		memcpy(fid->fileIdent, name, length);
		fid->descTag = udf_query_tag(disc, TAG_IDENT_FID, 1, le32_to_cpu(fid->descTag.tagLocation), data, ilength);

		fe->informationLength = cpu_to_le64(le64_to_cpu(fe->informationLength) + ilength);
	}
	*(tag *)desc->data->buffer = query_tag(disc, pspace, desc, 1);
	*(tag *)parent->data->buffer = query_tag(disc, pspace, parent, 1);
}

struct udf_desc *udf_create(struct udf_disc *disc, struct udf_extent *pspace, uint8_t *name, uint8_t length, uint32_t offset, struct udf_desc *parent, uint8_t filechar, uint8_t filetype, uint16_t flags)
{
	struct udf_desc *desc;

	if (disc->flags & FLAG_STRATEGY4096)
		offset = udf_alloc_blocks(disc, pspace, offset, 2);
	else
		offset = udf_alloc_blocks(disc, pspace, offset, 1);

	if (disc->flags & FLAG_EFE)
	{
		struct extendedFileEntry *efe;

		desc = set_desc(disc, pspace, TAG_IDENT_EFE, offset, sizeof(struct extendedFileEntry), NULL);
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
			efe->uniqueID = cpu_to_le64(le64_to_cpu(((uint64_t *)disc->udf_lvid->logicalVolContentsUse)[0]));
			if (!(le64_to_cpu(efe->uniqueID) & 0x00000000FFFFFFFFUL))
				((uint64_t *)disc->udf_lvid->logicalVolContentsUse)[0] = cpu_to_le64(le64_to_cpu(efe->uniqueID) + 16);
			else
				((uint64_t *)disc->udf_lvid->logicalVolContentsUse)[0] = cpu_to_le64(le64_to_cpu(efe->uniqueID) + 1);
		}
		if (disc->flags & FLAG_STRATEGY4096)
		{
			efe->icbTag.strategyType = cpu_to_le16(4096);
			efe->icbTag.strategyParameter = cpu_to_le16(1);
			efe->icbTag.numEntries = cpu_to_le16(2);
		}
		if (parent)
		{
//			efe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(parent->offset);
			efe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			efe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			insert_fid(disc, pspace, desc, parent, name, length, filechar);
		}
		else
		{
			efe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			efe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
		}
		efe->icbTag.fileType = filetype;
		efe->icbTag.flags = cpu_to_le16(le16_to_cpu(efe->icbTag.flags) | flags);
		if (filetype == ICBTAG_FILE_TYPE_DIRECTORY)
			query_lvidiu(disc)->numDirs = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numDirs)+1);
		else if (filetype != ICBTAG_FILE_TYPE_STREAMDIR && filetype != ICBTAG_FILE_TYPE_VAT20 && filetype != ICBTAG_FILE_TYPE_UNDEF && !(flags & ICBTAG_FLAG_STREAM))
			query_lvidiu(disc)->numFiles = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numFiles)+1);
		efe->descTag = query_tag(disc, pspace, desc, 1);
	}
	else
	{
		struct fileEntry *fe;

		desc = set_desc(disc, pspace, TAG_IDENT_FE, offset, sizeof(struct fileEntry), NULL);
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
			fe->uniqueID = cpu_to_le64(le64_to_cpu(((uint64_t *)disc->udf_lvid->logicalVolContentsUse)[0]));
			if (!(le64_to_cpu(fe->uniqueID) & 0x00000000FFFFFFFFUL))
				((uint64_t *)disc->udf_lvid->logicalVolContentsUse)[0] = cpu_to_le64(le64_to_cpu(fe->uniqueID) + 16);
			else
				((uint64_t *)disc->udf_lvid->logicalVolContentsUse)[0] = cpu_to_le64(le64_to_cpu(fe->uniqueID) + 1);
		}
		if (disc->flags & FLAG_STRATEGY4096)
		{
			fe->icbTag.strategyType = cpu_to_le16(4096);
			fe->icbTag.strategyParameter = cpu_to_le16(1);
			fe->icbTag.numEntries = cpu_to_le16(2);
		}
		if (parent)
		{
//			fe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(parent->offset);
			fe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			fe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			insert_fid(disc, pspace, desc, parent, name, length, filechar);
		}
		else
		{
			fe->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
			fe->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
		}
		fe->icbTag.fileType = filetype;
		fe->icbTag.flags = cpu_to_le16(le16_to_cpu(fe->icbTag.flags) | flags);
		if (filetype == ICBTAG_FILE_TYPE_DIRECTORY)
			query_lvidiu(disc)->numDirs = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numDirs)+1);
		else if (filetype != ICBTAG_FILE_TYPE_STREAMDIR && filetype != ICBTAG_FILE_TYPE_VAT20 && filetype != ICBTAG_FILE_TYPE_UNDEF && !(flags & ICBTAG_FLAG_STREAM))
			query_lvidiu(disc)->numFiles = cpu_to_le32(le32_to_cpu(query_lvidiu(disc)->numFiles)+1);
		fe->descTag = query_tag(disc, pspace, desc, 1);
	}

	return desc;
}

struct udf_desc *udf_mkdir(struct udf_disc *disc, struct udf_extent *pspace, uint8_t *name, uint8_t length, uint32_t offset, struct udf_desc *parent)
{
	struct udf_desc *desc;

	desc = udf_create(disc, pspace, name, length, offset, parent, FID_FILE_CHAR_DIRECTORY, ICBTAG_FILE_TYPE_DIRECTORY, 0);

	if (parent)
		insert_fid(disc, pspace, parent, desc, NULL, 0, FID_FILE_CHAR_DIRECTORY | FID_FILE_CHAR_PARENT);
	else
		insert_fid(disc, pspace, desc, desc, NULL, 0, FID_FILE_CHAR_DIRECTORY | FID_FILE_CHAR_PARENT);
	
	return desc;
}

#define BITS_PER_LONG 32

#define leBPL_to_cpup(x) leNUM_to_cpup(BITS_PER_LONG, x)
#define leNUM_to_cpup(x,y) xleNUM_to_cpup(x,y)
#define xleNUM_to_cpup(x,y) (le ## x ## _to_cpup(y))
#define uintBPL uint(BITS_PER_LONG)
#define uint(x) xuint(x)
#define xuint(x) uint ## x ## _t

inline unsigned long ffz(unsigned long word)
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

inline unsigned long udf_find_next_one_bit (void * addr, unsigned long size, unsigned long offset)
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
	tmp = leBPL_to_cpup(p);
found_first:
	tmp &= ~0UL >> (BITS_PER_LONG-size);
found_middle:
	return result + ffz(~tmp);
}

inline unsigned long udf_find_next_zero_bit(void * addr, unsigned long size, unsigned long offset)
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
	tmp = leBPL_to_cpup(p);
found_first:
	tmp |= (~0UL << size);
	if (tmp == ~0UL)	/* Are any bits zero? */
		return result + size; /* Nope. */
found_middle:
	return result + ffz(tmp);
}

int udf_alloc_bitmap_blocks(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *bitmap, uint32_t start, uint32_t blocks)
{
	uint32_t alignment = disc->sizing[PSPACE_SIZE].align;
	struct spaceBitmapDesc *sbd = (struct spaceBitmapDesc *)bitmap->data->buffer;
	uint32_t end;

	do
	{
		start = ((start + alignment - 1) / alignment) * alignment;
		if (sbd->bitmap[start/8] & (1 << (start%8)))
		{
			end = udf_find_next_zero_bit(sbd->bitmap, sbd->numOfBits, start);
		}
		else
			start = end = udf_find_next_one_bit(sbd->bitmap, sbd->numOfBits, start);
	} while ((end - start) <= blocks);

	clear_bits(sbd->bitmap, start, blocks);
	return start;
}

int udf_alloc_table_blocks(struct udf_disc *disc, struct udf_extent *pspace, struct udf_desc *table, uint32_t start, uint32_t blocks)
{
	uint32_t alignment = disc->sizing[PSPACE_SIZE].align;
	struct unallocSpaceEntry *use = (struct unallocSpaceEntry *)table->data->buffer;
	uint32_t end, offset = 0;
	short_ad *sad;

	do
	{
		sad = (short_ad *)&use->allocDescs[offset];
		if (start < le32_to_cpu(sad->extPosition))
			start = le32_to_cpu(sad->extPosition);
		start = ((start + alignment - 1) / alignment) * alignment;
		end = le32_to_cpu(sad->extPosition) + ((le32_to_cpu(sad->extLength) & 0x3FFFFFFF) >> disc->blocksize_bits);
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
	use->descTag = udf_query_tag(disc, TAG_IDENT_USE, 1, table->offset, table->data, sizeof(struct unallocSpaceEntry) + le32_to_cpu(use->lengthAllocDescs));
	return start;
}

int udf_alloc_blocks(struct udf_disc *disc, struct udf_extent *pspace, uint32_t start, uint32_t blocks)
{
	struct udf_desc *desc;
	struct partitionHeaderDesc *phd = (struct partitionHeaderDesc *)disc->udf_pd[0]->partitionContentsUse;

	disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(le32_to_cpu(disc->udf_lvid->freeSpaceTable[0]) - blocks);

	if (disc->flags & FLAG_FREED_BITMAP)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->freedSpaceBitmap.extPosition));
		return udf_alloc_bitmap_blocks(disc, pspace, desc, start, blocks);
	}
	else if (disc->flags & FLAG_FREED_TABLE)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->freedSpaceTable.extPosition));
		return udf_alloc_table_blocks(disc, pspace, desc, start, blocks);
	}
	else if (disc->flags & FLAG_UNALLOC_BITMAP)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->unallocSpaceBitmap.extPosition));
		return udf_alloc_bitmap_blocks(disc, pspace, desc, start, blocks);
	}
	else if (disc->flags & FLAG_UNALLOC_TABLE)
	{
		desc = find_desc(pspace, le32_to_cpu(phd->unallocSpaceTable.extPosition));
		return udf_alloc_table_blocks(disc, pspace, desc, start, blocks);
	}
	else if (disc->flags & FLAG_VAT)
	{
		int offset = 0, length = 0;
		if (pspace->tail)
		{
			offset = pspace->tail->offset;
			length = (pspace->tail->length + disc->blocksize - 1) >>
				disc->blocksize_bits;
		}
		if (offset + length > start)
			start = offset + length;
		disc->vat[disc->vat_entries++] = start;
		return start;
	}
	else
		return 0;
}
