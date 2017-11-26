/*
 * Copyright (C) 2017  Pali Rohár <pali.rohar@gmail.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include "libudffs.h"
#include "readdisc.h"

static int read_offset(int fd, struct udf_disc *disc, void *buf, size_t offset, size_t count, int warn_beyond)
{
	off_t off;
	ssize_t ret;

	if (offset > (size_t)disc->blocks * disc->blocksize + count)
	{
		if (warn_beyond)
			fprintf(stderr, "%s: Warning: Trying to read beyond end of disk\n", appname);
		return -1;
	}

	off = lseek(fd, offset, SEEK_SET);
	if (off != (off_t)-1 && (size_t)off != offset)
	{
		errno = EIO;
		off = (off_t)-1;
	}
	if (off == (off_t)-1)
	{
		fprintf(stderr, "%s: Warning: lseek failed: %s\n", appname, strerror(errno));
		return -1;
	}

	ret = read(fd, buf, count);
	if (ret >= 0 && (size_t)ret != count)
	{
		errno = EIO;
		ret = -1;
	}
	if (ret < 0)
	{
		fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno));
		return -1;
	}

	return 0;
}

static int read_vrs(int fd, struct udf_disc *disc, int *bea, int *nsr, int *tea)
{
	struct volStructDesc vsd;
	uint32_t vsd_len;
	int i;

	vsd_len = disc->blocksize > 2048 ? disc->blocksize : 2048;
	*nsr = -1;
	*bea = -1;
	*tea = -1;

	for (i = 0; i < 64; ++i)
	{
		if (32768 + i*vsd_len >= 256*disc->blocksize)
			break;
		if (read_offset(fd, disc, &vsd, 32768 + i*vsd_len, sizeof(vsd), 0) < 0)
			break;
		if (!vsd.stdIdent[0])
			break;
		else if (memcmp(vsd.stdIdent, VSD_STD_ID_BEA01, VSD_STD_ID_LEN) == 0)
		{
			if (*bea == -1)
			{
				*bea = i;
				disc->udf_vrs[0] = malloc(sizeof(vsd));
				if (!disc->udf_vrs[0])
				{
					fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
					return -1;
				}
				memcpy(disc->udf_vrs[0], &vsd, sizeof(vsd));
			}
		}
		else if (memcmp(vsd.stdIdent, VSD_STD_ID_NSR02, VSD_STD_ID_LEN) == 0 ||
		         memcmp(vsd.stdIdent, VSD_STD_ID_NSR03, VSD_STD_ID_LEN) == 0)
		{
			if (*nsr == -1)
			{
				*nsr = i;
				disc->udf_vrs[1] = malloc(sizeof(vsd));
				if (!disc->udf_vrs[1])
				{
					fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
					return -1;
				}
				memcpy(disc->udf_vrs[1], &vsd, sizeof(vsd));
			}
		}
		else if (memcmp(vsd.stdIdent, VSD_STD_ID_TEA01, VSD_STD_ID_LEN) == 0)
		{
			if (*tea == -1)
			{
				*tea = i;
				disc->udf_vrs[2] = malloc(sizeof(vsd));
				if (!disc->udf_vrs[2])
				{
					fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
					return -1;
				}
				memcpy(disc->udf_vrs[2], &vsd, sizeof(vsd));
			}
		}
		else if (memcmp(vsd.stdIdent, VSD_STD_ID_BOOT2, VSD_STD_ID_LEN) != 0 &&
		         memcmp(vsd.stdIdent, VSD_STD_ID_CD001, VSD_STD_ID_LEN) != 0 &&
		         memcmp(vsd.stdIdent, VSD_STD_ID_CDW02, VSD_STD_ID_LEN) != 0)
			break;
	}

	if (i == 64)
		fprintf(stderr, "%s: Warning: Too many Volume Sequence Descriptors in Volume Recognition Sequence, stopping scanning\n", appname);

	if (*nsr == -1)
		return -2;

	return 0;
}

static void setup_blocks(struct udf_disc *disc)
{
	uint64_t blocks = disc->blksize / disc->blocksize;

	if (blocks > UINT32_MAX)
		disc->blocks = UINT32_MAX;
	else
		disc->blocks = (uint32_t)blocks;

	disc->head->blocks = disc->blocks;
}

static void setup_vrs(struct udf_disc *disc, int bea, int nsr, int tea)
{
	int max;
	struct udf_extent *ext;

	max = tea;
	if (max < nsr)
		max = nsr;
	if (max < bea)
		max = bea;

	if (disc->blocksize >= 2048)
		ext = set_extent(disc, VRS, 32768 / disc->blocksize, max+1);
	else
		ext = set_extent(disc, VRS, 32768 / disc->blocksize, ((2048 * (max+1)) + disc->blocksize - 1) / disc->blocksize);

	if (disc->blocksize >= 2048)
	{
		set_desc(ext, 0x00, nsr, sizeof(struct volStructDesc), alloc_data(disc->udf_vrs[1], sizeof(struct volStructDesc)));
		if (bea != -1)
			set_desc(ext, 0x00, bea, sizeof(struct volStructDesc), alloc_data(disc->udf_vrs[0], sizeof(struct volStructDesc)));
		if (tea != -1)
			set_desc(ext, 0x00, tea, sizeof(struct volStructDesc), alloc_data(disc->udf_vrs[2], sizeof(struct volStructDesc)));
	}
	else
	{
		set_desc(ext, 0x00, nsr * 2048 / disc->blocksize, sizeof(struct volStructDesc), alloc_data(disc->udf_vrs[1], sizeof(struct volStructDesc)));
		if (bea != -1)
			set_desc(ext, 0x00, bea * 2048 / disc->blocksize, sizeof(struct volStructDesc), alloc_data(disc->udf_vrs[0], sizeof(struct volStructDesc)));
		if (tea != -1)
			set_desc(ext, 0x00, tea * 2048 / disc->blocksize, sizeof(struct volStructDesc), alloc_data(disc->udf_vrs[2], sizeof(struct volStructDesc)));
	}
}

static int read_anchor_i(int fd, struct udf_disc *disc, int i, uint32_t location)
{
	struct anchorVolDescPtr avdp;
	struct udf_extent *ext;

	if (read_offset(fd, disc, &avdp, (size_t)location * disc->blocksize, sizeof(avdp), 1) < 0)
		return -2;

	if (le32_to_cpu(avdp.descTag.tagLocation) != location)
		return -2;
	if (le16_to_cpu(avdp.descTag.tagIdent) != TAG_IDENT_AVDP)
		return -2;

	disc->udf_anchor[i] = malloc(sizeof(avdp));
	if (!disc->udf_anchor[i])
	{
		fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
		return -1;
	}

	memcpy(disc->udf_anchor[i], &avdp, sizeof(avdp));

	ext = set_extent(disc, ANCHOR, location, 1);
	set_desc(ext, TAG_IDENT_AVDP, 0, sizeof(avdp), alloc_data(disc->udf_anchor[i], sizeof(avdp)));

	return 0;
}

static int read_anchor_first(int fd, struct udf_disc *disc)
{
	return read_anchor_i(fd, disc, 0, 256);
}

static int read_anchor_second(int fd, struct udf_disc *disc)
{
	int ret1, ret2;

	if (disc->blocks > 257 && (size_t)(disc->blocks - 257) * disc->blocksize > (size_t)32768 + disc->blocksize && disc->blocks - 257 != 256)
		ret1 = read_anchor_i(fd, disc, 1, disc->blocks - 257);
	else
		ret1 = -2;
	if (ret1 == -1)
		return -1;

	if ((size_t)(disc->blocks - 1) * disc->blocksize > (size_t)32768 + disc->blocksize && disc->blocks - 1 != 256)
		ret2 = read_anchor_i(fd, disc, 2, disc->blocks - 1);
	else
		ret2 = -2;
	if (ret2 == -1)
		return -1;

	if (disc->udf_anchor[0])
	{
		if (ret1 < 0 && ret2 < 0)
			fprintf(stderr, "%s: Warning: Second and third Anchor Volume Descriptor Pointer not found\n", appname);
		return 0;
	}

	if (ret1 < 0 && ret2 < 0)
		return -2;

	if (ret2 < 0)
		fprintf(stderr, "%s: Warning: First and third Anchor Volume Descriptor Pointer not found, using second\n", appname);
	else if (ret1 < 0)
		fprintf(stderr, "%s: Warning: First and second Anchor Volume Descriptor Pointer not found, using third\n", appname);
	else
		fprintf(stderr, "%s: Warning: First Anchor Volume Descriptor Pointer not found, using second\n", appname);

	return 0;
}

static int read_anchor_512(int fd, struct udf_disc *disc)
{
	int ret;

	ret = read_anchor_i(fd, disc, 0, 512);
	if (ret == 0)
		fprintf(stderr, "%s: Warning: First, second and third Anchor Volume Descriptor Pointer not found, but found on sector 512, using it\n", appname);

	return ret;
}

static int detect_vrs_and_anchor(int fd, struct udf_disc *disc, int id, int *found_vrs, int *vsd_2048_valid, int *bea, int *nsr, int *tea)
{
	int ret;

	if (disc->blocksize <= 2048 && *vsd_2048_valid == 0)
		return -2;

	setup_blocks(disc);

	if (disc->blocksize > 2048 || *vsd_2048_valid == -1)
	{
		free(disc->udf_vrs[0]);
		free(disc->udf_vrs[1]);
		free(disc->udf_vrs[2]);
		disc->udf_vrs[0] = NULL;
		disc->udf_vrs[1] = NULL;
		disc->udf_vrs[2] = NULL;

		ret = read_vrs(fd, disc, bea, nsr, tea);
		if (ret == -2)
		{
			if (disc->blocksize <= 2048)
				*vsd_2048_valid = 0;
			return -3;
		}
		else if (ret < 0)
			return ret;

		if (disc->blocksize <= 2048)
			*vsd_2048_valid = 1;

		*found_vrs = 1;
	}

	if (id == 0)
		return read_anchor_first(fd, disc);
	else if (id == 1)
		return read_anchor_second(fd, disc);
	else if (id == 2)
		return read_anchor_512(fd, disc);
	{
		fprintf(stderr, "%s: Error: Wrong Anchor type\n", appname);
		exit(1);
	}
}

static int detect_udf(int fd, struct udf_disc *disc)
{
	int ret, ret2;
	int bea, nsr, tea;
	int bea_2048 = -1, nsr_2048 = -1, tea_2048 = -1;
	int vsd_2048_valid = -1;
	int found_vrs = 0;

	if (disc->blocksize)
	{
		if (disc->blksize / disc->blocksize > UINT32_MAX)
			fprintf(stderr, "%s: Warning: Disk is too big, using only %lu blocks\n", appname, (unsigned long int)UINT32_MAX);

		setup_blocks(disc);

		ret = read_vrs(fd, disc, &bea, &nsr, &tea);
		if (ret < 0)
		{
			if (ret == -2)
				fprintf(stderr, "%s: Error: UDF Volume Recognition Sequence not found\n", appname);
			return -1;
		}

		ret = read_anchor_first(fd, disc);
		ret2 = read_anchor_second(fd, disc);
		if (ret < 0 && ret2 < 0)
		{
			if (ret == -2 || ret2 == -2)
			{
				ret = read_anchor_512(fd, disc);
				if (ret < 0)
				{
					fprintf(stderr, "%s: Error: UDF Volume Recognition Sequence found but not Anchor Volume Descriptor Pointer, maybe wrong blocksize?\n", appname);
					return -1;
				}
			}
			else
				return -1;
		}

		setup_vrs(disc, bea, nsr, tea);
		return 0;
	}
	else if (disc->blkssz)
	{
		disc->blocksize = disc->blkssz;
		setup_blocks(disc);
		ret = read_vrs(fd, disc, &bea, &nsr, &tea);
		if (ret != -2)
		{
			if (ret != 0)
				return -1;
			ret = read_anchor_first(fd, disc);
			ret2 = read_anchor_second(fd, disc);
			if (ret == 0 || ret2 == 0)
			{
				setup_vrs(disc, bea, nsr, tea);
				return 0;
			}
			else if (ret == -2 || ret2 == -2)
			{
				ret = read_anchor_512(fd, disc);
				if (ret < 0 && ret != -2)
					return -1;
				else if (ret == 0)
					return 0;
			}
			else
				return -1;
		}
	}

	for (disc->blocksize = 512; disc->blocksize <= 4096; disc->blocksize *= 2)
	{
		if (disc->blocksize <= 2048 && vsd_2048_valid)
		{
			bea = bea_2048;
			nsr = nsr_2048;
			tea = tea_2048;
		}

		ret = detect_vrs_and_anchor(fd, disc, 0, &found_vrs, &vsd_2048_valid, &bea, &nsr, &tea);

		if (disc->blocksize <= 2048 && vsd_2048_valid)
		{
			bea_2048 = bea;
			nsr_2048 = nsr;
			tea_2048 = tea;
		}

		if (ret == -3 || ret == -2)
			continue;
		else if (ret < 0)
			return ret;

		ret = read_anchor_second(fd, disc);
		if (ret == -1)
			return -1;

		break;
	}

	if (disc->blocksize > 4096)
	{
		for (disc->blocksize = 512; disc->blocksize <= 4096; disc->blocksize *= 2)
		{
			ret = detect_vrs_and_anchor(fd, disc, 1, &found_vrs, &vsd_2048_valid, &bea, &nsr, &tea);
			if (ret == -3 || ret == -2)
				continue;
			else if (ret < 0)
				return ret;

			break;
		}

		if (disc->blocksize > 4096)
		{
			for (disc->blocksize = 512; disc->blocksize <= 4096; disc->blocksize *= 2)
			{
				ret = detect_vrs_and_anchor(fd, disc, 2, &found_vrs, &vsd_2048_valid, &bea, &nsr, &tea);
				if (ret == -3 || ret == -2)
					continue;
				else if (ret < 0)
					return ret;

				break;
			}

			if (disc->blocksize > 4096)
			{
				if (found_vrs)
					fprintf(stderr, "%s: Error: UDF Volume Recognition Sequence found but not Anchor Volume Descriptor Pointer, maybe wrong blocksize?\n", appname);
				else
					fprintf(stderr, "%s: Error: UDF Volume Recognition Sequence not found\n", appname);
				return -1;
			}
		}
	}

	if (disc->blksize / disc->blocksize > UINT32_MAX)
		fprintf(stderr, "%s: Warning: Disk is too big, using only %lu blocks\n", appname, (unsigned long int)UINT32_MAX);

	if (disc->blkssz && disc->blkssz != disc->blocksize)
		fprintf(stderr, "%s: Warning: Disk logical sector size (%d) does not match UDF block size (%u)\n", appname, disc->blkssz, (unsigned int)disc->blocksize);

	if (disc->blocksize <= 2048)
	{
		bea = bea_2048;
		nsr = nsr_2048;
		tea = tea_2048;
	}

	setup_vrs(disc, bea, nsr, tea);

	return 0;
}

static void read_mbr(int fd, struct udf_disc *disc)
{
	struct mbr mbr;

	if (read_offset(fd, disc, &mbr, 0, sizeof(mbr), 1) < 0)
		return;

	if (le16_to_cpu(mbr.boot_signature) != MBR_BOOT_SIGNATURE)
		return;

	set_extent(disc, MBR, 0, 1);
}

static int choose_anchor(struct udf_disc *disc)
{
	if (disc->udf_anchor[0])
		return 0;
	else if (disc->udf_anchor[1])
		return 1;
	else if (disc->udf_anchor[2])
		return 2;
	else
		return -1;
}

static int scan_vds(int fd, struct udf_disc *disc, enum udf_space_type vds_type)
{
	uint32_t location, length, count, i;
	uint32_t next_vdp_num, next_location, next_length, next_count;
	uint16_t type, udf_rev_le16;
	uint32_t gd_length;
	struct genericDesc gd;
	struct genericDesc *gd_ptr;
	struct udf_extent *ext;
	struct volDescPtr *vdp;
	struct logicalVolDesc *lvd;
	struct unallocSpaceDesc *usd;
	int id, anchor;
	int nested;

	anchor = choose_anchor(disc);
	if (anchor == -1)
		return -2;

	if (vds_type == MVDS)
	{
		next_location = le32_to_cpu(disc->udf_anchor[anchor]->mainVolDescSeqExt.extLocation);
		next_length = le32_to_cpu(disc->udf_anchor[anchor]->mainVolDescSeqExt.extLength) & EXT_LENGTH_MASK;
		id = 0;
	}
	else if (vds_type == RVDS)
	{
		next_location = le32_to_cpu(disc->udf_anchor[anchor]->reserveVolDescSeqExt.extLocation);
		next_length = le32_to_cpu(disc->udf_anchor[anchor]->reserveVolDescSeqExt.extLength) & EXT_LENGTH_MASK;
		id = 1;
	}
	else
	{
		fprintf(stderr, "%s: Error: Wrong Volume Descriptor Sequence type\n", appname);
		exit(1);
	}

	next_count = next_length / disc->blocksize;

	if (!next_location || !next_count)
		return -2;

	nested = 0;
	next_vdp_num = 0;

	while (next_location && next_count)
	{
		if (++nested > 64)
		{
			fprintf(stderr, "%s: Warning: Too many nested Volume Descriptor Sequences, stopping scanning\n", appname);
			break;
		}

		location = next_location;
		length = next_length;
		count = next_count;
		next_location = 0;
		next_length = 0;
		next_count = 0;

		ext = set_extent(disc, vds_type, location, count);

		if (count > 64)
			length = 64 * disc->blocksize;

		for (i = 0; i < count; ++i)
		{
			if (count >= 64)
			{
				fprintf(stderr, "%s: Warning: Too many descriptors in Volume Descriptor Sequence, stopping scanning\n", appname);
				break;
			}

			if (read_offset(fd, disc, &gd, ((size_t)location+i) * disc->blocksize, sizeof(gd), 1) < 0)
				return -3;

			type = le16_to_cpu(gd.descTag.tagIdent);
			if (type == 0)
				break;

			if (le32_to_cpu(gd.descTag.tagLocation) != location+i)
			{
				fprintf(stderr, "%s: Warning: Incorrect Volume Descriptor\n", appname);
				return -3;
			}

			switch (type)
			{
				case TAG_IDENT_PVD:
				case TAG_IDENT_PD:
				case TAG_IDENT_IUVD:
				case TAG_IDENT_TD:
				case TAG_IDENT_VDP:
				default:
					gd_ptr = malloc(sizeof(gd));
					if (!gd_ptr)
					{
						fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
						return -1;
					}
					memcpy(gd_ptr, &gd, sizeof(gd));
					set_desc(ext, type, i, sizeof(gd), alloc_data(gd_ptr, sizeof(gd)));

					if (type == TAG_IDENT_PVD && (!disc->udf_pvd[id] || le32_to_cpu(disc->udf_pvd[id]->volDescSeqNum) < le32_to_cpu(gd.volDescSeqNum)))
						disc->udf_pvd[id] = (struct primaryVolDesc *)gd_ptr;
					else if (type == TAG_IDENT_PD && (!disc->udf_pd[id] || le32_to_cpu(disc->udf_pd[id]->volDescSeqNum) < le32_to_cpu(gd.volDescSeqNum)))
						disc->udf_pd[id] = (struct partitionDesc *)gd_ptr;
					else if (type == TAG_IDENT_IUVD && (!disc->udf_iuvd[id] || le32_to_cpu(disc->udf_iuvd[id]->volDescSeqNum) < le32_to_cpu(gd.volDescSeqNum)))
						disc->udf_iuvd[id] = (struct impUseVolDesc *)gd_ptr;
					else if (type == TAG_IDENT_TD && !disc->udf_td[id])
						disc->udf_td[id] = (struct terminatingDesc *)gd_ptr;
					else if (type == TAG_IDENT_VDP)
					{
						vdp = (struct volDescPtr *)&gd;
						if (next_vdp_num < le32_to_cpu(vdp->volDescSeqNum))
						{
							next_vdp_num = le32_to_cpu(vdp->volDescSeqNum);
							next_location = le32_to_cpu(vdp->nextVolDescSeqExt.extLocation);
							next_length = le32_to_cpu(vdp->nextVolDescSeqExt.extLength) & EXT_LENGTH_MASK;
							next_count = next_length / disc->blocksize;
						}
					}
					else
						fprintf(stderr, "%s: Warning: Unknown descriptor in Volume Descriptor Sequence\n", appname);
					break;

				case TAG_IDENT_LVD:
					lvd = (struct logicalVolDesc *)&gd;

					gd_length = sizeof(*lvd) + le32_to_cpu(lvd->mapTableLength);
					if (i*disc->blocksize + gd_length > length)
					{
						fprintf(stderr, "%s: Warning: Logical Volume Descriptor is too big\n", appname);
						i = count-1;
						break;
					}

					lvd = malloc(gd_length);
					if (!lvd)
					{
						fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
						return -1;
					}

					if (gd_length <= sizeof(gd))
						memcpy(lvd, &gd, gd_length);
					else
					{
						memcpy(lvd, &gd, sizeof(gd));
						errno = 0;
						if (read(fd, (void *)lvd + sizeof(gd), gd_length - sizeof(gd)) != (ssize_t)(gd_length - sizeof(gd)))
						{
							fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
							free(lvd);
							return -3;
						}
					}

					set_desc(ext, TAG_IDENT_LVD, i, gd_length, alloc_data(lvd, gd_length));

					if (gd_length > disc->blocksize)
						i += (gd_length + (disc->blocksize-1)) / disc->blocksize - 1;

					if (!disc->udf_lvd[id] || le32_to_cpu(disc->udf_lvd[id]->volDescSeqNum) < le32_to_cpu(gd.volDescSeqNum))
					{
						disc->udf_lvd[id] = lvd;
						if (strncmp((char *)disc->udf_lvd[id]->domainIdent.ident, UDF_ID_COMPLIANT, sizeof(disc->udf_lvd[id]->domainIdent.ident)) == 0)
						{
							memcpy(&udf_rev_le16, disc->udf_lvd[id]->domainIdent.identSuffix, sizeof(udf_rev_le16));
							disc->udf_rev = le16_to_cpu(udf_rev_le16);
							disc->udf_write_rev = disc->udf_rev;
						}
					}

					if (le32_to_cpu(lvd->logicalBlockSize) != disc->blocksize)
						fprintf(stderr, "%s: Warning: blocksize in Logical Volume Descriptor is different as expected\n", appname);

					break;

				case TAG_IDENT_USD:
					usd = (struct unallocSpaceDesc *)&gd;

					gd_length = sizeof(*usd) + le32_to_cpu(usd->numAllocDescs) * sizeof(*usd->allocDescs);
					if (i*disc->blocksize + gd_length > length)
					{
						fprintf(stderr, "%s: Warning: Unallocated Space Descriptor is too big\n", appname);
						i = count-1;
						break;
					}

					usd = malloc(gd_length);
					if (!usd)
					{
						fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
						return -1;
					}

					if (gd_length <= sizeof(gd))
						memcpy(usd, &gd, gd_length);
					else
					{
						memcpy(usd, &gd, sizeof(gd));
						errno = 0;
						if (read(fd, (void *)usd + sizeof(gd), gd_length - sizeof(gd)) != (ssize_t)(gd_length - sizeof(gd)))
						{
							fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
							free(usd);
							return -3;
						}
					}

					set_desc(ext, TAG_IDENT_USD, i, gd_length, alloc_data(usd, gd_length));

					if (gd_length > disc->blocksize)
						i += (gd_length + (disc->blocksize-1)) / disc->blocksize - 1;

					if (!disc->udf_usd[id] || le32_to_cpu(disc->udf_usd[id]->volDescSeqNum) < le32_to_cpu(gd.volDescSeqNum))
						disc->udf_usd[id] = usd;

					break;
			}

			if (type == TAG_IDENT_TD)
				break;
		}
	}

	return 0;
}

static void scan_mvds(int fd, struct udf_disc *disc)
{
	int ret;

	ret = scan_vds(fd, disc, MVDS);
	if (ret == -2)
		fprintf(stderr, "%s: Warning: Main Volume Descriptor Sequence not found\n", appname);
	else if (ret == -3)
		fprintf(stderr, "%s: Warning: Main Volume Descriptor Sequence is damaged\n", appname);
}

static void scan_rvds(int fd, struct udf_disc *disc)
{
	int ret;

	ret = scan_vds(fd, disc, RVDS);
	if (ret == -2)
		fprintf(stderr, "%s: Warning: Reserve Volume Descriptor Sequence not found\n", appname);
	else if (ret == -3)
		fprintf(stderr, "%s: Warning: Reserve Volume Descriptor Sequence is damaged\n", appname);
}

static void scan_lvis(int fd, struct udf_disc *disc)
{
	uint32_t location, length;
	uint16_t type;
	unsigned char buffer[512];
	struct udf_extent *ext;
	struct logicalVolIntegrityDesc *lvid;
	tag *descTag;
	int id;
	int scanned;

	if (disc->udf_lvd[0])
		id = 0;
	else if (disc->udf_lvd[1])
		id = 1;
	else
		return;

	location = le32_to_cpu(disc->udf_lvd[id]->integritySeqExt.extLocation);
	length = le32_to_cpu(disc->udf_lvd[id]->integritySeqExt.extLength) & EXT_LENGTH_MASK;

	scanned = 0;

	while (location && length)
	{
		if (length > 64*disc->blocksize)
		{
			fprintf(stderr, "%s: Warning: Logical Volume Integrity Descriptor is too big\n", appname);
			break;
		}

		if (read_offset(fd, disc, &buffer, (size_t)location * disc->blocksize, sizeof(buffer), 1) < 0)
			return;

		descTag = (tag *)&buffer;
		type = le16_to_cpu(descTag->tagIdent);
		if (type == 0)
			break;

		if (le32_to_cpu(descTag->tagLocation) != location)
		{
			fprintf(stderr, "%s: Warning: Incorrect Logical Volume Integrity Descriptor\n", appname);
			break;
		}

		if (type == TAG_IDENT_TD)
			break;

		if (type != TAG_IDENT_LVID)
		{
			fprintf(stderr, "%s: Warning: Incorrect Logical Volume Integrity Descriptor\n", appname);
			break;
		}

		lvid = (struct logicalVolIntegrityDesc *)buffer;
		if (sizeof(*lvid) + le32_to_cpu(lvid->numOfPartitions) * 2 * sizeof(uint32_t) + le32_to_cpu(lvid->lengthOfImpUse) > length)
		{
			fprintf(stderr, "%s: Warning: Incorrect Logical Volume Integrity Descriptor\n", appname);
			break;
		}

		lvid = malloc(length);
		if (!lvid)
		{
			fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
			break;
		}

		if (length <= sizeof(buffer))
			memcpy(lvid, &buffer, length);
		else
		{
			memcpy(lvid, &buffer, sizeof(buffer));
			errno = 0;
			if (read(fd, (void *)lvid + sizeof(buffer), length - sizeof(buffer)) != (ssize_t)(length - sizeof(buffer)))
			{
				fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
				free(lvid);
				break;
			}
		}

		ext = set_extent(disc, LVID, location, (length + disc->blocksize-1) / disc->blocksize);
		set_desc(ext, TAG_IDENT_LVID, location, length, alloc_data(lvid, length));

		disc->udf_lvid = lvid;

		location = le32_to_cpu(lvid->nextIntegrityExt.extLocation);
		length = le32_to_cpu(lvid->nextIntegrityExt.extLength) & EXT_LENGTH_MASK;

		if (++scanned >= 1000)
		{
			fprintf(stderr, "%s: Warning: Too many Logical Volume Integrity Descriptors, stopping scanning\n", appname);
			break;
		}
	}
}

static void parse_lvidiu(struct udf_disc *disc)
{
	struct logicalVolIntegrityDescImpUse *lvidiu;

	if (!disc->udf_lvid || disc->udf_lvid->lengthOfImpUse < sizeof(*lvidiu))
	{
		fprintf(stderr, "%s: Warning: Logical Volume Integrity Descriptor Implementation Use not found\n", appname);
		return;
	}

	lvidiu = (struct logicalVolIntegrityDescImpUse *)&(disc->udf_lvid->impUse[le32_to_cpu(disc->udf_lvid->numOfPartitions) * 2 * sizeof(uint32_t)]);
	disc->udf_rev = le16_to_cpu(lvidiu->minUDFReadRev);
	disc->udf_write_rev = le16_to_cpu(lvidiu->minUDFWriteRev);
	disc->num_files = le32_to_cpu(lvidiu->numFiles);
	disc->num_dirs = le32_to_cpu(lvidiu->numDirs);
}

static struct genericPartitionMap *find_partition(struct udf_disc *disc, int id, uint16_t partition)
{
	uint16_t i;
	uint32_t offset;
	struct genericPartitionMap *pmap;

	if (partition >= le32_to_cpu(disc->udf_lvd[id]->numPartitionMaps))
		return NULL;

	offset = 0;
	for (i = 0; i <= partition; ++i)
	{
		if (offset >= le32_to_cpu(disc->udf_lvd[id]->mapTableLength))
			return NULL;
		pmap = (struct genericPartitionMap *)&disc->udf_lvd[id]->partitionMaps[offset];
		offset += pmap->partitionMapLength;
	}

	if (offset > le32_to_cpu(disc->udf_lvd[id]->mapTableLength))
		return NULL;

	return pmap;
}

static uint32_t find_block_position(struct udf_disc *disc, struct genericPartitionMap *pmap, uint32_t block)
{
	struct udfPartitionMap2 *upm2;
	struct sparablePartitionMap *spm;
	uint8_t count, i;
	uint16_t packet_len, num, j;
	uint32_t location, packet, offset;

	if (pmap->partitionMapType == GP_PARTITION_MAP_TYPE_1)
		return block;
	else if (pmap->partitionMapType == GP_PARTITION_MAP_TYPE_2)
	{
		upm2 = (struct udfPartitionMap2 *)pmap;
		if (strncmp((char *)upm2->partIdent.ident, UDF_ID_VIRTUAL, sizeof(upm2->partIdent.ident)) == 0)
		{
			/* TODO: Add support for VAT */
			fprintf(stderr, "%s: Warning: Virtual Partition Map is not supported\n", appname);
			return UINT32_MAX;
		}
		else if (strncmp((char *)upm2->partIdent.ident, UDF_ID_SPARABLE, sizeof(upm2->partIdent.ident)) == 0)
		{
			spm = (struct sparablePartitionMap *)upm2;
			count = spm->numSparingTables;
			packet_len = le16_to_cpu(spm->packetLength);

			packet = block & ~(packet_len-1);
			offset = block & (packet_len-1);

			for (i = 0; i < count; ++i)
			{
				if (!disc->udf_stable[i])
					continue;

				num = le16_to_cpu(disc->udf_stable[i]->reallocationTableLen);
				for (j = 0; j < num; ++j)
				{
					location = le32_to_cpu(disc->udf_stable[i]->mapEntry[j].origLocation);
					if (location >= 0xFFFFFFF0)
						break;
					else if (location == packet)
						return le32_to_cpu(disc->udf_stable[i]->mapEntry[j].mappedLocation) + offset;
					else if (location > packet)
						break;
				}
			}

			return block;
		}
		else if (strncmp((char *)upm2->partIdent.ident, UDF_ID_METADATA, sizeof(upm2->partIdent.ident)) == 0)
		{
			/* TODO: Add support for Metadata */
			fprintf(stderr, "%s: Warning: Metadata Partition Map is not supported\n", appname);
			return UINT32_MAX;
		}
		else
		{
			fprintf(stderr, "%s: Warning: Unknown Type 2 Partition Map\n", appname);
			return UINT32_MAX;
		}
	}
	else
	{
		fprintf(stderr, "%s: Warning: Unknown Partition Map\n", appname);
		return UINT32_MAX;
	}
}

