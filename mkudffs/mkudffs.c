/*
 * mkudffs.c
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

#include "mkudffs.h"
#include "defaults.h"
#include "config.h"

void udf_init_disc(struct udf_disc *disc)
{
	timestamp	ts;
	struct timeval	tv;
	struct tm 	*tm;
	int		altzone;

	memset(disc, 0x00, sizeof(disc));

	disc->blocksize = 2048;
	disc->blocksize_bits = 11;
	disc->udf_rev = le16_to_cpu(default_lvidiu.minUDFReadRev);
	disc->flags = FLAG_UTF8 | FLAG_CLOSED;
	if (disc->udf_rev >= 0x0200)
		disc->flags |= FLAG_EFE;

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);
	altzone = timezone - 3600;
	if (daylight)
		ts.typeAndTimezone = cpu_to_le16(((-altzone/60) & 0x0FFF) | 0x1000);
	else
		ts.typeAndTimezone = cpu_to_le16(((-timezone/60) & 0x0FFF) | 0x1000);
	ts.year = cpu_to_le16(1900 + tm->tm_year);
	ts.month = 1 + tm->tm_mon;
	ts.day = tm->tm_mday;
	ts.hour = tm->tm_hour;
	ts.minute = tm->tm_min;
	ts.second = tm->tm_sec;
	ts.centiseconds = tv.tv_usec / 10000;
	ts.hundredsOfMicroseconds = (tv.tv_usec - ts.centiseconds * 10000) / 100;
	ts.microseconds = tv.tv_usec - ts.centiseconds * 10000 - ts.hundredsOfMicroseconds * 100;

	/* Allocate/Initialize Descriptors */
	disc->udf_pvd[0] = malloc(sizeof(struct primaryVolDesc));
	memcpy(disc->udf_pvd[0], &default_pvd, sizeof(struct primaryVolDesc));
	memcpy(&disc->udf_pvd[0]->recordingDateAndTime, &ts, sizeof(timestamp));
	sprintf(&disc->udf_pvd[0]->volSetIdent[1], "%08lx%s",
		mktime(tm), &disc->udf_pvd[0]->volSetIdent[9]);
	disc->udf_pvd[0]->volIdent[31] = strlen(disc->udf_pvd[0]->volIdent);
	disc->udf_pvd[0]->volSetIdent[127] = strlen(disc->udf_pvd[0]->volSetIdent);

	disc->udf_lvd[0] = malloc(sizeof(struct logicalVolDesc));
	memcpy(disc->udf_lvd[0], &default_lvd, sizeof(struct logicalVolDesc));
	disc->udf_lvd[0]->logicalVolIdent[127] = strlen(disc->udf_lvd[0]->logicalVolIdent);

	disc->udf_pd[0] = malloc(sizeof(struct partitionDesc));
	memcpy(disc->udf_pd[0], &default_pd, sizeof(struct partitionDesc));

	disc->udf_usd[0] = malloc(sizeof(struct unallocSpaceDesc));
	memcpy(disc->udf_usd[0], &default_usd, sizeof(struct unallocSpaceDesc));

	disc->udf_iuvd[0] = malloc(sizeof(struct impUseVolDesc) + sizeof(struct impUseVolDescImpUse));
	memcpy(disc->udf_iuvd[0], &default_iuvd, sizeof(struct impUseVolDesc));
	memcpy(query_iuvdiu(disc), &default_iuvdiu, sizeof(struct impUseVolDescImpUse));
	query_iuvdiu(disc)->logicalVolIdent[127] = strlen(query_iuvdiu(disc)->logicalVolIdent);
	query_iuvdiu(disc)->LVInfo1[35] = strlen(query_iuvdiu(disc)->LVInfo1);
	query_iuvdiu(disc)->LVInfo2[35] = strlen(query_iuvdiu(disc)->LVInfo2);
	query_iuvdiu(disc)->LVInfo3[35] = strlen(query_iuvdiu(disc)->LVInfo3);

	disc->udf_td[0] = malloc(sizeof(struct terminatingDesc));
	memcpy(disc->udf_td[0], &default_td, sizeof(struct terminatingDesc));

	disc->udf_lvid = malloc(sizeof(struct logicalVolIntegrityDesc) + sizeof(struct logicalVolIntegrityDescImpUse));
	memcpy(disc->udf_lvid, &default_lvid, sizeof(struct logicalVolIntegrityDesc));
	memcpy(&disc->udf_lvid->recordingDateAndTime, &ts, sizeof(timestamp));
	memcpy(query_lvidiu(disc), &default_lvidiu, sizeof(struct logicalVolIntegrityDescImpUse));

	disc->udf_stable[0] = malloc(sizeof(struct sparingTable));
	memcpy(disc->udf_stable[0], &default_stable, sizeof(struct sparingTable));

	disc->vat = calloc(1, disc->blocksize);
	disc->vat_entries = 0;

	disc->udf_fsd = malloc(sizeof(struct fileSetDesc));
	memcpy(disc->udf_fsd, &default_fsd, sizeof(struct fileSetDesc));
	memcpy(&disc->udf_fsd->recordingDateAndTime, &ts, sizeof(timestamp));
	disc->udf_fsd->logicalVolIdent[127] = strlen(disc->udf_fsd->logicalVolIdent);
	disc->udf_fsd->fileSetIdent[31] = strlen(disc->udf_fsd->fileSetIdent);
	disc->udf_fsd->copyrightFileIdent[31] = strlen(disc->udf_fsd->copyrightFileIdent);
	disc->udf_fsd->abstractFileIdent[31] = strlen(disc->udf_fsd->abstractFileIdent);

	disc->head = malloc(sizeof(struct udf_extent));
	disc->tail = disc->head;

	disc->head->space_type = USPACE;
	disc->head->start = 0;
	disc->head->next = NULL;
	disc->head->prev = NULL;
}

