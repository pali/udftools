/*
 * mkudffs.c
 *
 * Copyright (c) 2001-2002  Ben Fennema
 * Copyright (c) 2014-2018  Pali Rohár <pali.rohar@gmail.com>
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
 * mkudffs support functions
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>

#include "mkudffs.h"
#include "file.h"
#include "defaults.h"

void udf_init_disc(struct udf_disc *disc)
{
	timestamp	ts;
	struct timeval	tv;
	struct tm 	*tm;
	int		altzone;
	uint32_t	uuid_time;
	char		uuid[17];

	memset(disc, 0x00, sizeof(*disc));

	disc->blocksize = 2048;
	disc->udf_rev = le16_to_cpu(default_lvidiu.minUDFReadRev);
	disc->flags = FLAG_LOCALE | FLAG_CLOSED | FLAG_EFE;
	disc->blkssz = 512;
	disc->mode = 0755;

	if (gettimeofday(&tv, NULL) != 0 || tv.tv_sec == (time_t)-1 || (tm = localtime(&tv.tv_sec)) == NULL || tm->tm_year < 1-1900 || tm->tm_year > 9999-1900)
	{
		/* fallback to 1.1.1980 00:00:00 */
		ts.typeAndTimezone = cpu_to_le16(0x1000);
		ts.year = cpu_to_le16(1980);
		ts.month = 1;
		ts.day = 1;
		ts.hour = 0;
		ts.minute = 0;
		ts.second = 0;
		ts.centiseconds = 0;
		ts.hundredsOfMicroseconds = 0;
		ts.microseconds = 0;
		/* and for uuid use random bytes */
		uuid_time = randu32();
	}
	else
	{
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
		if (tv.tv_sec < 0)
			uuid_time = randu32();
		else
			uuid_time = tv.tv_sec & 0xFFFFFFFF;
	}

	/* Allocate/Initialize Descriptors */
	disc->udf_pvd[0] = malloc(sizeof(struct primaryVolDesc));
	memcpy(disc->udf_pvd[0], &default_pvd, sizeof(struct primaryVolDesc));
	memcpy(&disc->udf_pvd[0]->recordingDateAndTime, &ts, sizeof(timestamp));
	snprintf(uuid, sizeof(uuid), "%08" PRIu32 "%08" PRIu32, uuid_time, randu32());
	memcpy(&disc->udf_pvd[0]->volSetIdent[1], uuid, 16);
	disc->udf_pvd[0]->volIdent[31] = strlen((char *)disc->udf_pvd[0]->volIdent);
	disc->udf_pvd[0]->volSetIdent[127] = strlen((char *)disc->udf_pvd[0]->volSetIdent);

	disc->udf_lvd[0] = malloc(sizeof(struct logicalVolDesc));
	memcpy(disc->udf_lvd[0], &default_lvd, sizeof(struct logicalVolDesc));
	disc->udf_lvd[0]->logicalVolIdent[127] = strlen((char *)disc->udf_lvd[0]->logicalVolIdent);

	disc->udf_pd[0] = malloc(sizeof(struct partitionDesc));
	memcpy(disc->udf_pd[0], &default_pd, sizeof(struct partitionDesc));

	disc->udf_usd[0] = malloc(sizeof(struct unallocSpaceDesc));
	memcpy(disc->udf_usd[0], &default_usd, sizeof(struct unallocSpaceDesc));

	disc->udf_iuvd[0] = malloc(sizeof(struct impUseVolDesc) + sizeof(struct impUseVolDescImpUse));
	memcpy(disc->udf_iuvd[0], &default_iuvd, sizeof(struct impUseVolDesc));
	memcpy(query_iuvdiu(disc), &default_iuvdiu, sizeof(struct impUseVolDescImpUse));
	query_iuvdiu(disc)->logicalVolIdent[127] = strlen((char *)query_iuvdiu(disc)->logicalVolIdent);
	query_iuvdiu(disc)->LVInfo1[35] = strlen((char *)query_iuvdiu(disc)->LVInfo1);
	query_iuvdiu(disc)->LVInfo2[35] = strlen((char *)query_iuvdiu(disc)->LVInfo2);
	query_iuvdiu(disc)->LVInfo3[35] = strlen((char *)query_iuvdiu(disc)->LVInfo3);

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
	disc->udf_fsd->logicalVolIdent[127] = strlen((char *)disc->udf_fsd->logicalVolIdent);
	disc->udf_fsd->fileSetIdent[31] = strlen((char *)disc->udf_fsd->fileSetIdent);
	disc->udf_fsd->copyrightFileIdent[31] = strlen((char *)disc->udf_fsd->copyrightFileIdent);
	disc->udf_fsd->abstractFileIdent[31] = strlen((char *)disc->udf_fsd->abstractFileIdent);

	disc->head = malloc(sizeof(struct udf_extent));
	disc->tail = disc->head;

	disc->head->space_type = USPACE;
	disc->head->start = 0;
	disc->head->blocks = 0;
	disc->head->next = NULL;
	disc->head->prev = NULL;
	disc->head->head = NULL;
	disc->head->tail = NULL;
}