static void read_stable(int fd, struct udf_disc *disc)
{
	long_ad *ad;
	size_t st_len;
	uint8_t count, i;
	uint16_t partition, packet_len, num, j;
	uint32_t location, length;
	unsigned char buffer[512];
	struct sparablePartitionMap *spm;
	struct sparingTable *st;
	struct udf_extent *ext;
	int id;

	if (disc->udf_lvd[0])
		id = 0;
	else if (disc->udf_lvd[1])
		id = 1;
	else
		return;

	ad = (long_ad *)disc->udf_lvd[id]->logicalVolContentsUse;
	partition = le16_to_cpu(ad->extLocation.partitionReferenceNum);

	spm = (struct sparablePartitionMap *)find_partition(disc, id, partition);
	if (!spm)
		return;

	if (spm->partitionMapType != GP_PARTITION_MAP_TYPE_2)
		return;

	if (strncmp((char *)spm->partIdent.ident, UDF_ID_SPARABLE, sizeof(spm->partIdent.ident)) != 0)
		return;

	count = spm->numSparingTables;
	if (count > 4)
	{
		fprintf(stderr, "%s: Warning: Too many Sparing Tables\n", appname);
		return;
	}

	length = le32_to_cpu(spm->sizeSparingTable);
	packet_len = le16_to_cpu(spm->packetLength);

	for (i = 0; i < count; ++i)
	{
		location = le32_to_cpu(spm->locSparingTable[i]);

		if (read_offset(fd, disc, &buffer, (size_t)location * disc->blocksize, sizeof(buffer), 1) < 0)
			return;

		st = (struct sparingTable *)&buffer;

		if (le16_to_cpu(st->descTag.tagIdent) != 0 || le32_to_cpu(st->descTag.tagLocation) != location)
		{
			fprintf(stderr, "%s: Warning: Invalid Sparing Table\n", appname);
			return;
		}

		if (st->sparingIdent.flags != 0 || strncmp((char *)st->sparingIdent.ident, UDF_ID_SPARING, sizeof(st->sparingIdent.ident)) != 0)
			continue;

		num = le16_to_cpu(st->reallocationTableLen);
		st_len = sizeof(*st) + num * sizeof(struct sparingEntry);
		if (st_len > length)
		{
			fprintf(stderr, "%s: Warning: Sparing Table is too big\n", appname);
			return;
		}

		disc->udf_stable[i] = malloc(st_len);
		if (!disc->udf_stable[i])
		{
			fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
			return;
		}

		if (st_len <= sizeof(buffer))
			memcpy(disc->udf_stable[i], &buffer, st_len);
		else
		{
			memcpy(disc->udf_stable[i], &buffer, sizeof(buffer));
			errno = 0;
			if (read(fd, (void *)disc->udf_stable[i] + sizeof(buffer), st_len - sizeof(buffer)) != (ssize_t)(st_len - sizeof(buffer)))
			{
				fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
				free(disc->udf_stable[i]);
				disc->udf_stable[i] = NULL;
				return;
			}
		}

		set_extent(disc, STABLE, location, (length + disc->blocksize-1) / disc->blocksize);

		for (j = 0; j < num; ++j)
		{
			location = le32_to_cpu(disc->udf_stable[i]->mapEntry[j].mappedLocation);
			ext = set_extent(disc, SSPACE, location, packet_len);
			if (ext->prev && ext->prev->space_type == SSPACE)
			{
				ext->prev->blocks = packet_len + location - ext->prev->start;
				ext->prev->next = ext->next;
				if (ext->next)
					ext->next->prev = ext->prev;
				free(ext);
			}
		}
	}
}