int udf_set_version(struct udf_disc *disc, int udf_rev)
{
	struct logicalVolIntegrityDescImpUse *lvidiu;

	if (disc->udf_rev == udf_rev)
		return 0;
	else if (udf_rev != 0x0102 &&
		udf_rev != 0x0150 &&
		udf_rev != 0x0200 &&
		udf_rev != 0x0201 &&
		udf_rev != 0x0250)
	{
		return 1;
	}
	else
		disc->udf_rev = udf_rev;

	if (disc->udf_rev < 0x0200)
	{
		disc->flags &= ~FLAG_EFE;
		strcpy(disc->udf_pd[0]->partitionContents.ident, PD_PARTITION_CONTENTS_NSR02);
	}
	else
	{
		disc->flags |= FLAG_EFE;
		strcpy(disc->udf_pd[0]->partitionContents.ident, PD_PARTITION_CONTENTS_NSR03);
	}

	((uint16_t *)disc->udf_fsd->domainIdent.identSuffix)[0] = cpu_to_le16(udf_rev); 
	((uint16_t *)disc->udf_lvd[0]->domainIdent.identSuffix)[0] = cpu_to_le16(udf_rev); 
	((uint16_t *)disc->udf_iuvd[0]->impIdent.identSuffix)[0] = le16_to_cpu(udf_rev); 
	lvidiu = query_lvidiu(disc);
	lvidiu->minUDFReadRev = le16_to_cpu(udf_rev);
	lvidiu->minUDFWriteRev = le16_to_cpu(udf_rev);
	lvidiu->maxUDFWriteRev = le16_to_cpu(udf_rev);
	((uint16_t *)disc->udf_stable[0]->sparingIdent.identSuffix)[0] = le16_to_cpu(udf_rev);
	return 0;
}

void split_space(struct udf_disc *disc)
{
	uint32_t sizes[UDF_ALLOC_TYPE_SIZE];
	uint32_t offsets[UDF_ALLOC_TYPE_SIZE];
	uint32_t blocks = disc->head->blocks;
	uint32_t start, size;
	struct sparablePartitionMap *spm;
	struct udf_extent *ext;
	int i, j;

	if (disc->flags & FLAG_BRIDGE)
	{
		set_extent(disc, RESERVED, 0, 512);
		set_extent(disc, ANCHOR, 512, 1);
	}
	else
	{
		set_extent(disc, RESERVED, 0, 32768 / disc->blocksize);
		if (disc->blocksize >= 2048)
			set_extent(disc, VRS, (2048 * 16) / disc->blocksize, 3);
		else
			set_extent(disc, VRS, (2048 * 16) / disc->blocksize, ((2048 * 3) + disc->blocksize - 1) / disc->blocksize);
		set_extent(disc, ANCHOR, 256, 1);
	}

	if (disc->flags & FLAG_CLOSED)
		set_extent(disc, ANCHOR, blocks-257, 1);

	if (!(disc->flags & FLAG_VAT))
		set_extent(disc, ANCHOR, blocks-1, 1);
	else
		set_extent(disc, PSPACE, blocks-1, 1);

	for (i=0; i<UDF_ALLOC_TYPE_SIZE; i++)
	{
		sizes[i] = disc->sizing[i].numSize * blocks / disc->sizing[i].denomSize;
		if (disc->sizing[i].minSize > sizes[i])
			sizes[i] = disc->sizing[i].minSize;
		offsets[i] = disc->sizing[i].align;
	}

	start = next_extent_size(find_extent(disc, 256), USPACE, sizes[VDS_SIZE], offsets[VDS_SIZE]);
	set_extent(disc, PVDS, start, sizes[VDS_SIZE]);
	start = next_extent_size(find_extent(disc, 256), USPACE, sizes[LVID_SIZE], offsets[LVID_SIZE]);
	set_extent(disc, LVID, start, sizes[LVID_SIZE]);
	if (disc->flags & FLAG_VAT)
	{
		start = next_extent_size(find_extent(disc, 256), USPACE, sizes[VDS_SIZE], offsets[VDS_SIZE]);
		set_extent(disc, RVDS, start, sizes[VDS_SIZE]);
	}
	else
	{
		start = prev_extent_size(disc->tail, USPACE, sizes[VDS_SIZE], offsets[VDS_SIZE]);
		set_extent(disc, RVDS, start, sizes[VDS_SIZE]);
	}

	if ((spm = find_type2_sparable_partition(disc, 0)))
	{
		for (i=0; i<spm->numSparingTables; i++)
		{
			if (i & 0x1)
				start = prev_extent_size(disc->tail, USPACE, sizes[STABLE_SIZE], offsets[STABLE_SIZE]);
			else
				start = next_extent_size(find_extent(disc, 256), USPACE, sizes[STABLE_SIZE], offsets[STABLE_SIZE]);
			set_extent(disc, STABLE, start, sizes[STABLE_SIZE]);
		}
		start = next_extent_size(find_extent(disc, 256), USPACE, sizes[SSPACE_SIZE], offsets[SSPACE_SIZE]);
		set_extent(disc, SSPACE, start, sizes[SSPACE_SIZE]);
	}

	start = next_extent(disc->head, LVID)->start;
	ext = next_extent(find_extent(disc, start), USPACE);
	if (ext->start % offsets[PSPACE_SIZE])
	{
		start = ext->start + offsets[PSPACE_SIZE] - (ext->start % offsets[PSPACE_SIZE]);
		size = ext->blocks - offsets[PSPACE_SIZE] + (ext->start % offsets[PSPACE_SIZE]);
	}
	else
	{
		start = ext->start;
		size = ext->blocks;
	}
	if (size % offsets[PSPACE_SIZE])
		size -= (size % offsets[PSPACE_SIZE]);
	set_extent(disc, PSPACE, start, size);
	for (i=0; i<disc->udf_lvd[0]->numPartitionMaps; i++)
	{
		if (i == 1)
			disc->udf_lvid->freeSpaceTable[i] = cpu_to_le32(0xFFFFFFFF);
		else
			disc->udf_lvid->freeSpaceTable[i] = cpu_to_le32(size);
	}
	for (j=0; j<disc->udf_lvd[0]->numPartitionMaps; j++)
	{
		if (j == 1)
			disc->udf_lvid->sizeTable[i+j] = cpu_to_le32(0xFFFFFFFF);
		else
			disc->udf_lvid->sizeTable[i+j] = cpu_to_le32(size);
	}
}