int udf_set_version(struct udf_disc *disc, uint16_t udf_rev)
{
	struct logicalVolIntegrityDescImpUse *lvidiu;
	uint16_t udf_rev_le16;

	if (disc->udf_rev == udf_rev)
		return 0;
	else if (
		udf_rev != 0x0101 &&
		udf_rev != 0x0102 &&
		udf_rev != 0x0150 &&
		udf_rev != 0x0200 &&
		udf_rev != 0x0201 &&
		udf_rev != 0x0250 &&
		udf_rev != 0x0260)
	{
		return 1;
	}
	else
		disc->udf_rev = udf_rev;

	if (disc->udf_rev >= 0x0200)

	{
		disc->flags |= FLAG_EFE;
		strcpy((char *)disc->udf_pd[0]->partitionContents.ident, PD_PARTITION_CONTENTS_NSR03);
	}
	else if (disc->udf_rev == 0x0150)
	{
		disc->flags &= ~FLAG_EFE;
		strcpy((char *)disc->udf_pd[0]->partitionContents.ident, PD_PARTITION_CONTENTS_NSR02);
	}
	else // 0x0102 and older
	{
		disc->flags &= ~FLAG_VAT;
		disc->flags &= ~FLAG_EFE;
		strcpy((char *)disc->udf_pd[0]->partitionContents.ident, PD_PARTITION_CONTENTS_NSR02);
	}

	udf_rev_le16 = cpu_to_le16(udf_rev);
	memcpy(disc->udf_fsd->domainIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
	memcpy(disc->udf_lvd[0]->domainIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
	memcpy(disc->udf_iuvd[0]->impIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
	memcpy(disc->udf_stable[0]->sparingIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
	lvidiu = query_lvidiu(disc);
	if (udf_rev < 0x0102)
	{
		/* The ImplementationUse area for the Logical Volume Integrity Descriptor
		 * prior to UDF 1.02 does not define UDF revisions, so clear these fields */
		lvidiu->minUDFReadRev = cpu_to_le16(0);
		lvidiu->minUDFWriteRev = cpu_to_le16(0);
		lvidiu->maxUDFWriteRev = cpu_to_le16(0);
	}
	else
	{
		if (udf_rev == 0x0260)
			lvidiu->minUDFReadRev = cpu_to_le16(0x0250);
		else
			lvidiu->minUDFReadRev = cpu_to_le16(udf_rev);
		lvidiu->minUDFWriteRev = cpu_to_le16(udf_rev);
		lvidiu->maxUDFWriteRev = cpu_to_le16(udf_rev);
	}
	return 0;
}

void split_space(struct udf_disc *disc)
{
	uint32_t sizes[UDF_ALLOC_TYPE_SIZE];
	uint32_t offsets[UDF_ALLOC_TYPE_SIZE];
	uint32_t blocks = disc->blocks;
	uint32_t start, size, start2, size2;
	struct sparablePartitionMap *spm;
	struct udf_extent *ext;
	uint32_t accessType;
	uint32_t i, value;

	// OS boot area
	if (disc->flags & FLAG_BOOTAREA_MBR)
		set_extent(disc, MBR, 0, 1);
	else
		set_extent(disc, RESERVED, 0, 32768 / disc->blocksize);

	// Volume Recognition Sequence
	if (disc->blocksize >= 2048)
		set_extent(disc, VRS, (2048 * 16) / disc->blocksize, 4); // 3 sectors for VSD + one empty sector
	else
		set_extent(disc, VRS, (2048 * 16) / disc->blocksize, ((2048 * 3) + disc->blocksize) / disc->blocksize); // 3 VSD in more sectors + one empty sector

	// First Anchor Point at sector 256
	if (blocks > 257)
		set_extent(disc, ANCHOR, 256, 1);

	// Second anchor point at sector (End-Of-Volume - 256)
	if (disc->flags & FLAG_CLOSED)
	{
		if (disc->flags & FLAG_VAT)
		{
			if (blocks <= 257 || blocks-257 <= 256)
			{
				fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
				exit(1);
			}
			set_extent(disc, ANCHOR, 257, 1);
		}
		else if (blocks >= 3072)
		{
			set_extent(disc, ANCHOR, blocks-257, 1);
		}
	}

	// Final anchor point at sector End-Of-Volume/Session for sequentially writable media
	if (!(disc->flags & FLAG_VAT))
		set_extent(disc, ANCHOR, blocks-1, 1);

	// Calculate minimal size for Sparing Table needed for Sparing Space
	if ((spm = find_type2_sparable_partition(disc, 0)) && disc->sizing[STABLE_SIZE].minSize == 0)
		disc->sizing[STABLE_SIZE].minSize = (sizeof(struct sparingTable) + disc->sizing[SSPACE_SIZE].minSize / le16_to_cpu(spm->packetLength) * sizeof(struct sparingEntry) + disc->blocksize-1) / disc->blocksize;

	for (i=0; i<UDF_ALLOC_TYPE_SIZE; i++)
	{
		sizes[i] = disc->sizing[i].numSize * blocks / disc->sizing[i].denomSize;
		if (disc->sizing[i].minSize > sizes[i])
			sizes[i] = disc->sizing[i].minSize;
		offsets[i] = disc->sizing[i].align;
	}

	accessType = le32_to_cpu(disc->udf_pd[0]->accessType);
	if ((accessType == PD_ACCESS_TYPE_OVERWRITABLE || accessType == PD_ACCESS_TYPE_REWRITABLE) && (uint64_t)sizes[LVID_SIZE] * disc->blocksize < 8192 && blocks > 257)
		sizes[LVID_SIZE] = (8192 + disc->blocksize-1) / disc->blocksize;

	if (sizes[VDS_SIZE] > 6 && blocks <= 257)
		sizes[VDS_SIZE] = 6;

	if (!(disc->flags & FLAG_VAT) && blocks < 770)
		start = 0;
	else
		start = 96;
	start = find_next_extent_size(disc, start, USPACE, sizes[VDS_SIZE], offsets[VDS_SIZE]);
	if (!start)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}
	set_extent(disc, MVDS, start, sizes[VDS_SIZE]);

	if (disc->flags & FLAG_VAT)
	{
		start = find_next_extent_size(disc, (256-sizes[VDS_SIZE])/offsets[VDS_SIZE]*offsets[VDS_SIZE], USPACE, sizes[VDS_SIZE], offsets[VDS_SIZE]);
		if (!start)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		set_extent(disc, RVDS, start, sizes[VDS_SIZE]);
	}
	else if (blocks > 257)
	{
		if (blocks >= 3072)
			start = find_next_extent_size(disc, (blocks-257+97)/32*32, USPACE, sizes[VDS_SIZE], offsets[VDS_SIZE]);
		else
			start = prev_extent_size(disc->tail, USPACE, sizes[VDS_SIZE], offsets[VDS_SIZE]);
		if (!start)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		set_extent(disc, RVDS, start, sizes[VDS_SIZE]);
	}

	start = prev_extent_size(disc->tail, USPACE, sizes[LVID_SIZE], offsets[LVID_SIZE]);
	if (start < 256 || blocks >= 770)
	{
		if (!(disc->flags & FLAG_VAT) && blocks < 770)
			start = 0;
		else
			start = 128;
		start = find_next_extent_size(disc, start, USPACE, sizes[LVID_SIZE], offsets[LVID_SIZE]);
		if (!start)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		set_extent(disc, LVID, start, sizes[LVID_SIZE]);
	}

	if ((spm = find_type2_sparable_partition(disc, 0)))
	{
		for (i=0; i<spm->numSparingTables; i++)
		{
			if (i == 0)
				start = find_next_extent_size(disc, next_extent(disc->head, MVDS)->start, USPACE, sizes[STABLE_SIZE], offsets[STABLE_SIZE]);
			else if (i == 1)
				start = prev_extent_size(disc->tail, USPACE, sizes[STABLE_SIZE], offsets[STABLE_SIZE]);
			else if (i == 2)
				start = prev_extent_size(next_extent(disc->head, ANCHOR), USPACE, sizes[STABLE_SIZE], offsets[STABLE_SIZE]);
			else
				start = find_next_extent_size(disc, prev_extent(disc->tail->prev, ANCHOR)->start, USPACE, sizes[STABLE_SIZE], offsets[STABLE_SIZE]);
			if (!start)
			{
				fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
				exit(1);
			}
			set_extent(disc, STABLE, start, sizes[STABLE_SIZE]);
		}
		start = find_next_extent_size(disc, next_extent(disc->head, MVDS)->start, USPACE, sizes[SSPACE_SIZE], offsets[SSPACE_SIZE]);
		if (!start)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		set_extent(disc, SSPACE, start, sizes[SSPACE_SIZE]);
	}

	start2 = 0;
	size2 = 0;
	for (i = 0; i < 3; ++i)
	{
		if (i != 0 && ((disc->flags & FLAG_VAT) || blocks >= 770))
			break;
		if (i == 0)
			ext = next_extent(find_extent(disc, 256), USPACE);
		else if (i == 1)
			ext = prev_extent(disc->tail, USPACE);
		else
			ext = next_extent(disc->head, USPACE);
		for (; ext; ext = next_extent(ext->next, USPACE))
		{
			// round start up to a multiple of alignment/packet_size
			if (ext->start % offsets[PSPACE_SIZE])
			{
				if (offsets[PSPACE_SIZE] >= ext->blocks + (ext->start % offsets[PSPACE_SIZE]))
					continue;
				start = ext->start + offsets[PSPACE_SIZE] - (ext->start % offsets[PSPACE_SIZE]);
				size = ext->blocks - offsets[PSPACE_SIZE] + (ext->start % offsets[PSPACE_SIZE]);
			}
			else
			{
				start = ext->start;
				size = ext->blocks;
			}

			// round size down to a multiple of alignment/packet_size
			if (size % offsets[PSPACE_SIZE])
				size -= (size % offsets[PSPACE_SIZE]);

			if (size == 0)
				continue;

			if (size2 < size)
			{
				start2 = start;
				size2 = size;
			}
		}
	}
	start = start2;
	size = size2;

	if (size == 0)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}

	set_extent(disc, PSPACE, start, size);

	for (i=0; i<le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps); i++)
	{
		if (i == 1)
			value = cpu_to_le32(0xFFFFFFFF);
		else
			value = cpu_to_le32(size);
		memcpy(&disc->udf_lvid->data[sizeof(uint32_t)*i], &value, sizeof(value));
	}
	for (i=0; i<le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps); i++)
	{
		if (i == 1)
			value = cpu_to_le32(0xFFFFFFFF);
		else
			value = cpu_to_le32(size);
		memcpy(&disc->udf_lvid->data[sizeof(uint32_t)*le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps) + sizeof(uint32_t)*i], &value, sizeof(value));
	}

	if (!next_extent(disc->head, LVID))
	{
		start = find_next_extent_size(disc, 0, USPACE, sizes[LVID_SIZE], offsets[LVID_SIZE]);
		if (!start)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		set_extent(disc, LVID, start, sizes[LVID_SIZE]);
	}
}