static void setup_pspace(struct udf_disc *disc)
{
	struct udf_extent *ext, *new_ext;
	uint32_t location, blocks;
	int id;

	if (disc->udf_pd[0])
		id = 0;
	else if (disc->udf_pd[1])
		id = 1;
	else
	{
		fprintf(stderr, "%s: Warning: Partition Space not found\n", appname);
		return;
	}

	location = le32_to_cpu(disc->udf_pd[id]->partitionStartingLocation);
	blocks = le32_to_cpu(disc->udf_pd[id]->partitionLength);
	if (!location || !blocks)
	{
		fprintf(stderr, "%s: Warning: Partition Space not found\n", appname);
		return;
	}

	if (location + blocks > disc->blocks)
		fprintf(stderr, "%s: Warning: Partition Space is beyond end of disk\n", appname);

	ext = find_extent(disc, location);
	if (ext->space_type != USPACE)
	{
		fprintf(stderr, "%s: Warning: Partition Space overlaps with older blocks\n", appname);
		ext = ext->next;
		if (ext->space_type == USPACE)
		{
			if (blocks > ext->start - location && blocks - (ext->start - location) < ext->blocks)
				ext = set_extent(disc, PSPACE, ext->start, blocks - (ext->start - location));
			else
				ext = set_extent(disc, PSPACE, ext->start, ext->blocks);
			ext->start = location;
			ext->blocks = blocks;
		}
		else
		{
			new_ext = malloc(sizeof(struct udf_extent));
			new_ext->space_type = PSPACE;
			new_ext->start = location;
			new_ext->blocks = blocks;
			new_ext->head = new_ext->tail = NULL;
			new_ext->prev = ext->prev;
			new_ext->next = ext;
			new_ext->prev->next = new_ext;
			new_ext->next->prev = new_ext;
		}
	}
	else
	{
		if ((location == ext->start || (location > ext->start && location + blocks > ext->start + ext->blocks)) && blocks > ext->blocks)
		{
			fprintf(stderr, "%s: Warning: Partition Space overlaps with older blocks\n", appname);
			ext = set_extent(disc, PSPACE, location, ext->blocks);
			ext->blocks = blocks;
		}
		else
			set_extent(disc, PSPACE, location, blocks);
	}
}