void dump_space(struct udf_disc *disc)
{
	struct udf_extent *start_ext;
	int i;

	start_ext = disc->head;

	while (start_ext != NULL)
	{
		printf("start=%d, blocks=%d, type=", start_ext->start, start_ext->blocks);
		for (i=0; i<UDF_SPACE_TYPE_SIZE; i++)
		{
			if (start_ext->space_type & (1<<i))
				printf("%s ", udf_space_type_str[i]);
		}
		printf("\n");
		start_ext = start_ext->next;
	}
}

int write_disc(struct udf_disc *disc)
{
	struct udf_extent *start_ext;
	int ret;

	start_ext = disc->head;

	while (start_ext != NULL)
	{
		if ((ret = disc->write(disc, start_ext)) < 0)
			return ret;
		start_ext = start_ext->next;
	}
}

void setup_vrs(struct udf_disc *disc)
{
	struct udf_extent *ext;
	struct udf_desc *desc;

	if (!(ext = next_extent(disc->head, VRS)))
		return;
	desc = set_desc(disc, ext, 0x00, 0, sizeof(struct volStructDesc), NULL);
	disc->udf_vrs[0] = (struct volStructDesc *)desc->data->buffer;
	disc->udf_vrs[0]->structType = 0x00;
	disc->udf_vrs[0]->structVersion = 0x01;
	memcpy(disc->udf_vrs[0]->stdIdent, VSD_STD_ID_BEA01, VSD_STD_ID_LEN);

	if (disc->blocksize >= 2048)
		desc = set_desc(disc, ext, 0x00, 1, sizeof(struct volStructDesc), NULL);
	else
		desc = set_desc(disc, ext, 0x00, 2048 / disc->blocksize, sizeof(struct volStructDesc), NULL);
	disc->udf_vrs[1] = (struct volStructDesc *)desc->data->buffer;
	disc->udf_vrs[1]->structType = 0x00;
	disc->udf_vrs[1]->structVersion = 0x01;
	if (disc->udf_rev >= 0x0200)
		memcpy(disc->udf_vrs[1]->stdIdent, VSD_STD_ID_NSR03, VSD_STD_ID_LEN);
	else
		memcpy(disc->udf_vrs[1]->stdIdent, VSD_STD_ID_NSR02, VSD_STD_ID_LEN);

	if (disc->blocksize >= 2048)
		desc = set_desc(disc, ext, 0x00, 2, sizeof(struct volStructDesc), NULL);
	else
		desc = set_desc(disc, ext, 0x00, 4096 / disc->blocksize, sizeof(struct volStructDesc), NULL);
	disc->udf_vrs[2] = (struct volStructDesc *)desc->data->buffer;
	disc->udf_vrs[2]->structType = 0x00;
	disc->udf_vrs[2]->structVersion = 0x01;
	memcpy(disc->udf_vrs[2]->stdIdent, VSD_STD_ID_TEA01, VSD_STD_ID_LEN);
}

void setup_anchor(struct udf_disc *disc)
{
	struct udf_extent *ext;
	uint32_t mloc, rloc, mlen, rlen;
	int i = 0;

	ext = next_extent(disc->head, PVDS);
	mloc = ext->start;
	mlen = ext->blocks << disc->blocksize_bits;

	ext = next_extent(disc->head, RVDS);
	rloc = ext->start;
	rlen = ext->blocks << disc->blocksize_bits;

	ext = next_extent(disc->head, ANCHOR);
	do
	{
		ext->head = ext->tail = malloc(sizeof(struct udf_desc) + sizeof(struct udf_data));
		ext->head->data = (struct udf_data *)&(ext->head)[1];
		ext->head->data->next = ext->head->data->prev = NULL;
		ext->head->ident = TAG_IDENT_AVDP;
		ext->head->offset = 0;
		ext->head->length = ext->head->data->length = sizeof(struct anchorVolDescPtr);
		disc->udf_anchor[i] = ext->head->data->buffer = malloc(sizeof(struct anchorVolDescPtr));
		ext->head->next = ext->head->prev = NULL;
		disc->udf_anchor[i]->mainVolDescSeqExt.extLocation = cpu_to_le32(mloc);
		disc->udf_anchor[i]->mainVolDescSeqExt.extLength = cpu_to_le32(mlen);
		disc->udf_anchor[i]->reserveVolDescSeqExt.extLocation = cpu_to_le32(rloc);
		disc->udf_anchor[i]->reserveVolDescSeqExt.extLength = cpu_to_le32(rlen);
		disc->udf_anchor[i]->descTag = query_tag(disc, ext, ext->head, 1);
		ext = next_extent(ext->next, ANCHOR);
	} while (i++, ext != NULL);
}

void setup_partition(struct udf_disc *disc)
{
	struct udf_extent *vat, *pspace;

	pspace = next_extent(disc->head, PSPACE);
	setup_space(disc, pspace, 0);
	setup_fileset(disc, pspace);
	setup_root(disc, pspace);
	if (disc->flags & FLAG_VAT)
		setup_vat(disc, pspace);
}