void dump_space(struct udf_disc *disc)
{
	struct udf_extent *start_ext;
	int i;

	start_ext = disc->head;

	while (start_ext != NULL)
	{
		printf("start=%"PRIu32", blocks=%"PRIu32", type=", start_ext->start, start_ext->blocks);
		for (i=0; i<UDF_SPACE_TYPE_SIZE; i++)
		{
			if (!(start_ext->space_type & (1<<i)))
				continue;
			if ((start_ext->space_type & (USPACE|RESERVED)) && !(disc->flags & FLAG_BOOTAREA_PRESERVE))
				printf("ERASE ");
			else
				printf("%s ", udf_space_type_str[i]);
		}
		printf("\n");
		start_ext = start_ext->next;
	}
}

int write_disc(struct udf_disc *disc)
{
	struct udf_extent *start_ext;
	int ret=0;

	start_ext = disc->head;

	while (start_ext != NULL)
	{
		if ((ret = disc->write(disc, start_ext)) < 0)
			return ret;
		start_ext = start_ext->next;
	}
	return ret;
}

static void fill_mbr(struct udf_disc *disc, struct mbr *mbr, uint32_t start)
{
	struct mbr old_mbr;
	uint64_t lba_blocks;
	struct stat st;
	struct hd_geometry geometry;
	unsigned int heads, sectors;
	struct mbr_partition *mbr_partition;
	int fd = disc->write_data ? (*(int *)disc->write_data) : -1;

	memcpy(mbr, &default_mbr, sizeof(struct mbr));
	mbr_partition = &mbr->partitions[0];

	if (fd >= 0 && lseek(fd, ((off_t)start) * disc->blocksize, SEEK_SET) >= 0)
	{
		if (read(fd, &old_mbr, sizeof(struct mbr)) == sizeof(struct mbr))
		{
			if (old_mbr.boot_signature == constant_cpu_to_le16(MBR_BOOT_SIGNATURE))
				mbr->disk_signature = le32_to_cpu(old_mbr.disk_signature);
		}
	}

	if (!mbr->disk_signature)
		mbr->disk_signature = le32_to_cpu(randu32());

	lba_blocks = ((uint64_t)disc->blocks * disc->blocksize + disc->blkssz - 1) / disc->blkssz;

	if (fd >= 0 && fstat(fd, &st) == 0 && S_ISBLK(st.st_mode) && ioctl(fd, HDIO_GETGEO, &geometry) == 0)
	{
		heads = geometry.heads;
		sectors = geometry.sectors;
	}
	else
	{
		/* Use LBA-Assist Translation for calculating CHS when disk geometry is not available */
		sectors = 63;
		if (lba_blocks < 16*63*1024)
			heads = 16;
		else if (lba_blocks < 32*63*1024)
			heads = 32;
		else if (lba_blocks < 64*63*1024)
			heads = 64;
		else if (lba_blocks < 128*63*1024)
			heads = 128;
		else
			heads = 255;
	}

	if (heads > 255 || sectors > 63 || lba_blocks >= heads*sectors*1024)
	{
		/* If CHS address is too large use tuple (1023, 254, 63) */
		mbr_partition->ending_chs[0] = 254;
		mbr_partition->ending_chs[1] = 255;
		mbr_partition->ending_chs[2] = 255;
	}
	else
	{
		mbr_partition->ending_chs[0] = (lba_blocks / sectors) % heads;
		mbr_partition->ending_chs[1] = ((1 + lba_blocks % sectors) & 63) | (((lba_blocks / (heads*sectors)) >> 8) * 64);
		mbr_partition->ending_chs[2] = (lba_blocks / (heads*sectors)) & 255;
	}

	mbr_partition->size_in_lba = cpu_to_le32(lba_blocks);
}