static void read_fsd(int fd, struct udf_disc *disc)
{
	long_ad *ad;
	uint16_t partition;
	uint32_t block_num, location, position, length;
	struct genericPartitionMap *pmap;
	struct udf_extent *ext;
	int id;

	if (disc->udf_lvd[0] && disc->udf_pd[0])
		id = 0;
	else if (disc->udf_lvd[1] && disc->udf_pd[1])
		id = 1;
	else if (disc->udf_lvd[0] && disc->udf_pd[1])
		id = 0;
	else if (disc->udf_lvd[1] && disc->udf_pd[0])
		id = 1;
	else
		return;

	ad = (long_ad *)disc->udf_lvd[id]->logicalVolContentsUse;
	block_num = le32_to_cpu(ad->extLocation.logicalBlockNum);
	partition = le16_to_cpu(ad->extLocation.partitionReferenceNum);
	length = le32_to_cpu(ad->extLength) & EXT_LENGTH_MASK;

	pmap = find_partition(disc, id, partition);
	if (!pmap)
	{
		fprintf(stderr, "%s: Warning: Incorrect Logical Volume Descriptor\n", appname);
		return;
	}

	position = find_block_position(disc, pmap, block_num);
	if (position == UINT32_MAX)
	{
		fprintf(stderr, "%s: Warning: File Set Descriptor cannot be read\n", appname);
		return;
	}

	if (id == 0 && !disc->udf_pd[0])
		id = 1;
	else if (id == 1 && !disc->udf_pd[1])
		id = 0;

	location = le32_to_cpu(disc->udf_pd[id]->partitionStartingLocation) + position;

	if (sizeof(*disc->udf_fsd) > length)
	{
		fprintf(stderr, "%s: Warning: Incorrect File Set Descriptor\n", appname);
		return;
	}

	disc->udf_fsd = malloc(length);
	if (!disc->udf_fsd)
	{
		fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
		return;
	}

	if (read_offset(fd, disc, disc->udf_fsd, (size_t)location * disc->blocksize, length, 1) < 0)
	{
		free(disc->udf_fsd);
		disc->udf_fsd = NULL;
		return;
	}

	if (le32_to_cpu(disc->udf_fsd->descTag.tagLocation) != position)
	{
		fprintf(stderr, "%s: Warning: Incorrect Logical Volume Integrity Descriptor\n", appname);
		free(disc->udf_fsd);
		disc->udf_fsd = NULL;
		return;
	}

	if (le16_to_cpu(disc->udf_fsd->descTag.tagIdent) != TAG_IDENT_FSD)
	{
		fprintf(stderr, "%s: Warning: Incorrect File Set Descriptor\n", appname);
		free(disc->udf_fsd);
		disc->udf_fsd = NULL;
		return;
	}

	ext = next_extent(disc->head, PSPACE);
	if (ext)
		set_desc(ext, TAG_IDENT_FSD, location, length, alloc_data(disc->udf_fsd, length));
}