int setup_space(struct udf_disc *disc, struct udf_extent *pspace, uint32_t offset)
{
	struct udf_desc *desc;
	struct partitionHeaderDesc *phd = (struct partitionHeaderDesc *)disc->udf_pd[0]->partitionContentsUse;
	int length = (((sizeof(struct spaceBitmapDesc) + pspace->blocks) >> (disc->blocksize_bits + 3)) + 1) << disc->blocksize_bits;

	if (disc->flags & FLAG_FREED_BITMAP)
	{
		phd->freedSpaceBitmap.extPosition = cpu_to_le32(offset);
		phd->freedSpaceBitmap.extLength = cpu_to_le32(length);
		disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(le32_to_cpu(disc->udf_lvid->freeSpaceTable[0]) - (length >> disc->blocksize_bits));
	}
	else if (disc->flags & FLAG_FREED_TABLE)
	{
		phd->freedSpaceTable.extPosition = cpu_to_le32(offset);
		if (disc->flags & FLAG_STRATEGY4096)
		{
			phd->freedSpaceTable.extLength = cpu_to_le32(disc->blocksize * 2);
			disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(le32_to_cpu(disc->udf_lvid->freeSpaceTable[0]) - 2);
		}
		else
		{
			phd->freedSpaceTable.extLength = cpu_to_le32(disc->blocksize);
			disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(le32_to_cpu(disc->udf_lvid->freeSpaceTable[0]) - 1);
		}
	}
	else if (disc->flags & FLAG_UNALLOC_BITMAP)
	{
		phd->unallocSpaceBitmap.extPosition = cpu_to_le32(offset);
		phd->unallocSpaceBitmap.extLength = cpu_to_le32(length);
		disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(le32_to_cpu(disc->udf_lvid->freeSpaceTable[0]) - (length >> disc->blocksize_bits));
	}
	else if (disc->flags & FLAG_UNALLOC_TABLE)
	{
		phd->unallocSpaceTable.extPosition = cpu_to_le32(offset);
		if (disc->flags & FLAG_STRATEGY4096)
		{
			phd->unallocSpaceTable.extLength = cpu_to_le32(disc->blocksize * 2);
			disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(le32_to_cpu(disc->udf_lvid->freeSpaceTable[0]) - 2);
		}
		else
		{
			phd->unallocSpaceTable.extLength = cpu_to_le32(disc->blocksize);
			disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(le32_to_cpu(disc->udf_lvid->freeSpaceTable[0]) - 1);
		}
	}

	if (disc->flags & FLAG_SPACE_BITMAP)
	{
		struct spaceBitmapDesc *sbd;
		int nBytes = (pspace->blocks+7)/8;

		length = sizeof(struct spaceBitmapDesc) + nBytes;
		desc = set_desc(disc, pspace, TAG_IDENT_SBD, offset, length, NULL);
		sbd = (struct spaceBitmapDesc *)desc->data->buffer;
		sbd->numOfBits = cpu_to_le32(pspace->blocks);
		sbd->numOfBytes = cpu_to_le32(nBytes);
		memset(sbd->bitmap, 0xFF, sizeof(uint8_t) * nBytes);
		if (pspace->blocks%8)
			sbd->bitmap[nBytes-1] = 0xFF >> (8-(pspace->blocks%8));
		clear_bits(sbd->bitmap, offset, (length + disc->blocksize - 1) >> disc->blocksize_bits);
		sbd->descTag = udf_query_tag(disc, TAG_IDENT_SBD, 1, desc->offset, desc->data, sizeof(tag));
	}
	else if (disc->flags & FLAG_SPACE_TABLE)
	{
		struct unallocSpaceEntry *use;
		short_ad *sad;
		int max = (0x3FFFFFFF / disc->blocksize) * disc->blocksize;
		int pos;
		long long rem;

		if (disc->flags & FLAG_STRATEGY4096)
			length = disc->blocksize * 2;
		else
			length = disc->blocksize;
		desc = set_desc(disc, pspace, TAG_IDENT_USE, offset, disc->blocksize, NULL);
		use = (struct unallocSpaceEntry *)desc->data->buffer;
		use->lengthAllocDescs = cpu_to_le32(sizeof(short_ad));
		sad = (short_ad *)&use->allocDescs[0];
		rem = (long long)pspace->blocks * disc->blocksize - length;
		if (disc->blocksize - sizeof(struct unallocSpaceEntry) < (rem / max) * sizeof(short_ad))
		pos = offset + (length/disc->blocksize);
		printf("pos=%d, rem=%lld\n", pos, rem);
		if (rem > 0x3FFFFFFF)
		{
			while (rem > max)
			{
				sad->extLength = cpu_to_le32(EXT_NOT_RECORDED_ALLOCATED | max);
				sad->extPosition = cpu_to_le32(pos);
				pos += max / disc->blocksize;
				sad ++;
				rem -= max;
				use->lengthAllocDescs = cpu_to_le32(le32_to_cpu(use->lengthAllocDescs) + sizeof(short_ad));
			}
		}
		sad->extLength = cpu_to_le32(EXT_NOT_RECORDED_ALLOCATED | rem);
		sad->extPosition = cpu_to_le32(pos);

		if (disc->flags & FLAG_STRATEGY4096)
		{
			use->icbTag.strategyType = cpu_to_le16(4096);
			use->icbTag.strategyParameter = cpu_to_le16(1);
			use->icbTag.numEntries = cpu_to_le16(2);
		}
		else
		{
			use->icbTag.strategyType = cpu_to_le16(4);
			use->icbTag.numEntries = cpu_to_le16(1);
		}
		use->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(0);
		use->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
		use->icbTag.fileType = ICBTAG_FILE_TYPE_USE;
		use->icbTag.flags = cpu_to_le16(ICBTAG_FLAG_AD_SHORT);
		use->descTag = udf_query_tag(disc, TAG_IDENT_USE, 1, desc->offset, desc->data, sizeof(struct unallocSpaceEntry) + le32_to_cpu(use->lengthAllocDescs));

		if (disc->flags & FLAG_STRATEGY4096)
		{
			struct udf_desc *tdesc;
			struct terminalEntry *te;

			if (disc->flags & FLAG_BLANK_TERMINAL)
			{
//				tdesc = set_desc(disc, pspace, TAG_IDENT_IE, offset+1, sizeof(struct indirectEntry), NULL);
			}
			else
			{
				tdesc = set_desc(disc, pspace, TAG_IDENT_TE, offset+1, sizeof(struct terminalEntry), NULL);
				te = (struct terminalEntry *)tdesc->data->buffer;
				te->icbTag.priorRecordedNumDirectEntries = cpu_to_le32(1);
				te->icbTag.strategyType = cpu_to_le16(4096);
				te->icbTag.strategyParameter = cpu_to_le16(1);
				te->icbTag.numEntries = cpu_to_le16(2);
				te->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(desc->offset);
				te->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
				te->icbTag.fileType = ICBTAG_FILE_TYPE_TE;
				te->descTag = query_tag(disc, pspace, tdesc, 1);
			}
		}
	}

	return (length + disc->blocksize - 1) >> disc->blocksize_bits;
}