void setup_mbr(struct udf_disc *disc)
{
	struct udf_extent *ext;
	struct udf_desc *desc;
	struct mbr *mbr;

	if (!(ext = next_extent(disc->head, MBR)))
		return;
	desc = set_desc(ext, 0x00, 0, ext->blocks * disc->blocksize, NULL);
	mbr = (struct mbr *)desc->data->buffer;
	fill_mbr(disc, mbr, ext->start);
}

void setup_vrs(struct udf_disc *disc)
{
	struct udf_extent *ext;
	struct udf_desc *desc;

	if (!(ext = next_extent(disc->head, VRS)))
		return;
	desc = set_desc(ext, 0x00, 0, sizeof(struct volStructDesc), NULL);
	disc->udf_vrs[0] = (struct volStructDesc *)desc->data->buffer;
	disc->udf_vrs[0]->structType = 0x00;
	disc->udf_vrs[0]->structVersion = 0x01;
	memcpy(disc->udf_vrs[0]->stdIdent, VSD_STD_ID_BEA01, VSD_STD_ID_LEN);

	if (disc->blocksize >= 2048)
		desc = set_desc(ext, 0x00, 1, sizeof(struct volStructDesc), NULL);
	else
		desc = set_desc(ext, 0x00, 2048 / disc->blocksize, sizeof(struct volStructDesc), NULL);
	disc->udf_vrs[1] = (struct volStructDesc *)desc->data->buffer;
	disc->udf_vrs[1]->structType = 0x00;
	disc->udf_vrs[1]->structVersion = 0x01;
	if (disc->udf_rev >= 0x0200)
		memcpy(disc->udf_vrs[1]->stdIdent, VSD_STD_ID_NSR03, VSD_STD_ID_LEN);
	else
		memcpy(disc->udf_vrs[1]->stdIdent, VSD_STD_ID_NSR02, VSD_STD_ID_LEN);

	if (disc->blocksize >= 2048)
		desc = set_desc(ext, 0x00, 2, sizeof(struct volStructDesc), NULL);
	else
		desc = set_desc(ext, 0x00, 4096 / disc->blocksize, sizeof(struct volStructDesc), NULL);
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

	ext = next_extent(disc->head, MVDS);
	if (!ext)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}
	mloc = ext->start;
	mlen = ext->blocks * disc->blocksize;

	ext = next_extent(disc->head, RVDS);
	if (!ext && disc->blocks > 257)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}
	if (disc->blocks > 257)
	{
		rloc = ext->start;
		rlen = ext->blocks * disc->blocksize;
	}
	else
	{
		rloc = mloc;
		rlen = mlen;
	}

	ext = next_extent(disc->head, ANCHOR);
	if (!ext)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}
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
		memset(disc->udf_anchor[i]->reserved, 0, sizeof(disc->udf_anchor[i]->reserved));
		disc->udf_anchor[i]->descTag = query_tag(disc, ext, ext->head, 1);
		ext = next_extent(ext->next, ANCHOR);
	} while (i++, ext != NULL);
}