static void setup_total_space_blocks(struct udf_disc *disc)
{
	int id;

	if (disc->udf_pd[0])
		id = 0;
	else if (disc->udf_pd[1])
		id = 1;
	else
	{
		fprintf(stderr, "%s: Warning: Determining total space blocks is not possible\n", appname);
		return;
	}

	disc->total_space_blocks = le32_to_cpu(disc->udf_pd[id]->partitionLength);

	if (disc->total_space_blocks + le32_to_cpu(disc->udf_pd[id]->partitionStartingLocation) > disc->blocks)
		fprintf(stderr, "%s: Warning: Some space blocks are beyond end of disk\n", appname);
}

static uint32_t count_bitmap_blocks(int fd, struct udf_disc *disc, uint32_t location, uint32_t length)
{
	unsigned long int buffer[512/sizeof(unsigned long int)];
	unsigned long int val;
	struct spaceBitmapDesc sbd;
	uint32_t bits;
	uint32_t bytes;
	uint32_t blocks;
	size_t i;

	if (sizeof(sbd) > length)
	{
		fprintf(stderr, "%s: Warning: Invalid Space Bitmap Descriptor\n", appname);
		return 0;
	}

	if (read_offset(fd, disc, &sbd, (size_t)location * disc->blocksize, sizeof(sbd), 1) < 0)
		return 0;

	bits = le32_to_cpu(sbd.numOfBits);
	bytes = le32_to_cpu(sbd.numOfBytes);

	if (bytes > length - sizeof(sbd) || bytes < (bits+7) / 8)
	{
		fprintf(stderr, "%s: Warning: Invalid Space Bitmap Descriptor\n", appname);
		return 0;
	}

	bytes = (bits+7) / 8;
	blocks = 0;

	for (bytes = (bits+7) / 8; bytes > sizeof(buffer); bytes -= sizeof(buffer))
	{
		if (read(fd, &buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer))
		{
			fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
			return 0;
		}

		for (i = 0; i < sizeof(buffer)/sizeof(*buffer); ++i)
		{
			val = buffer[i];
			while (val)
			{
				val &= val - 1;
				++blocks;
			}
		}
	}

	if (bytes)
	{
		memset(&buffer, 0, sizeof(buffer));
		if (read(fd, &buffer, bytes) != (ssize_t)bytes)
		{
			fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
			return 0;
		}

		if (bits % 8)
			((unsigned char *)buffer)[bytes-1] &= (1 << bits) - 1;

		for (i = 0; i < (bytes+sizeof(*buffer)) / sizeof(*buffer); ++i)
		{
			val = buffer[i];
			while (val)
			{
				val &= val - 1;
				++blocks;
			}
		}
	}

	return blocks;
}