int setup_fileset(struct udf_disc *disc, struct udf_extent *pspace)
{
	uint32_t offset = 0;
	struct udf_desc *desc;
	int length = sizeof(struct fileSetDesc);

	offset = udf_alloc_blocks(disc, pspace, offset, 1);

	((long_ad *)disc->udf_lvd[0]->logicalVolContentsUse)->extLength = cpu_to_le32(disc->blocksize);
	((long_ad *)disc->udf_lvd[0]->logicalVolContentsUse)->extLocation.logicalBlockNum = cpu_to_le32(offset);
	((long_ad *)disc->udf_lvd[0]->logicalVolContentsUse)->extLocation.partitionReferenceNum = cpu_to_le16(0);
//	((uint16_t *)disc->udf_fsd->domainIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);

	desc = set_desc(disc, pspace, TAG_IDENT_FSD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_fsd;

	if (!(disc->flags & FLAG_VAT) && disc->udf_rev >= 0x0200)
	{
		struct udf_desc *ss;

		ss = udf_create(disc, pspace, NULL, 0, offset+1, NULL, FID_FILE_CHAR_DIRECTORY, ICBTAG_FILE_TYPE_STREAMDIR, 0);
		insert_fid(disc, pspace, ss, ss, NULL, 0, FID_FILE_CHAR_DIRECTORY | FID_FILE_CHAR_PARENT);
		offset = ss->offset;

		disc->udf_fsd->streamDirectoryICB.extLength = cpu_to_le32(disc->blocksize);
		disc->udf_fsd->streamDirectoryICB.extLocation.logicalBlockNum = cpu_to_le32(offset);
		disc->udf_fsd->streamDirectoryICB.extLocation.partitionReferenceNum = cpu_to_le16(0);
		
	}

	disc->udf_fsd->descTag = query_tag(disc, pspace, desc, 1);

	return (length + disc->blocksize - 1) >> disc->blocksize_bits;
}

int setup_root(struct udf_disc *disc, struct udf_extent *pspace)
{
	uint32_t offset = 0;
	struct udf_desc *desc, *fsd_desc, *tdesc;
	struct terminalEntry *te;

	desc = udf_mkdir(disc, pspace, NULL, 0, offset, NULL);
	offset = desc->offset;

	if (disc->flags & FLAG_STRATEGY4096)
		disc->udf_fsd->rootDirectoryICB.extLength = cpu_to_le32(disc->blocksize * 2);
	else
		disc->udf_fsd->rootDirectoryICB.extLength = cpu_to_le32(disc->blocksize);
	disc->udf_fsd->rootDirectoryICB.extLocation.logicalBlockNum = cpu_to_le32(offset);
	disc->udf_fsd->rootDirectoryICB.extLocation.partitionReferenceNum = cpu_to_le16(0);
	fsd_desc = next_desc(pspace->head, TAG_IDENT_FSD);
	disc->udf_fsd->descTag = query_tag(disc, pspace, fsd_desc, 1);

	if (disc->flags & FLAG_STRATEGY4096)
	{
		if (disc->flags & FLAG_BLANK_TERMINAL)
		{
//			tdesc = set_desc(disc, pspace, TAG_IDENT_IE, offset+1, sizeof(struct indirectEntry), NULL);
			offset ++;
		}
		else
		{
			tdesc = set_desc(disc, pspace, TAG_IDENT_TE, offset+1, sizeof(struct terminalEntry), NULL);
			te = (struct terminalEntry *)tdesc->data->buffer;
			te->icbTag.priorRecordedNumDirectEntries = cpu_to_le32(1);
			te->icbTag.strategyType = cpu_to_le16(4096);
			te->icbTag.strategyParameter = cpu_to_le16(1);
			te->icbTag.numEntries = cpu_to_le16(2);
			te->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(desc->offset);
			te->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			te->icbTag.fileType = ICBTAG_FILE_TYPE_TE;
			te->descTag = query_tag(disc, pspace, tdesc, 1);
			offset = tdesc->offset;
		}
	}

	if (next_extent(disc->head, STABLE) && next_extent(disc->head, SSPACE))
	{
		struct udf_desc *nat;
		if (disc->udf_rev == 0x0150)
		{
			struct fileEntry *fe;
			nat = udf_create(disc, pspace, "\x08" "Non-Allocatable Space", 22, offset+1, desc, FID_FILE_CHAR_HIDDEN, ICBTAG_FILE_TYPE_REGULAR, ICBTAG_FLAG_SYSTEM);
			fe = (struct fileEntry *)nat->data->buffer;
			fe->icbTag.flags = cpu_to_le16((le16_to_cpu(fe->icbTag.flags) & ~ICBTAG_FLAG_AD_MASK) | ICBTAG_FLAG_AD_SHORT);
			fe->descTag = query_tag(disc, pspace, nat, 1);
			offset = nat->offset;
		}
		else
		{
			struct extendedFileEntry *efe;
			struct udf_desc *ss;

			ss = find_desc(pspace, le32_to_cpu(disc->udf_fsd->streamDirectoryICB.extLocation.logicalBlockNum));
#if 0
			nat = udf_create(disc, pspace, NULL, 0, offset+1, NULL, FID_FILE_CHAR_DIRECTORY, ICBTAG_FILE_TYPE_STREAMDIR, 0);
			insert_fid(disc, pspace, nat, nat, NULL, 0, FID_FILE_CHAR_DIRECTORY | FID_FILE_CHAR_PARENT);
			offset = nat->offset;

			disc->udf_fsd->streamDirectoryICB.extLength = cpu_to_le32(disc->blocksize);
			disc->udf_fsd->streamDirectoryICB.extLocation.logicalBlockNum = cpu_to_le32(offset);
			disc->udf_fsd->streamDirectoryICB.extLocation.partitionReferenceNum = cpu_to_le16(0);
#endif
			nat = udf_create(disc, pspace, "\x08" "*UDF Non-Allocatable Space", 27, offset+1, ss, FID_FILE_CHAR_METADATA, ICBTAG_FILE_TYPE_REGULAR, ICBTAG_FLAG_STREAM | ICBTAG_FLAG_SYSTEM);

			efe = (struct extendedFileEntry *)nat->data->buffer;
			efe->icbTag.flags = cpu_to_le16((le16_to_cpu(efe->icbTag.flags) & ~ICBTAG_FLAG_AD_MASK) | ICBTAG_FLAG_AD_SHORT);
			efe->descTag = query_tag(disc, pspace, nat, 1);
			offset = nat->offset;
		}

	}

	desc = udf_mkdir(disc, pspace, "\x08" "lost+found", 11, offset+1, desc);
	offset = desc->offset;

	if (disc->flags & FLAG_STRATEGY4096)
	{
		if (disc->flags & FLAG_BLANK_TERMINAL)
		{
//			tdesc = set_desc(disc, pspace, TAG_IDENT_IE, offset+1, sizeof(struct indirectEntry), NULL);
			offset ++;
		}
		else
		{
			tdesc = set_desc(disc, pspace, TAG_IDENT_TE, offset+1, sizeof(struct terminalEntry), NULL);
			te = (struct terminalEntry *)tdesc->data->buffer;
			te->icbTag.priorRecordedNumDirectEntries = cpu_to_le32(1);
			te->icbTag.strategyType = cpu_to_le16(4096);
			te->icbTag.strategyParameter = cpu_to_le16(1);
			te->icbTag.numEntries = cpu_to_le16(2);
			te->icbTag.parentICBLocation.logicalBlockNum = cpu_to_le32(desc->offset);
			te->icbTag.parentICBLocation.partitionReferenceNum = cpu_to_le16(0);
			te->icbTag.fileType = ICBTAG_FILE_TYPE_TE;
			te->descTag = query_tag(disc, pspace, tdesc, 1);
			offset = tdesc->offset;
		}
	}

	if (disc->flags & FLAG_STRATEGY4096)
		return 4;
	else
		return 2;
}

void setup_vds(struct udf_disc *disc)
{
	struct udf_extent *pvds, *rvds, *lvid, *stable[4], *sspace;

	pvds = next_extent(disc->head, PVDS);
	rvds = next_extent(disc->head, RVDS);
	lvid = next_extent(disc->head, LVID);
	stable[0] = next_extent(disc->head, STABLE);
	sspace = next_extent(disc->head, SSPACE);

	setup_pvd(disc, pvds, rvds, 0);
	setup_lvid(disc, lvid);
	if (stable[0] && sspace)
	{
		int i;

		for (i=1; i<4 && stable[i-1]; i++)
		{
			stable[i] = next_extent(stable[i-1]->next, STABLE);
		}
		setup_stable(disc, stable, sspace);
	}
	setup_lvd(disc, pvds, rvds, lvid, 1);
	setup_pd(disc, pvds, rvds, 2);
	setup_usd(disc, pvds, rvds, 3);
	setup_iuvd(disc, pvds, rvds, 4);
	if (pvds->blocks > 5)
		setup_td(disc, pvds, rvds, 5);
}

void setup_pvd(struct udf_disc *disc, struct udf_extent *pvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct primaryVolDesc);

	desc = set_desc(disc, pvds, TAG_IDENT_PVD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_pvd[0];
	disc->udf_pvd[0]->descTag = query_tag(disc, pvds, desc, 1);

	desc = set_desc(disc, rvds, TAG_IDENT_PVD, offset, length, NULL);
	memcpy(disc->udf_pvd[1] = desc->data->buffer, disc->udf_pvd[0], length);
	disc->udf_pvd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_lvd(struct udf_disc *disc, struct udf_extent *pvds, struct udf_extent *rvds, struct udf_extent *lvid, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct logicalVolDesc) + le32_to_cpu(disc->udf_lvd[0]->mapTableLength);

	disc->udf_lvd[0]->integritySeqExt.extLength = cpu_to_le32(lvid->blocks * disc->blocksize);
	disc->udf_lvd[0]->integritySeqExt.extLocation = cpu_to_le32(lvid->start);
//	((uint16_t *)disc->udf_lvd[0]->domainIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);

	desc = set_desc(disc, pvds, TAG_IDENT_LVD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_lvd[0];
	disc->udf_lvd[0]->descTag = query_tag(disc, pvds, desc, 1);

	desc = set_desc(disc, rvds, TAG_IDENT_LVD, offset, length, NULL);
	memcpy(disc->udf_lvd[1] = desc->data->buffer, disc->udf_lvd[0], length);
	disc->udf_lvd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_pd(struct udf_disc *disc, struct udf_extent *pvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	struct udf_extent *ext;
	int length = sizeof(struct partitionDesc);

	ext = next_extent(disc->head, PSPACE);
	disc->udf_pd[0]->partitionStartingLocation = cpu_to_le32(ext->start);
	disc->udf_pd[0]->partitionLength = cpu_to_le32(ext->blocks);
#if 0
	if (disc->udf_rev >= 0x0200)
		strcpy(disc->udf_pd[0]->partitionContents.ident, PARTITION_CONTENTS_NSR03);
	else
		strcpy(disc->udf_pd[0]->partitionContents.ident, PARTITION_CONTENTS_NSR02);
#endif

	desc = set_desc(disc, pvds, TAG_IDENT_PD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_pd[0];
	disc->udf_pd[0]->descTag = query_tag(disc, pvds, desc, 1);

	desc = set_desc(disc, rvds, TAG_IDENT_PD, offset, length, NULL);
	memcpy(disc->udf_pd[1] = desc->data->buffer, disc->udf_pd[0], length);
	disc->udf_pd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_usd(struct udf_disc *disc, struct udf_extent *pvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	struct udf_extent *ext;
	int count = 0;
	int length = sizeof(struct unallocSpaceDesc);

	ext = next_extent(disc->head, USPACE);
	while (ext)
	{
		length += sizeof(extent_ad);
		disc->udf_usd[0] = realloc(disc->udf_usd[0], length);
		disc->udf_usd[0]->numAllocDescs = cpu_to_le32(le32_to_cpu(disc->udf_usd[0]->numAllocDescs)+1);
		disc->udf_usd[0]->allocDescs[count].extLength = cpu_to_le32(ext->blocks * disc->blocksize);
		disc->udf_usd[0]->allocDescs[count].extLocation = cpu_to_le32(ext->start);
		count ++;
		ext = next_extent(ext->next, USPACE);
	}

	desc = set_desc(disc, pvds, TAG_IDENT_USD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_usd[0];
	disc->udf_usd[0]->descTag = query_tag(disc, pvds, desc, 1);

	desc = set_desc(disc, rvds, TAG_IDENT_USD, offset, length, NULL);
	memcpy(disc->udf_usd[1] = desc->data->buffer, disc->udf_usd[0], length);
	disc->udf_usd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_iuvd(struct udf_disc *disc, struct udf_extent *pvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct impUseVolDesc);

//	((uint16_t *)disc->udf_iuvd[0]->impIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);

	desc = set_desc(disc, pvds, TAG_IDENT_IUVD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_iuvd[0];
	disc->udf_iuvd[0]->descTag = query_tag(disc, pvds, desc, 1);

	desc = set_desc(disc, rvds, TAG_IDENT_IUVD, offset, length, NULL);
	memcpy(disc->udf_iuvd[1] = desc->data->buffer, disc->udf_iuvd[0], length);
	disc->udf_iuvd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_td(struct udf_disc *disc, struct udf_extent *pvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct terminatingDesc);

	desc = set_desc(disc, pvds, TAG_IDENT_TD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_td[0];
	disc->udf_td[0]->descTag = query_tag(disc, pvds, desc, 1);

	desc = set_desc(disc, rvds, TAG_IDENT_TD, offset, length, NULL);
	memcpy(disc->udf_td[1] = desc->data->buffer, disc->udf_td[0], length);
	disc->udf_td[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_lvid(struct udf_disc *disc, struct udf_extent *lvid)
{
	struct udf_desc *desc;
	struct udf_extent *ext;
	int length = sizeof(struct logicalVolIntegrityDesc) + le32_to_cpu(disc->udf_lvid->numOfPartitions) * sizeof(uint32_t) * 2 + le32_to_cpu(disc->udf_lvid->lengthOfImpUse);

	ext = next_extent(disc->head, PSPACE);
//	disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(ext->blocks);
//	disc->udf_lvid->sizeTable[1] = cpu_to_le32(ext->blocks);
	if (disc->flags & FLAG_VAT)
		disc->udf_lvid->integrityType = cpu_to_le32(LVID_INTEGRITY_TYPE_OPEN);
	desc = set_desc(disc, lvid, TAG_IDENT_LVID, 0, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_lvid;
	disc->udf_lvid->descTag = query_tag(disc, lvid, desc, 1);

	if (!(disc->flags & FLAG_BLANK_TERMINAL) && lvid->blocks > 1)
	{
		desc = set_desc(disc, lvid, TAG_IDENT_TD, 1, sizeof(struct terminatingDesc), NULL);
		((struct terminatingDesc *)desc->data->buffer)->descTag = query_tag(disc, lvid, desc, 1);
	}
}

void setup_stable(struct udf_disc *disc, struct udf_extent *stable[4], struct udf_extent *sspace)
{
	struct udf_desc *desc;
	int i, length = 0, num, packetlen;
	struct sparablePartitionMap *spm;

	spm = find_type2_sparable_partition(disc, 0);
	packetlen = le16_to_cpu(spm->packetLength);
	num = sspace->blocks / packetlen;
	length = sizeof(struct sparingTable) + num * sizeof(struct sparingEntry);
	spm->sizeSparingTable = cpu_to_le32(length);
	for (i=0; i<spm->numSparingTables; i++)
		spm->locSparingTable[i] = cpu_to_le32(stable[i]->start);

	disc->udf_stable[0] = realloc(disc->udf_stable[0], length);
	disc->udf_stable[0]->reallocationTableLen = cpu_to_le16(num);
	for (i=0; i<num; i++)
	{
		disc->udf_stable[0]->mapEntry[i].origLocation = cpu_to_le32(0xFFFFFFFF);
		disc->udf_stable[0]->mapEntry[i].mappedLocation = cpu_to_le32(sspace->start + (i * packetlen));
	}
	desc = set_desc(disc, stable[0], 0, 0, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_stable[0];
	disc->udf_stable[0]->descTag = query_tag(disc, stable[0], desc, 1);

	for (i=1; i<4 && stable[i]; i++)
	{
		desc = set_desc(disc, stable[i], 0, 0, length, NULL);
		memcpy(disc->udf_stable[i] = desc->data->buffer, disc->udf_stable[0], length);
		disc->udf_stable[i]->descTag = query_tag(disc, stable[i], desc, 1);
	}
}

void setup_vat(struct udf_disc *disc, struct udf_extent *ext)
{
	uint32_t offset = 0;
	struct udf_desc *vtable, *desc;
	struct udf_data *data;
	struct fileEntry *fe;
	struct extendedFileEntry *efe;
	struct fileIdentDesc *fid;
	uint32_t *vsector, len;
	struct virtualAllocationTable15 *vat15;
	struct virtualAllocationTable20 *vat20;

	ext = disc->tail;

	if (disc->udf_rev >= 0x0200)
	{
		vtable = udf_create(disc, ext, UDF_ID_ALLOC, strlen(UDF_ID_ALLOC), offset, NULL, FID_FILE_CHAR_HIDDEN, ICBTAG_FILE_TYPE_VAT20, 0);
		len = sizeof(struct virtualAllocationTable20);
		data = alloc_data(&default_vat20, len);
		vat20 = data->buffer;
		vat20->numFiles = query_lvidiu(disc)->numFiles;
		vat20->numDirs = query_lvidiu(disc)->numDirs;
		vat20->logicalVolIdent[127] = strlen(vat20->logicalVolIdent);
		insert_data(disc, ext, vtable, data);
		data = alloc_data(disc->vat, disc->vat_entries * sizeof(uint32_t));
		insert_data(disc, ext, vtable, data);
	}
	else
	{
		vtable = udf_create(disc, ext, UDF_ID_ALLOC, strlen(UDF_ID_ALLOC), offset, NULL, FID_FILE_CHAR_HIDDEN, ICBTAG_FILE_TYPE_UNDEF, 0);
		len = sizeof(struct virtualAllocationTable15);
		data = alloc_data(disc->vat, disc->vat_entries * sizeof(uint32_t));
		insert_data(disc, ext, vtable, data);
		data = alloc_data(&default_vat15, len);
		vat15 = data->buffer;
		((uint16_t *)vat15->vatIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);
		insert_data(disc, ext, vtable, data);
	}
}

void add_type1_partition(struct udf_disc *disc, uint16_t partitionNum)
{
	struct genericPartitionMap1 *pm;
	int mtl = le32_to_cpu(disc->udf_lvd[0]->mapTableLength);
	int npm = le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps);

	disc->udf_lvd[0] = realloc(disc->udf_lvd[0],
		sizeof(struct logicalVolDesc) + mtl +
		sizeof(struct genericPartitionMap1));

	pm = (struct genericPartitionMap1 *)&disc->udf_lvd[0]->partitionMaps[mtl];
	mtl += sizeof(struct genericPartitionMap1);

	disc->udf_lvd[0]->mapTableLength = cpu_to_le32(mtl);
	disc->udf_lvd[0]->numPartitionMaps = cpu_to_le32(npm + 1);
	pm->partitionMapType = 1;
	pm->partitionMapLength = sizeof(struct genericPartitionMap1);
	pm->volSeqNum = cpu_to_le16(1);
	pm->partitionNum = cpu_to_le16(partitionNum);

	disc->udf_lvid->numOfPartitions = cpu_to_le32(npm + 1);
	disc->udf_lvid = realloc(disc->udf_lvid,
		sizeof(struct logicalVolIntegrityDesc) +
		sizeof(uint32_t) * 2 * (npm + 1) +
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->impUse[sizeof(uint32_t) * 2 * (npm + 1)],
		&disc->udf_lvid->impUse[sizeof(uint32_t) * 2 * npm],
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->impUse[sizeof(uint32_t) * (npm + 1)],
		&disc->udf_lvid->impUse[sizeof(uint32_t) * npm],
		sizeof(uint32_t));
}

void add_type2_sparable_partition(struct udf_disc *disc, uint16_t partitionNum, uint8_t spartable, uint16_t packetlen)
{
	struct sparablePartitionMap *pm;
	int mtl = le32_to_cpu(disc->udf_lvd[0]->mapTableLength);
	int npm = le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps);

	disc->udf_lvd[0] = realloc(disc->udf_lvd[0],
		sizeof(struct logicalVolDesc) + mtl +
		sizeof(struct sparablePartitionMap));

	pm = (struct sparablePartitionMap *)&disc->udf_lvd[0]->partitionMaps[mtl];
	mtl += sizeof(struct sparablePartitionMap);

	disc->udf_lvd[0]->mapTableLength = cpu_to_le32(mtl);
	disc->udf_lvd[0]->numPartitionMaps = cpu_to_le32(npm + 1);
	memcpy(pm, &default_sparmap, sizeof(struct sparablePartitionMap));
	pm->partitionNum = cpu_to_le16(partitionNum);
	((uint16_t *)pm->partIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);
	if (packetlen)
		pm->packetLength = cpu_to_le16(packetlen);
	pm->numSparingTables = spartable;
	pm->sizeSparingTable = cpu_to_le32(sizeof(struct sparingTable));

	disc->udf_lvid->numOfPartitions = cpu_to_le32(npm + 1);
	disc->udf_lvid = realloc(disc->udf_lvid,
		sizeof(struct logicalVolIntegrityDesc) +
		sizeof(uint32_t) * 2 * (npm + 1) +
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->impUse[sizeof(uint32_t) * 2 * (npm + 1)],
		&disc->udf_lvid->impUse[sizeof(uint32_t) * 2 * npm],
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->impUse[sizeof(uint32_t) * (npm + 1)],
		&disc->udf_lvid->impUse[sizeof(uint32_t) * npm],
		sizeof(uint32_t));
}

struct sparablePartitionMap *find_type2_sparable_partition(struct udf_disc *disc, uint16_t partitionNum)
{
	int i, npm, mtl = 0;
	struct genericPartitionMap *pm;
	struct udfPartitionMap2 *pm2;
	struct sparablePartitionMap *spm;

	npm = le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps);

	for (i=0; i<npm; i++)
	{
		pm = (struct genericPartitionMap *)&disc->udf_lvd[0]->partitionMaps[mtl];
		if (pm->partitionMapType == 2)
		{
			pm2 = (struct udfPartitionMap2 *)&disc->udf_lvd[0]->partitionMaps[mtl];
			if (!strncmp(pm2->partIdent.ident, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE)))
			{
				spm = (struct sparablePartitionMap *)&disc->udf_lvd[0]->partitionMaps[mtl];
				if (le16_to_cpu(spm->partitionNum) == partitionNum)
					return spm;
			}
		}
		mtl += pm->partitionMapLength;
	}
	return NULL;
}

void add_type2_virtual_partition(struct udf_disc *disc, uint16_t partitionNum)
{
	struct virtualPartitionMap *pm;
	int mtl = le32_to_cpu(disc->udf_lvd[0]->mapTableLength);
	int npm = le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps);

	disc->udf_lvd[0] = realloc(disc->udf_lvd[0],
		sizeof(struct logicalVolDesc) + mtl +
		sizeof(struct virtualPartitionMap));

	pm = (struct virtualPartitionMap *)&disc->udf_lvd[0]->partitionMaps[mtl];
	mtl += sizeof(struct virtualPartitionMap);

	disc->udf_lvd[0]->mapTableLength = cpu_to_le32(mtl);
	disc->udf_lvd[0]->numPartitionMaps = cpu_to_le32(npm + 1);
	memcpy(pm, &default_virtmap, sizeof(struct virtualPartitionMap));
	pm->partitionNum = cpu_to_le16(partitionNum);
	((uint16_t *)pm->partIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);

	disc->udf_lvid->numOfPartitions = cpu_to_le32(npm + 1);
	disc->udf_lvid = realloc(disc->udf_lvid,
		sizeof(struct logicalVolIntegrityDesc) +
		sizeof(uint32_t) * 2 * (npm + 1) +
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->impUse[sizeof(uint32_t) * 2 * (npm + 1)],
		&disc->udf_lvid->impUse[sizeof(uint32_t) * 2 * npm],
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->impUse[sizeof(uint32_t) * (npm + 1)],
		&disc->udf_lvid->impUse[sizeof(uint32_t) * npm],
		sizeof(uint32_t));
}
	

char *udf_space_type_str[UDF_SPACE_TYPE_SIZE] = { "RESERVED", "VRS", "ANCHOR", "PVDS", "RVDS", "LVID", "STABLE", "SSPACE", "PSPACE", "USPACE", "BAD" };