void setup_partition(struct udf_disc *disc)
{
	struct udf_extent *pspace;

	pspace = next_extent(disc->head, PSPACE);
	if (!pspace)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}
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
	uint32_t length = (sizeof(struct spaceBitmapDesc) + (pspace->blocks+7)/8 + disc->blocksize-1) / disc->blocksize * disc->blocksize;
	uint32_t value;

	memcpy(&value, &disc->udf_lvid->data[sizeof(uint32_t)*0], sizeof(value));

	if (disc->flags & FLAG_FREED_BITMAP)
	{
		phd->freedSpaceBitmap.extPosition = cpu_to_le32(offset);
		phd->freedSpaceBitmap.extLength = cpu_to_le32(length);
		value = cpu_to_le32(le32_to_cpu(value) - (length / disc->blocksize));
	}
	else if (disc->flags & FLAG_FREED_TABLE)
	{
		phd->freedSpaceTable.extPosition = cpu_to_le32(offset);
		if (disc->flags & FLAG_STRATEGY4096)
		{
			phd->freedSpaceTable.extLength = cpu_to_le32(disc->blocksize * 2);
			value = cpu_to_le32(le32_to_cpu(value) - 2);
		}
		else
		{
			phd->freedSpaceTable.extLength = cpu_to_le32(disc->blocksize);
			value = cpu_to_le32(le32_to_cpu(value) - 1);
		}
	}
	else if (disc->flags & FLAG_UNALLOC_BITMAP)
	{
		phd->unallocSpaceBitmap.extPosition = cpu_to_le32(offset);
		phd->unallocSpaceBitmap.extLength = cpu_to_le32(length);
		value = cpu_to_le32(le32_to_cpu(value) - (length / disc->blocksize));
	}
	else if (disc->flags & FLAG_UNALLOC_TABLE)
	{
		phd->unallocSpaceTable.extPosition = cpu_to_le32(offset);
		if (disc->flags & FLAG_STRATEGY4096)
		{
			phd->unallocSpaceTable.extLength = cpu_to_le32(disc->blocksize * 2);
			value = cpu_to_le32(le32_to_cpu(value) - 2);
		}
		else
		{
			phd->unallocSpaceTable.extLength = cpu_to_le32(disc->blocksize);
			value = cpu_to_le32(le32_to_cpu(value) - 1);
		}
	}

	memcpy(&disc->udf_lvid->data[sizeof(uint32_t)*0], &value, sizeof(value));

	if (disc->flags & FLAG_SPACE_BITMAP)
	{
		struct spaceBitmapDesc *sbd;
		int nBytes = (pspace->blocks+7)/8;

		length = sizeof(struct spaceBitmapDesc) + nBytes;
		desc = set_desc(pspace, TAG_IDENT_SBD, offset, length, NULL);
		sbd = (struct spaceBitmapDesc *)desc->data->buffer;
		sbd->numOfBits = cpu_to_le32(pspace->blocks);
		sbd->numOfBytes = cpu_to_le32(nBytes);
		memset(sbd->bitmap, 0xFF, sizeof(uint8_t) * nBytes);
		if (pspace->blocks%8)
			sbd->bitmap[nBytes-1] = 0xFF >> (8-(pspace->blocks%8));
		clear_bits(sbd->bitmap, offset, (length + disc->blocksize - 1) / disc->blocksize);
		sbd->descTag = udf_query_tag(disc, TAG_IDENT_SBD, 1, desc->offset, desc->data, 0, sizeof(struct spaceBitmapDesc));
	}
	else if (disc->flags & FLAG_SPACE_TABLE)
	{
		struct unallocSpaceEntry *use;
		short_ad *sad;
		uint32_t max_value = (UINT32_MAX & EXT_LENGTH_MASK);
		uint32_t max = (max_value / disc->blocksize) * disc->blocksize;
		uint32_t pos=0;
		uint64_t rem;

		if (disc->flags & FLAG_STRATEGY4096)
			length = disc->blocksize * 2;
		else
			length = disc->blocksize;
		desc = set_desc(pspace, TAG_IDENT_USE, offset, disc->blocksize, NULL);
		use = (struct unallocSpaceEntry *)desc->data->buffer;
		use->lengthAllocDescs = cpu_to_le32(sizeof(short_ad));
		sad = (short_ad *)&use->allocDescs[0];
		rem = (uint64_t)pspace->blocks * disc->blocksize - length;
		if (disc->blocksize - sizeof(struct unallocSpaceEntry) < (rem / max) * sizeof(short_ad))
		{
			fprintf(stderr, "%s: Error: Creation of so large filesystems with unalloc table not supported.\n", appname);
			exit(1);
		}
		pos = offset + (length/disc->blocksize);
		if (rem > max_value)
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
		use->descTag = udf_query_tag(disc, TAG_IDENT_USE, 1, desc->offset, desc->data, 0, sizeof(struct unallocSpaceEntry) + le32_to_cpu(use->lengthAllocDescs));

		if (disc->flags & FLAG_STRATEGY4096)
		{
			struct udf_desc *tdesc;
			struct terminalEntry *te;

			if (disc->flags & FLAG_BLANK_TERMINAL)
			{
//				tdesc = set_desc(pspace, TAG_IDENT_IE, offset+1, sizeof(struct indirectEntry), NULL);
			}
			else
			{
				tdesc = set_desc(pspace, TAG_IDENT_TE, offset+1, sizeof(struct terminalEntry), NULL);
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

	return (length + disc->blocksize - 1) / disc->blocksize;
}

int setup_fileset(struct udf_disc *disc, struct udf_extent *pspace)
{
	uint32_t offset = 0;
	struct udf_desc *desc;
	int length = sizeof(struct fileSetDesc);
	long_ad ad;

	offset = udf_alloc_blocks(disc, pspace, offset, 1);

	memset(&ad, 0, sizeof(ad));
	ad.extLength = cpu_to_le32(disc->blocksize);
	ad.extLocation.logicalBlockNum = cpu_to_le32(offset);
	if (disc->flags & FLAG_VAT)
		ad.extLocation.partitionReferenceNum = cpu_to_le16(1);
	else
		ad.extLocation.partitionReferenceNum = cpu_to_le16(0);
	memcpy(disc->udf_lvd[0]->logicalVolContentsUse, &ad, sizeof(ad));

	desc = set_desc(pspace, TAG_IDENT_FSD, offset, 0, NULL);
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

	return (length + disc->blocksize - 1) / disc->blocksize;
}

int setup_root(struct udf_disc *disc, struct udf_extent *pspace)
{
	uint32_t offset = 0;
	struct udf_desc *desc, *fsd_desc, *tdesc;
	struct terminalEntry *te;

	desc = udf_mkdir(disc, pspace, NULL, 0, offset, NULL); // the root directory does not have a name
	offset = desc->offset;

	if (disc->flags & FLAG_STRATEGY4096)
		disc->udf_fsd->rootDirectoryICB.extLength = cpu_to_le32(disc->blocksize * 2);
	else
		disc->udf_fsd->rootDirectoryICB.extLength = cpu_to_le32(disc->blocksize);
	disc->udf_fsd->rootDirectoryICB.extLocation.logicalBlockNum = cpu_to_le32(offset);
	if (disc->flags & FLAG_VAT)
		disc->udf_fsd->rootDirectoryICB.extLocation.partitionReferenceNum = cpu_to_le16(1);
	else
		disc->udf_fsd->rootDirectoryICB.extLocation.partitionReferenceNum = cpu_to_le16(0);
	fsd_desc = next_desc(pspace->head, TAG_IDENT_FSD);
	disc->udf_fsd->descTag = query_tag(disc, pspace, fsd_desc, 1);

	if (disc->flags & FLAG_STRATEGY4096)
	{
		if (disc->flags & FLAG_BLANK_TERMINAL)
		{
//			tdesc = set_desc(pspace, TAG_IDENT_IE, offset+1, sizeof(struct indirectEntry), NULL);
			offset ++;
		}
		else
		{
			tdesc = set_desc(pspace, TAG_IDENT_TE, offset+1, sizeof(struct terminalEntry), NULL);
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

		if (disc->flags & FLAG_EFE)
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
			nat = udf_create(disc, pspace, (const dchars *)"\x08" "*UDF Non-Allocatable Space", 27, offset+1, ss, FID_FILE_CHAR_METADATA, ICBTAG_FILE_TYPE_REGULAR, ICBTAG_FLAG_STREAM | ICBTAG_FLAG_SYSTEM);

			efe = (struct extendedFileEntry *)nat->data->buffer;
			efe->icbTag.flags = cpu_to_le16((le16_to_cpu(efe->icbTag.flags) & ~ICBTAG_FLAG_AD_MASK) | ICBTAG_FLAG_AD_SHORT);
			efe->descTag = query_tag(disc, pspace, nat, 1);
			offset = nat->offset;
		}
		else
		{
			struct fileEntry *fe;
			nat = udf_create(disc, pspace, (const dchars *)"\x08" "Non-Allocatable Space", 22, offset+1, desc, FID_FILE_CHAR_HIDDEN, ICBTAG_FILE_TYPE_REGULAR, ICBTAG_FLAG_SYSTEM);
			fe = (struct fileEntry *)nat->data->buffer;
			fe->icbTag.flags = cpu_to_le16((le16_to_cpu(fe->icbTag.flags) & ~ICBTAG_FLAG_AD_MASK) | ICBTAG_FLAG_AD_SHORT);
			fe->descTag = query_tag(disc, pspace, nat, 1);
			offset = nat->offset;
		}
	}

#if 0 // this works fine if you really want a lost+find directory on disc
	desc = udf_mkdir(disc, pspace, (const dchars *)"\x08" "lost+found", 11, offset+1, desc);
	offset = desc->offset;

	if (disc->flags & FLAG_STRATEGY4096)
	{
		if (disc->flags & FLAG_BLANK_TERMINAL)
		{
//			tdesc = set_desc(pspace, TAG_IDENT_IE, offset+1, sizeof(struct indirectEntry), NULL);
			offset ++;
		}
		else
		{
			tdesc = set_desc(pspace, TAG_IDENT_TE, offset+1, sizeof(struct terminalEntry), NULL);
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
#endif
	if (disc->flags & FLAG_STRATEGY4096)
		return 4;
	else
		return 2;
}

void setup_vds(struct udf_disc *disc)
{
	struct udf_extent *mvds, *rvds, *lvid, *stable[4], *sspace;

	mvds = next_extent(disc->head, MVDS);
	rvds = next_extent(disc->head, RVDS);
	lvid = next_extent(disc->head, LVID);
	stable[0] = next_extent(disc->head, STABLE);
	sspace = next_extent(disc->head, SSPACE);

	if (!mvds || (!rvds && disc->blocks > 257) || !lvid)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}

	setup_pvd(disc, mvds, rvds, 0);
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
	setup_lvd(disc, mvds, rvds, lvid, 1);
	setup_pd(disc, mvds, rvds, 2);
	setup_usd(disc, mvds, rvds, 3);
	setup_iuvd(disc, mvds, rvds, 4);
	setup_td(disc, mvds, rvds, 5);
}

void setup_pvd(struct udf_disc *disc, struct udf_extent *mvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct primaryVolDesc);

	desc = set_desc(mvds, TAG_IDENT_PVD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_pvd[0];
	disc->udf_pvd[0]->descTag = query_tag(disc, mvds, desc, 1);

	if (!rvds)
		return;
	desc = set_desc(rvds, TAG_IDENT_PVD, offset, length, NULL);
	memcpy(disc->udf_pvd[1] = desc->data->buffer, disc->udf_pvd[0], length);
	disc->udf_pvd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_lvd(struct udf_disc *disc, struct udf_extent *mvds, struct udf_extent *rvds, struct udf_extent *lvid, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct logicalVolDesc) + le32_to_cpu(disc->udf_lvd[0]->mapTableLength);

	disc->udf_lvd[0]->integritySeqExt.extLength = cpu_to_le32(lvid->blocks * disc->blocksize);
	disc->udf_lvd[0]->integritySeqExt.extLocation = cpu_to_le32(lvid->start);
//	((uint16_t *)disc->udf_lvd[0]->domainIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);

	desc = set_desc(mvds, TAG_IDENT_LVD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_lvd[0];
	disc->udf_lvd[0]->descTag = query_tag(disc, mvds, desc, 1);

	if (!rvds)
		return;
	desc = set_desc(rvds, TAG_IDENT_LVD, offset, length, NULL);
	memcpy(disc->udf_lvd[1] = desc->data->buffer, disc->udf_lvd[0], length);
	disc->udf_lvd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_pd(struct udf_disc *disc, struct udf_extent *mvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	struct udf_extent *ext;
	int length = sizeof(struct partitionDesc);

	ext = next_extent(disc->head, PSPACE);
	if (!ext)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}
	disc->udf_pd[0]->partitionStartingLocation = cpu_to_le32(ext->start);
	disc->udf_pd[0]->partitionLength = cpu_to_le32(ext->blocks);
#if 0
	if (disc->udf_rev >= 0x0200)
		strcpy(disc->udf_pd[0]->partitionContents.ident, PARTITION_CONTENTS_NSR03);
	else
		strcpy(disc->udf_pd[0]->partitionContents.ident, PARTITION_CONTENTS_NSR02);
#endif

	desc = set_desc(mvds, TAG_IDENT_PD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_pd[0];
	disc->udf_pd[0]->descTag = query_tag(disc, mvds, desc, 1);

	if (!rvds)
		return;
	desc = set_desc(rvds, TAG_IDENT_PD, offset, length, NULL);
	memcpy(disc->udf_pd[1] = desc->data->buffer, disc->udf_pd[0], length);
	disc->udf_pd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_usd(struct udf_disc *disc, struct udf_extent *mvds, struct udf_extent *rvds, uint32_t offset)
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

	desc = set_desc(mvds, TAG_IDENT_USD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_usd[0];
	disc->udf_usd[0]->descTag = query_tag(disc, mvds, desc, 1);

	if (!rvds)
		return;
	desc = set_desc(rvds, TAG_IDENT_USD, offset, length, NULL);
	memcpy(disc->udf_usd[1] = desc->data->buffer, disc->udf_usd[0], length);
	disc->udf_usd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_iuvd(struct udf_disc *disc, struct udf_extent *mvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct impUseVolDesc);

//	((uint16_t *)disc->udf_iuvd[0]->impIdent.identSuffix)[0] = cpu_to_le16(disc->udf_rev);

	desc = set_desc(mvds, TAG_IDENT_IUVD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_iuvd[0];
	disc->udf_iuvd[0]->descTag = query_tag(disc, mvds, desc, 1);

	if (!rvds)
		return;
	desc = set_desc(rvds, TAG_IDENT_IUVD, offset, length, NULL);
	memcpy(disc->udf_iuvd[1] = desc->data->buffer, disc->udf_iuvd[0], length);
	disc->udf_iuvd[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_td(struct udf_disc *disc, struct udf_extent *mvds, struct udf_extent *rvds, uint32_t offset)
{
	struct udf_desc *desc;
	int length = sizeof(struct terminatingDesc);

	desc = set_desc(mvds, TAG_IDENT_TD, offset, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_td[0];
	disc->udf_td[0]->descTag = query_tag(disc, mvds, desc, 1);

	if (!rvds)
		return;
	desc = set_desc(rvds, TAG_IDENT_TD, offset, length, NULL);
	memcpy(disc->udf_td[1] = desc->data->buffer, disc->udf_td[0], length);
	disc->udf_td[1]->descTag = query_tag(disc, rvds, desc, 1);
}

void setup_lvid(struct udf_disc *disc, struct udf_extent *lvid)
{
	struct udf_desc *desc;
//	struct udf_extent *ext;
	int length = sizeof(struct logicalVolIntegrityDesc) + le32_to_cpu(disc->udf_lvid->numOfPartitions) * sizeof(uint32_t) * 2 + le32_to_cpu(disc->udf_lvid->lengthOfImpUse);

//	ext = next_extent(disc->head, PSPACE);
//	disc->udf_lvid->freeSpaceTable[0] = cpu_to_le32(ext->blocks);
//	disc->udf_lvid->sizeTable[1] = cpu_to_le32(ext->blocks);
	if (disc->flags & FLAG_VAT)
		disc->udf_lvid->integrityType = cpu_to_le32(LVID_INTEGRITY_TYPE_OPEN);
	desc = set_desc(lvid, TAG_IDENT_LVID, 0, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_lvid;
	disc->udf_lvid->descTag = query_tag(disc, lvid, desc, 1);

	if (!(disc->flags & FLAG_BLANK_TERMINAL) && lvid->blocks > 1)
	{
		desc = set_desc(lvid, TAG_IDENT_TD, 1, sizeof(struct terminatingDesc), NULL);
		((struct terminatingDesc *)desc->data->buffer)->descTag = query_tag(disc, lvid, desc, 1);
	}
}

void setup_stable(struct udf_disc *disc, struct udf_extent *stable[4], struct udf_extent *sspace)
{
	struct udf_desc *desc;
	uint32_t i, num, length;
	uint16_t packetlen;
	struct sparablePartitionMap *spm;

	spm = find_type2_sparable_partition(disc, 0);
	if (!spm)
	{
		fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
		exit(1);
	}

	packetlen = le16_to_cpu(spm->packetLength);
	num = sspace->blocks / packetlen;
	if (num > UINT16_MAX)
		num = UINT16_MAX;
	length = sizeof(struct sparingTable) + num * sizeof(struct sparingEntry);

	if (length > stable[0]->blocks * disc->blocksize)
	{
		length = stable[0]->blocks * disc->blocksize;
		if (length < sizeof(struct sparingTable))
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		num = (length - sizeof(struct sparingTable)) / sizeof(struct sparingEntry);
		if (num > UINT16_MAX)
			num = UINT16_MAX;
		if (num == 0)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		length = sizeof(struct sparingTable) + num * sizeof(struct sparingEntry);
	}

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
	desc = set_desc(stable[0], 0, 0, 0, NULL);
	desc->length = desc->data->length = length;
	desc->data->buffer = disc->udf_stable[0];
	disc->udf_stable[0]->descTag = query_tag(disc, stable[0], desc, 1);

	for (i=1; i<4 && stable[i]; i++)
	{
		desc = set_desc(stable[i], 0, 0, length, NULL);
		memcpy(disc->udf_stable[i] = desc->data->buffer, disc->udf_stable[0], length);
		disc->udf_stable[i]->descTag = query_tag(disc, stable[i], desc, 1);
	}
}

void setup_vat(struct udf_disc *disc, struct udf_extent *pspace)
{
	uint32_t offset;
	struct udf_extent *anchor;
	struct udf_desc *vtable;
	struct udf_data *data;
	uint32_t len, i;
	struct virtualAllocationTable15 *vat15;
	struct virtualAllocationTable20 *vat20;
	struct impUseExtAttr *ea_attr;
	struct LVExtensionEA *ea_lv;
	uint8_t buffer[(sizeof(*ea_attr)+sizeof(*ea_lv)+3)/4*4];
	uint16_t checksum;
	uint16_t udf_rev_le16;
	uint32_t min_blocks;
	uint32_t align;

	/* Put VAT to the last sector correctly aligned */
	align = disc->sizing[PSPACE_SIZE].align;
	offset = pspace->tail->offset + (pspace->tail->length + disc->blocksize-1) / disc->blocksize;
	offset = (offset + align) / align * align - 1;

	if (disc->flags & FLAG_MIN_300_BLOCKS)
	{
		// On optical TAO discs one track has minimal size of 300 sectors
		min_blocks = (300 + align) / align * align - 1;
		if (pspace->start + offset < min_blocks)
			offset = min_blocks - pspace->start;
	}

	if (disc->flags & FLAG_CLOSED)
	{
		anchor = prev_extent(disc->tail, ANCHOR);
		if (pspace->start - anchor->start > 256)
		{
			fprintf(stderr, "%s: Error: Not enough blocks on device\n", appname);
			exit(1);
		}
		offset = 256 - (pspace->start - anchor->start);
	}

	if (disc->udf_rev >= 0x0200)
	{
		vtable = udf_create(disc, pspace, (const dchars *)"\x08" UDF_ID_ALLOC, strlen(UDF_ID_ALLOC)+1, offset, NULL, FID_FILE_CHAR_HIDDEN, ICBTAG_FILE_TYPE_VAT20, 0);
		disc->vat_entries--; // Remove VAT file itself from VAT table
		len = sizeof(struct virtualAllocationTable20);
		data = alloc_data(&default_vat20, len);
		vat20 = data->buffer;
		vat20->numFiles = query_lvidiu(disc)->numFiles;
		vat20->numDirs = query_lvidiu(disc)->numDirs;
		vat20->minUDFReadRev = query_lvidiu(disc)->minUDFReadRev;
		vat20->minUDFWriteRev = query_lvidiu(disc)->minUDFWriteRev;
		vat20->maxUDFWriteRev = query_lvidiu(disc)->maxUDFWriteRev;
		memcpy(vat20->logicalVolIdent, disc->udf_lvd[0]->logicalVolIdent, 128);
		insert_data(disc, pspace, vtable, data);
		data = alloc_data(disc->vat, disc->vat_entries * sizeof(uint32_t));
		insert_data(disc, pspace, vtable, data);
	}
	else
	{
		vtable = udf_create(disc, pspace, (const dchars *)"\x08" UDF_ID_ALLOC, strlen(UDF_ID_ALLOC)+1, offset, NULL, FID_FILE_CHAR_HIDDEN, ICBTAG_FILE_TYPE_UNDEF, 0);
		disc->vat_entries--; // Remove VAT file itself from VAT table
		udf_rev_le16 = cpu_to_le16(disc->udf_rev);
		memset(&buffer, 0, sizeof(buffer));
		ea_attr = (struct impUseExtAttr *)buffer;
		ea_attr->attrType = cpu_to_le32(EXTATTR_IMP_USE);
		ea_attr->attrSubtype = EXTATTR_SUBTYPE;
		ea_attr->attrLength = cpu_to_le32(sizeof(buffer));
		ea_attr->impUseLength = cpu_to_le32(sizeof(*ea_lv));
		ea_attr->impIdent.identSuffix[2] = UDF_OS_CLASS_UNIX;
		ea_attr->impIdent.identSuffix[3] = UDF_OS_ID_LINUX;
		memcpy(ea_attr->impIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
		strcpy((char *)ea_attr->impIdent.ident, UDF_ID_VAT_LVEXTENSION);
		ea_lv = (struct LVExtensionEA *)&ea_attr->impUse[0];
		checksum = 0;
		for (i = 0; i < sizeof(*ea_attr); ++i)
			checksum += ((uint8_t *)ea_attr)[i];
		ea_lv->headerChecksum = cpu_to_le16(checksum);
		if (disc->flags & FLAG_EFE)
		{
			struct extendedFileEntry *efe = (struct extendedFileEntry *)vtable->data->buffer;
			ea_lv->verificationID = efe->uniqueID;
		}
		else
		{
			struct fileEntry *fe = (struct fileEntry *)vtable->data->buffer;
			ea_lv->verificationID = fe->uniqueID;
		}
		ea_lv->numFiles = query_lvidiu(disc)->numFiles;
		ea_lv->numDirs = query_lvidiu(disc)->numDirs;
		memcpy(ea_lv->logicalVolIdent, disc->udf_lvd[0]->logicalVolIdent, 128);
		insert_ea(disc, vtable, (struct genericFormat *)buffer, sizeof(buffer));
		len = sizeof(struct virtualAllocationTable15);
		data = alloc_data(disc->vat, disc->vat_entries * sizeof(uint32_t));
		insert_data(disc, pspace, vtable, data);
		data = alloc_data(&default_vat15, len);
		vat15 = data->buffer;
		memcpy(vat15->vatIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
		insert_data(disc, pspace, vtable, data);
	}

	disc->vat_block = pspace->start + vtable->offset;
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
	memmove(&disc->udf_lvid->data[sizeof(uint32_t) * 2 * (npm + 1)],
		&disc->udf_lvid->data[sizeof(uint32_t) * 2 * npm],
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->data[sizeof(uint32_t) * (npm + 1)],
		&disc->udf_lvid->data[sizeof(uint32_t) * npm],
		sizeof(uint32_t));
}

void add_type2_sparable_partition(struct udf_disc *disc, uint16_t partitionNum, uint8_t spartable, uint16_t packetlen)
{
	struct sparablePartitionMap *pm;
	int mtl = le32_to_cpu(disc->udf_lvd[0]->mapTableLength);
	int npm = le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps);
	uint16_t udf_rev_le16 = cpu_to_le16(disc->udf_rev);

	disc->udf_lvd[0] = realloc(disc->udf_lvd[0],
		sizeof(struct logicalVolDesc) + mtl +
		sizeof(struct sparablePartitionMap));

	pm = (struct sparablePartitionMap *)&disc->udf_lvd[0]->partitionMaps[mtl];
	mtl += sizeof(struct sparablePartitionMap);

	disc->udf_lvd[0]->mapTableLength = cpu_to_le32(mtl);
	disc->udf_lvd[0]->numPartitionMaps = cpu_to_le32(npm + 1);
	memcpy(pm, &default_sparmap, sizeof(struct sparablePartitionMap));
	pm->partitionNum = cpu_to_le16(partitionNum);
	memcpy(pm->partIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
	if (packetlen)
		pm->packetLength = cpu_to_le16(packetlen);
	pm->numSparingTables = spartable;
	pm->sizeSparingTable = cpu_to_le32(sizeof(struct sparingTable));

	disc->udf_lvid->numOfPartitions = cpu_to_le32(npm + 1);
	disc->udf_lvid = realloc(disc->udf_lvid,
		sizeof(struct logicalVolIntegrityDesc) +
		sizeof(uint32_t) * 2 * (npm + 1) +
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->data[sizeof(uint32_t) * 2 * (npm + 1)],
		&disc->udf_lvid->data[sizeof(uint32_t) * 2 * npm],
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->data[sizeof(uint32_t) * (npm + 1)],
		&disc->udf_lvid->data[sizeof(uint32_t) * npm],
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
			if (!strncmp((char *)pm2->partIdent.ident, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE)))
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
	uint16_t udf_rev_le16 = cpu_to_le16(disc->udf_rev);

	disc->udf_lvd[0] = realloc(disc->udf_lvd[0],
		sizeof(struct logicalVolDesc) + mtl +
		sizeof(struct virtualPartitionMap));

	pm = (struct virtualPartitionMap *)&disc->udf_lvd[0]->partitionMaps[mtl];
	mtl += sizeof(struct virtualPartitionMap);

	disc->udf_lvd[0]->mapTableLength = cpu_to_le32(mtl);
	disc->udf_lvd[0]->numPartitionMaps = cpu_to_le32(npm + 1);
	memcpy(pm, &default_virtmap, sizeof(struct virtualPartitionMap));
	pm->partitionNum = cpu_to_le16(partitionNum);
	memcpy(pm->partIdent.identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));

	disc->udf_lvid->numOfPartitions = cpu_to_le32(npm + 1);
	disc->udf_lvid = realloc(disc->udf_lvid,
		sizeof(struct logicalVolIntegrityDesc) +
		sizeof(uint32_t) * 2 * (npm + 1) +
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->data[sizeof(uint32_t) * 2 * (npm + 1)],
		&disc->udf_lvid->data[sizeof(uint32_t) * 2 * npm],
		sizeof(struct logicalVolIntegrityDescImpUse));
	memmove(&disc->udf_lvid->data[sizeof(uint32_t) * (npm + 1)],
		&disc->udf_lvid->data[sizeof(uint32_t) * npm],
		sizeof(uint32_t));
}


char *udf_space_type_str[UDF_SPACE_TYPE_SIZE] = { "RESERVED", "VRS", "ANCHOR", "MVDS", "RVDS", "LVID", "STABLE", "SSPACE", "PSPACE", "USPACE", "BAD", "MBR" };