static uint32_t count_table_blocks(int fd, struct udf_disc *disc, uint32_t location, uint32_t length)
{
	unsigned char buffer[512];
	struct unallocSpaceEntry *use;
	size_t use_len;
	uint64_t space, blocks;
	size_t i, count;
	short_ad *sad;
	long_ad *lad;

	if (sizeof(*use) > length)
	{
		fprintf(stderr, "%s: Warning: Invalid Space Entry\n", appname);
		return 0;
	}

	if (read_offset(fd, disc, &buffer, (size_t)location * disc->blocksize, sizeof(buffer), 1) < 0)
		return 0;

	use = (struct unallocSpaceEntry *)&buffer;
	use_len = sizeof(*use) + le32_to_cpu(use->lengthAllocDescs);
	if (use_len > length)
	{
		fprintf(stderr, "%s: Warning: Invalid Space Entry\n", appname);
		return 0;
	}

	use = malloc(use_len);
	if (!use)
	{
		fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
		return 0;
	}

	if (use_len <= sizeof(buffer))
		memcpy(use, &buffer, use_len);
	else
	{
		memcpy(use, &buffer, sizeof(buffer));
		errno = 0;
		if (read(fd, (void *)use + sizeof(buffer), use_len - sizeof(buffer)) != (ssize_t)(use_len - sizeof(buffer)))
		{
			fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
			free(use);
			return 0;
		}
	}

	space = 0;

	switch (le16_to_cpu(use->icbTag.flags) & ICBTAG_FLAG_AD_MASK)
	{
		case ICBTAG_FLAG_AD_SHORT:
			sad = (short_ad *)&use->allocDescs[0];
			count = (use_len-sizeof(*use)) / sizeof(*sad);
			for (i = 0; i < count; ++i)
				space += le32_to_cpu(sad[i].extLength) & EXT_LENGTH_MASK;
			break;

		case ICBTAG_FLAG_AD_LONG:
			lad = (long_ad *)&use->allocDescs[0];
			count = (use_len-sizeof(*use)) / sizeof(*lad);
			for (i = 0; i < count; ++i)
				space += le32_to_cpu(lad[i].extLength) & EXT_LENGTH_MASK;
			break;

		default:
			fprintf(stderr, "%s: Warning: Invalid Information Control Block in Space Entry\n", appname);
			break;
	}

	free(use);

	blocks = (space + disc->blocksize-1) / disc->blocksize;
	if (blocks > UINT32_MAX)
		return UINT32_MAX;
	else
		return blocks;
}

static void scan_free_space_blocks(int fd, struct udf_disc *disc)
{
	long_ad *ad;
	uint16_t partition;
	uint32_t blocks, position, location, length;
	struct genericPartitionMap *pmap;
	struct partitionHeaderDesc *phd;
	char *ident;
	int id;

	if (!disc->udf_lvid)
		return;

	if (disc->udf_lvd[0])
		id = 0;
	else if (disc->udf_lvd[1])
		id = 1;
	else
		return;

	ad = (long_ad *)disc->udf_lvd[id]->logicalVolContentsUse;
	partition = le16_to_cpu(ad->extLocation.partitionReferenceNum);

	if (partition < le32_to_cpu(disc->udf_lvid->numOfPartitions))
	{
		blocks = le32_to_cpu(disc->udf_lvid->freeSpaceTable[partition]);
		if (blocks != 0xFFFFFFFF)
		{
			disc->free_space_blocks = blocks;
			return;
		}
	}

	pmap = find_partition(disc, id, partition);
	if (!pmap)
		return;

	if (find_block_position(disc, pmap, 0) == UINT32_MAX)
	{
		fprintf(stderr, "%s: Warning: Determining free space blocks is not possible\n", appname);
		return;
	}

	if (disc->udf_pd[0])
		id = 0;
	else if (disc->udf_pd[1])
		id = 1;
	else
		return;

	ident = (char *)disc->udf_pd[id]->partitionContents.ident;
	length = sizeof(disc->udf_pd[id]->partitionContents.ident);
	if (strncmp(ident, PD_PARTITION_CONTENTS_NSR02, length) != 0 && strncmp(ident, PD_PARTITION_CONTENTS_NSR03, length) != 0)
	{
		fprintf(stderr, "%s: Warning: Unknown Partition Descriptor Content, determining free space blocks is not possible\n", appname);
		return;
	}

	location = le32_to_cpu(disc->udf_pd[id]->partitionStartingLocation);
	phd = (struct partitionHeaderDesc *)disc->udf_pd[id]->partitionContentsUse;

	length = le32_to_cpu(phd->unallocSpaceBitmap.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		position = find_block_position(disc, pmap, le32_to_cpu(phd->unallocSpaceBitmap.extPosition));
		blocks = count_bitmap_blocks(fd, disc, location + position, length);
		if (blocks)
		{
			disc->free_space_blocks = blocks;
			return;
		}
	}

	length = le32_to_cpu(phd->freedSpaceBitmap.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		position = find_block_position(disc, pmap, le32_to_cpu(phd->freedSpaceBitmap.extPosition));
		blocks = count_bitmap_blocks(fd, disc, location + position, length);
		if (blocks)
		{
			disc->free_space_blocks = blocks;
			return;
		}
	}

	length = le32_to_cpu(phd->unallocSpaceTable.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		position = find_block_position(disc, pmap, le32_to_cpu(phd->unallocSpaceTable.extPosition));
		blocks = count_table_blocks(fd, disc, location + position, length);
		if (blocks)
		{
			disc->free_space_blocks = blocks;
			return;
		}
	}

	length = le32_to_cpu(phd->freedSpaceTable.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		position = find_block_position(disc, pmap, le32_to_cpu(phd->freedSpaceTable.extPosition));
		blocks = count_table_blocks(fd, disc, location + position, length);
		if (blocks)
		{
			disc->free_space_blocks = blocks;
			return;
		}
	}

	disc->free_space_blocks = 0;
}

int read_disc(int fd, struct udf_disc *disc)
{
	if (detect_udf(fd, disc) < 0)
		return -1;

	read_mbr(fd, disc);

	scan_mvds(fd, disc);
	scan_rvds(fd, disc);

	if (!disc->udf_pvd[0] && !disc->udf_pvd[1])
		fprintf(stderr, "%s: Warning: Primary Volume Descriptor not found\n", appname);
	if (!disc->udf_pd[0] && !disc->udf_pd[1])
		fprintf(stderr, "%s: Warning: Partition Descriptor not found\n", appname);
	if (!disc->udf_lvd[0] && !disc->udf_lvd[1])
		fprintf(stderr, "%s: Warning: Logical Volume Descriptor not found\n", appname);

	if (!disc->udf_usd[0] && !disc->udf_usd[1])
		fprintf(stderr, "%s: Warning: Unallocated Space Descriptor not found\n", appname);
	if (!disc->udf_iuvd[0] && !disc->udf_iuvd[1])
		fprintf(stderr, "%s: Warning: Implementation Use Volume Descriptor not found\n", appname);
	if (!disc->udf_td[0] && !disc->udf_td[1])
		fprintf(stderr, "%s: Warning: Terminating Descriptor not found\n", appname);

	scan_lvis(fd, disc);

	if (!disc->udf_lvid)
		fprintf(stderr, "%s: Warning: Logical Volume Integrity Descriptor not found\n", appname);

	parse_lvidiu(disc);
	read_stable(fd, disc);
	setup_pspace(disc);

	/* TODO: setup USPACE extents */

	read_fsd(fd, disc);

	if (!disc->udf_fsd)
		fprintf(stderr, "%s: Warning: File Set Descriptor not found\n", appname);

	setup_total_space_blocks(disc);
	scan_free_space_blocks(fd, disc);

	return 0;
}
