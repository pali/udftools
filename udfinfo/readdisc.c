/*
 * Copyright (C) 2017-2021  Pali Rohár <pali.rohar@gmail.com>
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
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/cdrom.h>
#include <sys/ioctl.h>

#include "libudffs.h"
#include "readdisc.h"

static int read_offset(int fd, struct udf_disc *disc, void *buf, loff_t offset, size_t count, int warn_beyond)
{
    loff_t off;
	ssize_t ret;

    if (offset + (loff_t)count > (loff_t)disc->blocks * disc->blocksize)
	{
		if (warn_beyond)
			fprintf(stderr, "%s: Warning: Trying to read beyond end of disk\n", appname);
		return -1;
	}

	off = lseek(fd, offset, SEEK_SET);
    if (off != (loff_t)-1 && off != offset)
	{
		errno = EIO;
        off = (loff_t)-1;
	}
    if (off == (loff_t)-1)
	{
		fprintf(stderr, "%s: Warning: lseek failed: %s\n", appname, strerror(errno));
		return -1;
	}

	ret = read_nointr(fd, buf, count);
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
		if (read_offset(fd, disc, &vsd, disc->start_block*disc->blocksize + 32768 + i*vsd_len, sizeof(vsd), 0) < 0)
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
		fprintf(stderr, "%s: Warning: Too many Volume Structure Descriptors in Volume Recognition Sequence, stopping scanning\n", appname);

	if (*nsr == -1)
		return -2;

	return 0;
}

static void setup_blocks(struct udf_disc *disc, uint64_t start, uint64_t last)
{
	uint64_t blocks = disc->blksize / disc->blocksize;

	if (blocks > UINT32_MAX)
		disc->blocks = UINT32_MAX;
	else
		disc->blocks = (uint32_t)blocks;

	disc->head->blocks = disc->blocks;

	if (start)
		disc->start_block = start / disc->blocksize;

	if (last == (uint64_t)-1)
		disc->last_block = disc->blocks - 1;
	else if (last)
		disc->last_block = last / disc->blocksize;
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
		ext = set_extent(disc, VRS, disc->start_block + 32768 / disc->blocksize, max+1);
	else
		ext = set_extent(disc, VRS, disc->start_block + 32768 / disc->blocksize, ((2048 * (max+1)) + disc->blocksize - 1) / disc->blocksize);

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

    if (read_offset(fd, disc, &avdp, (loff_t)location * disc->blocksize, sizeof(avdp), 1) < 0)
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
	return read_anchor_i(fd, disc, 0, disc->start_block + 256);
}

static int read_anchor_second(int fd, struct udf_disc *disc)
{
	int ret1, ret2;
	uint32_t last_block;

	if (disc->last_block < disc->blocks)
		last_block = disc->last_block;
	else
		last_block = disc->blocks - 1;

    if (last_block > 256 && (loff_t)(last_block - 256) * disc->blocksize > (loff_t)32768 + disc->blocksize && last_block - 256 != 256)
		ret1 = read_anchor_i(fd, disc, 1, last_block - 256);
	else
		ret1 = -2;
	if (ret1 == -1)
		return -1;

    if ((loff_t)last_block * disc->blocksize > (loff_t)32768 + disc->blocksize && last_block != 256)
		ret2 = read_anchor_i(fd, disc, 2, last_block);
	else
		ret2 = -2;
	if (ret2 == -1)
		return -1;

	if (disc->udf_anchor[0] && disc->udf_anchor[1] && disc->udf_anchor[0]->mainVolDescSeqExt.extLocation != disc->udf_anchor[1]->mainVolDescSeqExt.extLocation)
		fprintf(stderr, "%s: Warning: First and second Anchor Volume Descriptor Pointer contain different location of Main Volume Descriptor Sequence, maybe wrong --startblock or --lastblock?\n", appname);
	if (disc->udf_anchor[0] && disc->udf_anchor[2] && disc->udf_anchor[0]->mainVolDescSeqExt.extLocation != disc->udf_anchor[2]->mainVolDescSeqExt.extLocation)
		fprintf(stderr, "%s: Warning: First and third Anchor Volume Descriptor Pointer contain different location of Main Volume Descriptor Sequence, maybe wrong --startblock or --lastblock?\n", appname);

	if (disc->udf_anchor[0])
		return 0;

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

	ret = read_anchor_i(fd, disc, 0, disc->start_block + 512);
	if (ret == 0)
		fprintf(stderr, "%s: Warning: First, second and third Anchor Volume Descriptor Pointer not found, but found on sector 512, using it\n", appname);

	return ret;
}

static int detect_vrs_and_anchor(int fd, struct udf_disc *disc, uint64_t start, uint64_t last, int id, int *found_vrs, int *vsd_len2048_off0_valid, int *bea, int *nsr, int *tea)
{
	int ret;

	if (disc->blocksize <= 2048 && disc->start_block == 0 && *vsd_len2048_off0_valid == 0)
		return -2;

	setup_blocks(disc, start, last);

	if (disc->blocksize > 2048 || disc->start_block != 0 || *vsd_len2048_off0_valid == -1)
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
			if (disc->blocksize <= 2048 && disc->start_block == 0)
				*vsd_len2048_off0_valid = 0;
			return -3;
		}
		else if (ret < 0)
			return ret;

		if (disc->blocksize <= 2048 && disc->start_block == 0)
			*vsd_len2048_off0_valid = 1;

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
	int vsd_len2048_off0_valid = -1;
	int found_vrs = 0;
	uint64_t start = 0;
	uint64_t last = 0;
	long last_written;
	struct stat st;
	struct cdrom_multisession multisession;

	if (disc->start_block == (uint32_t)-1)
	{
		memset(&multisession, 0, sizeof(multisession));
		multisession.addr_format = CDROM_LBA;
		if (fstat(fd, &st) == 0 && S_ISBLK(st.st_mode) && ioctl(fd, CDROMMULTISESSION, &multisession) == 0 && multisession.xa_flag)
			start = (uint64_t)multisession.addr.lba * (disc->blkssz ? disc->blkssz : 2048);
		if (!start)
			disc->start_block = 0;
	}

	if (!disc->last_block)
	{
		if (disc->vat_block)
			disc->last_block = disc->vat_block;
		else if (fstat(fd, &st) == 0 && S_ISBLK(st.st_mode) && ioctl(fd, CDROM_LAST_WRITTEN, &last_written) == 0)
			last = (uint64_t)last_written * (disc->blkssz ? disc->blkssz : 2048);
		else
			last = (uint64_t)-1;
	}

	if (disc->blocksize)
	{
		if (disc->blksize / disc->blocksize > UINT32_MAX)
			fprintf(stderr, "%s: Warning: Disk is too big (%"PRIu64"), using only %"PRIu32" blocks\n", appname, disc->blksize / disc->blocksize, UINT32_MAX);

		setup_blocks(disc, start, last);

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
					fprintf(stderr, "%s: Error: UDF Volume Recognition Sequence found but not Anchor Volume Descriptor Pointer, maybe wrong --blocksize?\n", appname);
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
		setup_blocks(disc, start, last);
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

	for (disc->blocksize = 512; disc->blocksize <= 32768; disc->blocksize *= 2)
	{
		if (disc->blocksize <= 2048 && disc->start_block == 0 && vsd_len2048_off0_valid)
		{
			bea = bea_2048;
			nsr = nsr_2048;
			tea = tea_2048;
		}

		ret = detect_vrs_and_anchor(fd, disc, start, last, 0, &found_vrs, &vsd_len2048_off0_valid, &bea, &nsr, &tea);

		if (disc->blocksize <= 2048 && disc->start_block == 0 && vsd_len2048_off0_valid)
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

	if (disc->blocksize > 32768)
	{
		for (disc->blocksize = 512; disc->blocksize <= 32768; disc->blocksize *= 2)
		{
			ret = detect_vrs_and_anchor(fd, disc, start, last, 1, &found_vrs, &vsd_len2048_off0_valid, &bea, &nsr, &tea);
			if (ret == -3 || ret == -2)
				continue;
			else if (ret < 0)
				return ret;

			break;
		}

		if (disc->blocksize > 32768)
		{
			for (disc->blocksize = 512; disc->blocksize <= 32768; disc->blocksize *= 2)
			{
				ret = detect_vrs_and_anchor(fd, disc, start, last, 2, &found_vrs, &vsd_len2048_off0_valid, &bea, &nsr, &tea);
				if (ret == -3 || ret == -2)
					continue;
				else if (ret < 0)
					return ret;

				break;
			}

			if (disc->blocksize > 32768)
			{
				if (found_vrs)
					fprintf(stderr, "%s: Error: UDF Volume Recognition Sequence found but not Anchor Volume Descriptor Pointer, maybe wrong --blocksize?\n", appname);
				else
					fprintf(stderr, "%s: Error: UDF Volume Recognition Sequence not found\n", appname);
				return -1;
			}
		}
	}

	if (disc->blksize / disc->blocksize > UINT32_MAX)
		fprintf(stderr, "%s: Warning: Disk is too big (%"PRIu64"), using only %"PRIu32" blocks\n", appname, disc->blksize / disc->blocksize, UINT32_MAX);

	if (disc->blkssz && disc->blkssz != disc->blocksize)
		fprintf(stderr, "%s: Warning: Disk logical sector size (%u) does not match UDF block size (%"PRIu32")\n", appname, disc->blkssz, disc->blocksize);

	if (disc->blocksize <= 2048 && disc->start_block == 0 && vsd_len2048_off0_valid)
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
	uint32_t next_location, next_length, next_count;
	uint16_t type;
	size_t gd_length;
	struct genericDesc *gd_ptr;
	struct primaryVolDesc *pvd_ptr;
	struct partitionDesc *pd_ptr;
	struct impUseVolDesc *iuvd_ptr;
	struct udf_extent *ext;
	struct volDescPtr *vdp;
	struct logicalVolDesc *lvd;
	struct unallocSpaceDesc *usd;
	struct domainIdentSuffix *dis;
	struct UDFIdentSuffix *uis;
	unsigned char buffer[512];
	int id, anchor;
	int nested;
	int done;

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
		if (next_location == le32_to_cpu(disc->udf_anchor[anchor]->mainVolDescSeqExt.extLocation))
		{
			fprintf(stderr, "%s: Warning: Reserve Volume Descriptor Sequence is on same location as Main\n", appname);
			disc->udf_pvd[1] = disc->udf_pvd[0];
			disc->udf_lvd[1] = disc->udf_lvd[0];
			disc->udf_pd[1] = disc->udf_pd[0];
			disc->udf_pd2[1] = disc->udf_pd2[0];
			disc->udf_usd[1] = disc->udf_usd[0];
			disc->udf_iuvd[1] = disc->udf_iuvd[0];
			disc->udf_td[1] = disc->udf_td[0];
			return 0;
		}
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

		if (count > 256)
			length = 256 * disc->blocksize;

		done = 0;

		for (i = 0; i < count; ++i)
		{
			if (count >= 256)
			{
				fprintf(stderr, "%s: Warning: Too many descriptors (%"PRIu32") in Volume Descriptor Sequence, stopping scanning\n", appname, count);
				break;
			}

            if (read_offset(fd, disc, &buffer, ((loff_t)location+i) * disc->blocksize, sizeof(buffer), 1) < 0)
				return -3;

			gd_ptr = (struct genericDesc *)&buffer;
			type = le16_to_cpu(gd_ptr->descTag.tagIdent);
			if (type == 0)
				break;

			if (le32_to_cpu(gd_ptr->descTag.tagLocation) != location+i)
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
					gd_ptr = malloc(sizeof(buffer));
					if (!gd_ptr)
					{
						fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
						return -1;
					}
					memcpy(gd_ptr, &buffer, sizeof(buffer));
					set_desc(ext, type, i, sizeof(buffer), alloc_data(gd_ptr, sizeof(buffer)));

					switch (type)
					{
						case TAG_IDENT_PVD:
							pvd_ptr = (struct primaryVolDesc *)gd_ptr;
							/* Take all PVDs with the smallest primaryVolDescNum and then choose first with the highest volDescSeqNum */
							if (!disc->udf_pvd[id] || le32_to_cpu(disc->udf_pvd[id]->primaryVolDescNum) > le32_to_cpu(pvd_ptr->primaryVolDescNum) ||
							    (le32_to_cpu(disc->udf_pvd[id]->primaryVolDescNum) == le32_to_cpu(pvd_ptr->primaryVolDescNum) &&
							     le32_to_cpu(disc->udf_pvd[id]->volDescSeqNum) < le32_to_cpu(pvd_ptr->volDescSeqNum)))
								disc->udf_pvd[id] = pvd_ptr;
							break;

						case TAG_IDENT_PD:
							pd_ptr = (struct partitionDesc *)gd_ptr;
							if (!disc->udf_pd[id] || le16_to_cpu(disc->udf_pd[id]->partitionNumber) == le16_to_cpu(pd_ptr->partitionNumber))
							{
								if (!disc->udf_pd[id] || le32_to_cpu(disc->udf_pd[id]->volDescSeqNum) < le32_to_cpu(pd_ptr->volDescSeqNum))
									disc->udf_pd[id] = pd_ptr;
							}
							else if (!disc->udf_pd2[id] || le16_to_cpu(disc->udf_pd2[id]->partitionNumber) == le16_to_cpu(pd_ptr->partitionNumber))
							{
								if (!disc->udf_pd2[id] || le32_to_cpu(disc->udf_pd2[id]->volDescSeqNum) < le32_to_cpu(pd_ptr->volDescSeqNum))
									disc->udf_pd2[id] = pd_ptr;
							}
							else
							{
								fprintf(stderr, "%s: Warning: More then two Partition Descriptors are present, ignoring others\n", appname);
							}
							break;

						case TAG_IDENT_IUVD:
							iuvd_ptr = (struct impUseVolDesc *)gd_ptr;
							if (strncmp((char *)iuvd_ptr->impIdent.ident, UDF_ID_LV_INFO, sizeof(iuvd_ptr->impIdent.ident)) == 0)
							{
								if (!disc->udf_iuvd[id] || le32_to_cpu(disc->udf_iuvd[id]->volDescSeqNum) < le32_to_cpu(iuvd_ptr->volDescSeqNum))
								{
									disc->udf_iuvd[id] = iuvd_ptr;
									uis = (struct UDFIdentSuffix *)disc->udf_iuvd[id]->impIdent.identSuffix;
									if (disc->udf_rev < le16_to_cpu(uis->UDFRevision))
										disc->udf_rev = disc->udf_write_rev = le16_to_cpu(uis->UDFRevision);
								}
							}
							break;

						case TAG_IDENT_TD:
							if (!disc->udf_td[id])
								disc->udf_td[id] = (struct terminatingDesc *)gd_ptr;
							done = 1;
							break;

						case TAG_IDENT_VDP:
							vdp = (struct volDescPtr *)gd_ptr;
							next_location = le32_to_cpu(vdp->nextVolDescSeqExt.extLocation);
							if (next_location <= location)
							{
								fprintf(stderr, "%s: Warning: Next descriptor in Volume Descriptor Sequence is not on higher block number, ignoring it\n", appname);
								next_location = 0;
							}
							else
							{
								next_length = le32_to_cpu(vdp->nextVolDescSeqExt.extLength) & EXT_LENGTH_MASK;
								next_count = next_length / disc->blocksize;
							}
							done = 1;
							break;

						default:
							fprintf(stderr, "%s: Warning: Unknown descriptor in Volume Descriptor Sequence\n", appname);
							break;
					}
					break;

				case TAG_IDENT_LVD:
					lvd = (struct logicalVolDesc *)gd_ptr;

					if (sizeof(*lvd) + (uint64_t)le32_to_cpu(lvd->mapTableLength) > 256*disc->blocksize)
					{
						fprintf(stderr, "%s: Warning: Logical Volume Descriptor is too big (%"PRIu64")\n", appname, (uint64_t)(sizeof(*lvd) + (uint64_t)le32_to_cpu(lvd->mapTableLength)));
						i = count-1;
						break;
					}

					gd_length = sizeof(*lvd) + le32_to_cpu(lvd->mapTableLength);
					if (i*disc->blocksize + gd_length > length)
					{
						fprintf(stderr, "%s: Warning: Logical Volume Descriptor is beyond end of Volume Descriptor Sequence\n", appname);
						i = count-1;
						break;
					}

					lvd = malloc(gd_length);
					if (!lvd)
					{
						fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
						return -1;
					}

					if (gd_length <= sizeof(buffer))
						memcpy(lvd, &buffer, gd_length);
					else
					{
						memcpy(lvd, &buffer, sizeof(buffer));
						if (read_nointr(fd, (uint8_t *)lvd + sizeof(buffer), gd_length - sizeof(buffer)) != (ssize_t)(gd_length - sizeof(buffer)))
						{
							fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
							free(lvd);
							return -3;
						}
					}

					set_desc(ext, TAG_IDENT_LVD, i, gd_length, alloc_data(lvd, gd_length));

					if (gd_length > disc->blocksize)
						i += (gd_length + (disc->blocksize-1)) / disc->blocksize - 1;

					if (strncmp((char *)lvd->domainIdent.ident, UDF_ID_COMPLIANT, sizeof(lvd->domainIdent.ident)) == 0)
					{
						if (!disc->udf_lvd[id] || le32_to_cpu(disc->udf_lvd[id]->volDescSeqNum) < le32_to_cpu(lvd->volDescSeqNum))
						{
							disc->udf_lvd[id] = lvd;
							dis = (struct domainIdentSuffix *)lvd->domainIdent.identSuffix;
							if (disc->udf_rev < le16_to_cpu(dis->UDFRevision))
								disc->udf_rev = disc->udf_write_rev = le16_to_cpu(dis->UDFRevision);
						}

						if (le32_to_cpu(lvd->logicalBlockSize) != disc->blocksize)
							fprintf(stderr, "%s: Warning: blocksize in Logical Volume Descriptor is different as expected\n", appname);
					}

					break;

				case TAG_IDENT_USD:
					usd = (struct unallocSpaceDesc *)&buffer;

					if (sizeof(*usd) + (uint64_t)le32_to_cpu(usd->numAllocDescs) * sizeof(*usd->allocDescs) > 256*disc->blocksize)
					{
						fprintf(stderr, "%s: Warning: Unallocated Space Descriptor is too big (%"PRIu64")\n", appname, (uint64_t)(sizeof(*usd) + (uint64_t)le32_to_cpu(usd->numAllocDescs) * sizeof(*usd->allocDescs)));
						i = count-1;
						break;
					}

					gd_length = sizeof(*usd) + le32_to_cpu(usd->numAllocDescs) * sizeof(*usd->allocDescs);
					if (i*disc->blocksize + gd_length > length)
					{
						fprintf(stderr, "%s: Warning: Unallocated Space Descriptor is beyond end of Volume Descriptor Sequence\n", appname);
						i = count-1;
						break;
					}

					usd = malloc(gd_length);
					if (!usd)
					{
						fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
						return -1;
					}

					if (gd_length <= sizeof(buffer))
						memcpy(usd, &buffer, gd_length);
					else
					{
						memcpy(usd, &buffer, sizeof(buffer));
						if (read_nointr(fd, (uint8_t *)usd + sizeof(buffer), gd_length - sizeof(buffer)) != (ssize_t)(gd_length - sizeof(buffer)))
						{
							fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
							free(usd);
							return -3;
						}
					}

					set_desc(ext, TAG_IDENT_USD, i, gd_length, alloc_data(usd, gd_length));

					if (gd_length > disc->blocksize)
						i += (gd_length + (disc->blocksize-1)) / disc->blocksize - 1;

					if (!disc->udf_usd[id] || le32_to_cpu(disc->udf_usd[id]->volDescSeqNum) < le32_to_cpu(usd->volDescSeqNum))
						disc->udf_usd[id] = usd;

					break;
			}

			if (done)
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
	uint32_t next_location, next_length;
	uint16_t type;
	size_t lvid_length;
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
		if (length > 256*disc->blocksize)
		{
			fprintf(stderr, "%s: Warning: Logical Volume Integrity Descriptor Sequence is too big (%"PRIu32")\n", appname, length);
			break;
		}

        if (read_offset(fd, disc, &buffer, (loff_t)location * disc->blocksize, sizeof(buffer), 1) < 0)
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
		if (le32_to_cpu(lvid->numOfPartitions) > 32)
		{
			fprintf(stderr, "%s: Warning: Too many partitions (%"PRIu32") in Logical Volume Integrity Descriptor, stopping scanning\n", appname, le32_to_cpu(lvid->numOfPartitions));
			break;
		}
		if (le32_to_cpu(lvid->lengthOfImpUse) > 32*disc->blocksize)
		{
			fprintf(stderr, "%s: Warning: Logical Volume Integrity Descriptor Implementation Use is too big (%"PRIu32"), stopping scanning\n", appname, le32_to_cpu(lvid->lengthOfImpUse));
			break;
		}

		lvid_length = sizeof(*lvid) + le32_to_cpu(lvid->numOfPartitions) * 2 * sizeof(uint32_t) + le32_to_cpu(lvid->lengthOfImpUse);
		if (lvid_length > length)
		{
			fprintf(stderr, "%s: Warning: Incorrect Logical Volume Integrity Descriptor\n", appname);
			break;
		}

		lvid = malloc(lvid_length);
		if (!lvid)
		{
			fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
			break;
		}

		if (lvid_length <= sizeof(buffer))
			memcpy(lvid, &buffer, lvid_length);
		else
		{
			memcpy(lvid, &buffer, sizeof(buffer));
			if (read_nointr(fd, (uint8_t *)lvid + sizeof(buffer), lvid_length - sizeof(buffer)) != (ssize_t)(lvid_length - sizeof(buffer)))
			{
				fprintf(stderr, "%s: Warning: read failed: %s\n", appname, strerror(errno ? errno : EIO));
				free(lvid);
				break;
			}
		}

		ext = set_extent(disc, LVID, location, (lvid_length + disc->blocksize-1) / disc->blocksize);
		set_desc(ext, TAG_IDENT_LVID, 0, lvid_length, alloc_data(lvid, lvid_length));

		disc->udf_lvid = lvid;

		next_location = le32_to_cpu(lvid->nextIntegrityExt.extLocation);
		next_length = le32_to_cpu(lvid->nextIntegrityExt.extLength) & EXT_LENGTH_MASK;

		if (next_length && next_location <= location)
		{
			fprintf(stderr, "%s: Warning: Next Logical Volume Integrity is not on higher block number, ignoring it\n", appname);
			next_length = 0;
		}

		if (next_location && next_length)
		{
			location = next_location;
			length = next_length;
		}
		else if (length > disc->blocksize)
		{
			++location;
			length -= disc->blocksize;
		}
		else
		{
			length = 0;
		}

		if (length > 0 && ++scanned >= 1000)
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

	lvidiu = (struct logicalVolIntegrityDescImpUse *)&(disc->udf_lvid->data[le32_to_cpu(disc->udf_lvid->numOfPartitions) * 2 * sizeof(uint32_t)]);
	disc->num_files = le32_to_cpu(lvidiu->numFiles);
	disc->num_dirs = le32_to_cpu(lvidiu->numDirs);

	/* Fields minUDFReadRev and minUDFWriteRev are defined since UDF 1.02 */
	if (disc->udf_rev >= 0x0102)
	{
		disc->udf_rev = le16_to_cpu(lvidiu->minUDFReadRev);
		if (disc->udf_write_rev < le16_to_cpu(lvidiu->minUDFWriteRev))
			disc->udf_write_rev = le16_to_cpu(lvidiu->minUDFWriteRev);
		if (disc->udf_write_rev < disc->udf_rev)
			disc->udf_write_rev = disc->udf_rev;
	}
}

static struct genericPartitionMap *find_partition(struct udf_disc *disc, uint8_t type, const char *ident, uint16_t partition, uint16_t *partition_map_num)
{
	uint32_t i, offset;
	struct genericPartitionMap *pmap;
	struct genericPartitionMap1 *pm1;
	struct udfPartitionMap2 *upm2;
	int id;

	if (disc->udf_lvd[0])
		id = 0;
	else if (disc->udf_lvd[1])
		id = 1;
	else
		return NULL;

	for (i = 0, offset = 0; i < le32_to_cpu(disc->udf_lvd[id]->numPartitionMaps) && offset < le32_to_cpu(disc->udf_lvd[id]->mapTableLength); ++i, offset += pmap->partitionMapLength)
	{
		pmap = (struct genericPartitionMap *)&disc->udf_lvd[id]->partitionMaps[offset];

		if (type != (uint8_t)-1 && pmap->partitionMapType != type)
			continue;

		if (partition_map_num && *partition_map_num != (uint16_t)-1 && i != *partition_map_num)
			continue;

		if (partition != (uint16_t)-1)
		{
			if (pmap->partitionMapType == GP_PARTITION_MAP_TYPE_1)
			{
				pm1 = (struct genericPartitionMap1 *)pmap;
				if (le16_to_cpu(pm1->partitionNum) != partition)
					continue;
			}
			else if (pmap->partitionMapType == GP_PARTITION_MAP_TYPE_2)
			{
				upm2 = (struct udfPartitionMap2 *)pmap;
				if (le16_to_cpu(upm2->partitionNum) != partition)
					continue;
			}
			else
				continue;
		}

		if (ident)
		{
			if (pmap->partitionMapType != GP_PARTITION_MAP_TYPE_2)
				continue;
			upm2 = (struct udfPartitionMap2 *)pmap;
			if (strncmp((char *)upm2->partIdent.ident, ident, sizeof(upm2->partIdent.ident)) != 0)
				continue;
		}

		if (partition_map_num && *partition_map_num == (uint16_t)-1)
			*partition_map_num = i;
		return pmap;
	}

	return NULL;
}

static struct partitionDesc *find_partition_descriptor(struct udf_disc *disc, uint16_t partition)
{
	int id;

	if (disc->udf_pd[0])
		id = 0;
	else if (disc->udf_pd[1])
		id = 1;
	else
		return NULL;

	if (le16_to_cpu(disc->udf_pd[id]->partitionNumber) == partition)
		return disc->udf_pd[id];

	if (disc->udf_pd2[0])
		id = 0;
	else if (disc->udf_pd2[1])
		id = 1;
	else
		return NULL;

	if (le16_to_cpu(disc->udf_pd2[id]->partitionNumber) == partition)
		return disc->udf_pd2[id];

	return NULL;
}

static uint32_t find_block_position(struct udf_disc *disc, struct genericPartitionMap *pmap, uint32_t block, uint16_t *partition)
{
	struct genericPartitionMap1 *pm1;
	struct udfPartitionMap2 *upm2;
	struct sparablePartitionMap *spm;
	uint32_t count, i;
	uint16_t packet_len, num, j;
	uint32_t location, length, packet, offset;

	if (pmap->partitionMapType == GP_PARTITION_MAP_TYPE_1)
	{
		pm1 = (struct genericPartitionMap1 *)pmap;
		*partition = le16_to_cpu(pm1->partitionNum);
		return block;
	}
	else if (pmap->partitionMapType == GP_PARTITION_MAP_TYPE_2)
	{
		upm2 = (struct udfPartitionMap2 *)pmap;
		*partition = le16_to_cpu(upm2->partitionNum);
		if (strncmp((char *)upm2->partIdent.ident, UDF_ID_VIRTUAL, sizeof(upm2->partIdent.ident)) == 0)
		{
			if (disc->vat && block < disc->vat_entries)
				return le32_to_cpu(disc->vat[block]);
			else
				return block;
		}
		else if (strncmp((char *)upm2->partIdent.ident, UDF_ID_SPARABLE, sizeof(upm2->partIdent.ident)) == 0)
		{
			spm = (struct sparablePartitionMap *)upm2;
			count = spm->numSparingTables;
			packet_len = le16_to_cpu(spm->packetLength);
			offset = block % packet_len;
			packet = block - offset;

			if (count > 4)
				count = 4;

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
			location = 0;
			count = disc->metadata_filemap_count[0];
			for (i = 0; i < count; ++i)
			{
				length = (le32_to_cpu(disc->metadata_filemap[0][i].extLength) & EXT_LENGTH_MASK) / disc->blocksize;
				if (block >= location && block < location + length)
					return le32_to_cpu(disc->metadata_filemap[0][i].extPosition) + (block - location);
				location += length;
			}

			location = 0;
			count = disc->metadata_filemap_count[1];
			for (i = 0; i < count; ++i)
			{
				length = (le32_to_cpu(disc->metadata_filemap[1][i].extLength) & EXT_LENGTH_MASK) / disc->blocksize;
				if (block >= location && block < location + length)
					return le32_to_cpu(disc->metadata_filemap[1][i].extPosition) + (block - location);
				location += length;
			}

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
	size_t st_len;
	uint8_t count, i;
	uint16_t packet_len, num, j;
	uint32_t location, length;
	unsigned char buffer[512];
	struct UDFIdentSuffix *uis;
	struct sparablePartitionMap *spm;
	struct sparingTable *st;
	struct udf_extent *ext;

	spm = (struct sparablePartitionMap *)find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_SPARABLE, -1, NULL);
	if (!spm)
		return;

	count = spm->numSparingTables;
	if (count > 4)
	{
		fprintf(stderr, "%s: Warning: Too many Sparing Tables\n", appname);
		count = 4;
	}

	length = le32_to_cpu(spm->sizeSparingTable);
	packet_len = le16_to_cpu(spm->packetLength);

	uis = (struct UDFIdentSuffix *)spm->partIdent.identSuffix;
	if (disc->udf_write_rev < le16_to_cpu(uis->UDFRevision))
		disc->udf_write_rev = le16_to_cpu(uis->UDFRevision);

	for (i = 0; i < count; ++i)
	{
		location = le32_to_cpu(spm->locSparingTable[i]);

        if (read_offset(fd, disc, &buffer, (loff_t)location * disc->blocksize, sizeof(buffer), 1) < 0)
			return;

		st = (struct sparingTable *)&buffer;

		if (le16_to_cpu(st->descTag.tagIdent) != 0 || le32_to_cpu(st->descTag.tagLocation) != location)
		{
			fprintf(stderr, "%s: Warning: Invalid Sparing Table\n", appname);
			return;
		}

		if (st->sparingIdent.flags != 0 || strncmp((char *)st->sparingIdent.ident, UDF_ID_SPARING, sizeof(st->sparingIdent.ident)) != 0)
			continue;

		uis = (struct UDFIdentSuffix *)st->sparingIdent.identSuffix;
		if (disc->udf_write_rev < le16_to_cpu(uis->UDFRevision))
			disc->udf_write_rev = le16_to_cpu(uis->UDFRevision);

		num = le16_to_cpu(st->reallocationTableLen);
		st_len = sizeof(*st) + num * sizeof(struct sparingEntry);
		if (st_len > length)
		{
			fprintf(stderr, "%s: Warning: Sparing Table is too big (%zu)\n", appname, st_len);
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
			if (read_nointr(fd, (uint8_t *)disc->udf_stable[i] + sizeof(buffer), st_len - sizeof(buffer)) != (ssize_t)(st_len - sizeof(buffer)))
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

static void read_vat(int fd, struct udf_disc *disc)
{
	struct partitionDesc *pd;
	struct virtualPartitionMap *vpm;
	struct UDFIdentSuffix *uis;
	long_ad *lad;
	short_ad *sad;
	uint32_t j, count;
	uint32_t ext_length, ext_position, ext_location;
	uint16_t ext_partition;
	uint64_t vat_length, vat_offset;
	uint32_t i, vat_block;
	uint32_t length, offset, location;
	uint32_t ea_length, ea_offset;
	uint32_t ea_attr_length, ea_attr_offset;
	uint64_t unique_id;
	struct fileEntry *fe;
	struct extendedFileEntry *efe;
	struct virtualAllocationTable15 *vat15;
	struct virtualAllocationTable20 *vat20;
	struct extendedAttrHeaderDesc ea_hdr;
	struct impUseExtAttr ea_attr;
	struct LVExtensionEA ea_lv;
	unsigned char *vat;
	unsigned char *descs;
	unsigned char buffer[512];

	vpm = (struct virtualPartitionMap *)find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_VIRTUAL, -1, NULL);
	if (!vpm)
	{
		if (disc->vat_block)
			fprintf(stderr, "%s: Error: Virtual Partition Map not found, but --vatblock specified\n", appname);
		return;
	}

	uis = (struct UDFIdentSuffix *)vpm->partIdent.identSuffix;
	if (disc->udf_write_rev < le16_to_cpu(uis->UDFRevision))
		disc->udf_write_rev = le16_to_cpu(uis->UDFRevision);

	pd = find_partition_descriptor(disc, le16_to_cpu(vpm->partitionNum));
	if (!pd)
	{
		fprintf(stderr, "%s: Error: Virtual Partition Map found, but corresponding Partition Descriptor not found\n", appname);
		return;
	}

	location = le32_to_cpu(pd->partitionStartingLocation);

	if (disc->vat_block)
		vat_block = disc->vat_block;
	else
		vat_block = disc->last_block;

	for (i = vat_block + 3; i > 0 && i > vat_block - 32; --i)
	{
        if (read_offset(fd, disc, &buffer, (loff_t)i * disc->blocksize, sizeof(buffer), 0) < 0)
			continue;

		fe = (struct fileEntry *)&buffer;

		if (le16_to_cpu(fe->descTag.tagIdent) != TAG_IDENT_FE && le16_to_cpu(fe->descTag.tagIdent) != TAG_IDENT_EFE)
			continue;

		if (fe->icbTag.fileType != ICBTAG_FILE_TYPE_VAT15 && fe->icbTag.fileType != ICBTAG_FILE_TYPE_VAT20)
			continue;

		if (location + le32_to_cpu(fe->descTag.tagLocation) != i)
		{
			fprintf(stderr, "%s: Warning: Found Virtual Allocation Table at partition offset %"PRIu32" (block %"PRIu32"), but expected at offset %"PRIu32", maybe wrong --startblock or --lastblock? ignoring it\n", appname, i-location, i, le32_to_cpu(fe->descTag.tagLocation));
			continue;
		}

		if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_FE)
		{
			ea_offset = sizeof(*fe);
			ea_length = le32_to_cpu(fe->lengthExtendedAttr);
			offset = ea_offset + ea_length;
			length = le32_to_cpu(fe->lengthAllocDescs);
			unique_id = le64_to_cpu(fe->uniqueID);
		}
		else if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_EFE)
		{
			efe = (struct extendedFileEntry *)&buffer;
			ea_offset = sizeof(*efe);
			ea_length = le32_to_cpu(efe->lengthExtendedAttr);
			offset = ea_offset + ea_length;
			length = le32_to_cpu(efe->lengthAllocDescs);
			unique_id = le64_to_cpu(efe->uniqueID);
		}
		else
			continue;

		if (length == 0)
		{
			fprintf(stderr, "%s: Warning: Information Control Block for Virtual Allocation Table is empty\n", appname);
			break;
		}

		if (ea_length > disc->blocksize || offset > disc->blocksize || length > disc->blocksize || offset + length > disc->blocksize)
		{
			fprintf(stderr, "%s: Warning: Information Control Block for Virtual Allocation Table is larger then block size\n", appname);
			break;
		}

		descs = malloc(length);
		if (!descs)
		{
			fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
			break;
		}

        if (read_offset(fd, disc, descs, (loff_t)i * disc->blocksize + offset, length, 1) < 0)
		{
			free(descs);
			break;
		}

		if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB)
		{
			if (le64_to_cpu(fe->informationLength) > length)
			{
				fprintf(stderr, "%s: Warning: Virtual Allocation Table inside of Information Control Block is larger then allocated block\n", appname);
				free(descs);
				break;
			}
			vat = descs;
			vat_length = le64_to_cpu(fe->informationLength);
		}
		else
		{
			sad = NULL;
			lad = NULL;
			if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
			{
				sad = (short_ad *)descs;
				count = length / sizeof(short_ad);
				vat_length = 0;
				for (j = 0; j < count; ++j)
				{
					if ((le32_to_cpu(sad[j].extLength) & EXT_LENGTH_MASK) >= UINT64_MAX - vat_length)
					{
						vat_length = UINT64_MAX;
						break;
					}
					vat_length += le32_to_cpu(sad[j].extLength) & EXT_LENGTH_MASK;
				}
			}
			else if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG)
			{
				lad = (long_ad *)descs;
				count = length / sizeof(long_ad);
				vat_length = 0;
				for (j = 0; j < count; ++j)
				{
					if ((le32_to_cpu(lad[j].extLength) & EXT_LENGTH_MASK) >= UINT64_MAX - vat_length)
					{
						vat_length = UINT64_MAX;
						break;
					}
					vat_length += le32_to_cpu(lad[j].extLength) & EXT_LENGTH_MASK;
				}
			}
			else
			{
				fprintf(stderr, "%s: Error: Information Control Block for Virtual Allocation Table has unknown Allocation Descriptors type\n", appname);
				free(descs);
				break;
			}

			if (vat_length == 0)
			{
				fprintf(stderr, "%s: Warning: Virtual Allocation Table is empty\n", appname);
				free(descs);
				break;
			}
			else if (vat_length > (uint64_t)256 * disc->blocksize)
			{
				fprintf(stderr, "%s: Warning: Virtual Allocation Table is too big\n", appname);
				free(descs);
				break;
			}

			/* Prefer non-virtual partition if exists */
			if (disc->udf_pd[0] && le16_to_cpu(disc->udf_pd[0]->partitionNumber) != le16_to_cpu(vpm->partitionNum))
			{
				ext_location = le32_to_cpu(disc->udf_pd[0]->partitionStartingLocation);
				ext_partition = le16_to_cpu(disc->udf_pd[0]->partitionNumber);
			}
			else if (disc->udf_pd[1] && le16_to_cpu(disc->udf_pd[1]->partitionNumber) != le16_to_cpu(vpm->partitionNum))
			{
				ext_location = le32_to_cpu(disc->udf_pd[1]->partitionStartingLocation);
				ext_partition = le16_to_cpu(disc->udf_pd[1]->partitionNumber);
			}
			else if (disc->udf_pd2[0] && le16_to_cpu(disc->udf_pd2[0]->partitionNumber) != le16_to_cpu(vpm->partitionNum))
			{
				ext_location = le32_to_cpu(disc->udf_pd2[0]->partitionStartingLocation);
				ext_partition = le16_to_cpu(disc->udf_pd2[0]->partitionNumber);
			}
			else if (disc->udf_pd2[1] && le16_to_cpu(disc->udf_pd2[1]->partitionNumber) != le16_to_cpu(vpm->partitionNum))
			{
				ext_location = le32_to_cpu(disc->udf_pd2[1]->partitionStartingLocation);
				ext_partition = le16_to_cpu(disc->udf_pd2[1]->partitionNumber);
			}
			else
			{
				ext_location = location;
				ext_partition = le16_to_cpu(vpm->partitionNum);
			}

			vat = malloc(vat_length);
			if (!vat)
			{
				fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
				free(descs);
				break;
			}

			vat_offset = 0;
			for (j = 0; j < count; ++j)
			{
				if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT)
				{
					ext_length = le32_to_cpu(sad[j].extLength) & EXT_LENGTH_MASK;
					ext_position = le32_to_cpu(sad[j].extPosition);
					if (ext_length == 0)
						continue;
				}
				else
				{
					ext_length = le32_to_cpu(lad[j].extLength) & EXT_LENGTH_MASK;
					ext_position = le32_to_cpu(lad[j].extLocation.logicalBlockNum);
					if (ext_length == 0)
						continue;
					if (le32_to_cpu(lad[j].extLocation.partitionReferenceNum) != ext_partition)
					{
						fprintf(stderr, "%s: Error: Virtual Allocation Table is stored on different partition\n", appname);
						count = 0;
						break;
					}
				}

                if (read_offset(fd, disc, vat + vat_offset, (loff_t)(ext_location + ext_position) * disc->blocksize, ext_length, 1) != 0)
				{
					fprintf(stderr, "%s: Error: Virtual Allocation Table is damaged\n", appname);
					count = 0;
					break;
				}

				vat_offset += ext_length;
			}

			free(descs);

			if (count == 0)
			{
				free(vat);
				break;
			}
		}

		if (fe->icbTag.fileType == ICBTAG_FILE_TYPE_VAT15)
		{
			if (vat_length < 36)
			{
				fprintf(stderr, "%s: Warning: Virtual Allocation Table is too small\n", appname);
				free(vat);
				break;
			}
			vat15 = (struct virtualAllocationTable15 *)(vat + ((vat_length - 36) / 4) * 4);
			if (strncmp((const char *)vat15->vatIdent.ident, UDF_ID_ALLOC, sizeof(vat15->vatIdent.ident)) != 0)
			{
				fprintf(stderr, "%s: Warning: Virtual Allocation Table is damaged\n", appname);
				free(vat);
				break;
			}
			uis = (struct UDFIdentSuffix *)vat15->vatIdent.identSuffix;
			if (disc->udf_write_rev < le16_to_cpu(uis->UDFRevision))
				disc->udf_write_rev = le16_to_cpu(uis->UDFRevision);
			disc->vat = (uint32_t *)vat;
			disc->vat_entries = (vat_length - 36) / 4;
			if (ea_length)
			{
				if (sizeof(ea_hdr) > ea_length)
					fprintf(stderr, "%s: Warning: Extended Attributes for Virtual Allocation Table are damaged\n", appname);
                else if (read_offset(fd, disc, &ea_hdr, (loff_t)i * disc->blocksize + ea_offset, sizeof(ea_hdr), 1) != 0)
					fprintf(stderr, "%s: Warning: Extended Attributes for Virtual Allocation Table are damaged\n", appname);
				else
				{
					/* UDF 1.50 3.3.4.1: if attribute does not exist then location point to byte after the EA space */
					ea_attr_offset = le32_to_cpu(ea_hdr.impAttrLocation);
					while (ea_attr_offset < ea_length)
					{
                        if (read_offset(fd, disc, &ea_attr, (loff_t)i * disc->blocksize + ea_offset + ea_attr_offset, sizeof(ea_attr), 1) != 0)
						{
							fprintf(stderr, "%s: Warning: Extended Attributes for Virtual Allocation Table are damaged\n", appname);
							break;
						}
						ea_attr_length = le32_to_cpu(ea_attr.attrLength);
						if (ea_attr_length == 0)
							break;
						if (ea_attr_offset + ea_attr_length > ea_length || sizeof(ea_attr) + le32_to_cpu(ea_attr.impUseLength) > ea_attr_length)
						{
							fprintf(stderr, "%s: Warning: Extended Attributes for Virtual Allocation Table are damaged\n", appname);
							break;
						}
						if (le32_to_cpu(ea_attr.attrType) == EXTATTR_IMP_USE && strncmp((const char *)ea_attr.impIdent.ident, UDF_ID_VAT_LVEXTENSION, sizeof(ea_attr.impIdent.ident)) == 0)
						{
							uis = (struct UDFIdentSuffix *)ea_attr.impIdent.identSuffix;
							if (disc->udf_write_rev < le16_to_cpu(uis->UDFRevision))
								disc->udf_write_rev = le16_to_cpu(uis->UDFRevision);
							if (ea_attr_length < sizeof(ea_attr) + sizeof(ea_lv) || le32_to_cpu(ea_attr.impUseLength) < sizeof(ea_lv))
							{
								fprintf(stderr, "%s: Warning: Logical Volume Extended Information for Virtual Allocation Table is damaged\n", appname);
								break;
							}
                            if (read_offset(fd, disc, &ea_lv, (loff_t)i * disc->blocksize + ea_offset + ea_attr_offset + sizeof(ea_attr), sizeof(ea_lv), 1) != 0)
							{
								fprintf(stderr, "%s: Warning: Logical Volume Extended Information for Virtual Allocation Table is damaged\n", appname);
								break;
							}
							if (le64_to_cpu(ea_lv.verificationID) != unique_id)
							{
								fprintf(stderr, "%s: Warning: Logical Volume Extended Information for Virtual Allocation Table is damaged\n", appname);
							}
							else
							{
								if (disc->udf_lvd[0])
									memcpy(disc->udf_lvd[0]->logicalVolIdent, ea_lv.logicalVolIdent, sizeof(ea_lv.logicalVolIdent));
								if (disc->udf_lvd[1])
									memcpy(disc->udf_lvd[1]->logicalVolIdent, ea_lv.logicalVolIdent, sizeof(ea_lv.logicalVolIdent));
								if (disc->udf_lvid)
								{
									disc->num_files = le32_to_cpu(ea_lv.numFiles);
									disc->num_dirs = le32_to_cpu(ea_lv.numDirs);
								}
								break;
							}
						}
						ea_attr_offset += ea_attr_length;
					}
				}
			}
		}
		else if (fe->icbTag.fileType == ICBTAG_FILE_TYPE_VAT20)
		{
			vat20 = (struct virtualAllocationTable20 *)vat;
			if (le16_to_cpu(vat20->lengthHeader) < sizeof(*vat20) || le16_to_cpu(vat20->lengthHeader) != sizeof(*vat20) + le16_to_cpu(vat20->lengthImpUse) || le16_to_cpu(vat20->lengthHeader) > vat_length)
			{
				fprintf(stderr, "%s: Warning: Virtual Allocation Table is damaged\n", appname);
				free(vat);
				break;
			}
			if (disc->udf_lvd[0])
				memcpy(disc->udf_lvd[0]->logicalVolIdent, vat20->logicalVolIdent, sizeof(vat20->logicalVolIdent));
			if (disc->udf_lvd[1])
				memcpy(disc->udf_lvd[1]->logicalVolIdent, vat20->logicalVolIdent, sizeof(vat20->logicalVolIdent));
			if (disc->udf_lvid)
			{
				disc->udf_rev = le16_to_cpu(vat20->minUDFReadRev);
				if (disc->udf_write_rev < le16_to_cpu(vat20->minUDFWriteRev))
					disc->udf_write_rev = le16_to_cpu(vat20->minUDFWriteRev);
				if (disc->udf_write_rev < disc->udf_rev)
					disc->udf_write_rev = disc->udf_rev;
				disc->num_files = le32_to_cpu(vat20->numFiles);
				disc->num_dirs = le32_to_cpu(vat20->numDirs);
			}
			disc->vat = (uint32_t *)(vat + le16_to_cpu(vat20->lengthHeader));
			disc->vat_entries = (vat_length - le16_to_cpu(vat20->lengthHeader)) / 4;
		}
		else
		{
			fprintf(stderr, "%s: Error: Wrong file type\n", appname);
			exit(1);
		}

		disc->vat_block = i;

		if (disc->udf_lvid)
			disc->udf_lvid->integrityType = cpu_to_le32(LVID_INTEGRITY_TYPE_CLOSE);

		if (i != vat_block)
			fprintf(stderr, "%s: Note: Found Virtual Allocation Table at block %"PRIu32" (expected at block %"PRIu32")\n", appname, i, vat_block);

		return;
	}

	fprintf(stderr, "%s: Error: Virtual Allocation Table not found, maybe wrong --vatblock?\n", appname);
}

static void read_metadata_file(int fd, struct udf_disc *disc, uint32_t start, uint32_t location, int mirror)
{
	short_ad *sad;
	struct fileEntry *fe;
	struct extendedFileEntry *efe;
	uint32_t length, offset;
	uint32_t ea_length, ea_offset;
	unsigned char buffer[512];

    if (read_offset(fd, disc, &buffer, (loff_t)(start + location) * disc->blocksize, sizeof(buffer), 1) < 0)
	{
		fprintf(stderr, "%s: Warning: Cannot read Metadata %sFile\n", appname, (mirror ? "Mirror " : ""));
		return;
	}

	fe = (struct fileEntry *)&buffer;

	if (fe->icbTag.fileType != (mirror ? ICBTAG_FILE_TYPE_MIRROR : ICBTAG_FILE_TYPE_MAIN))
	{
		fprintf(stderr, "%s: Warning: Information Control Block for Metadata %sFile has unknown File type\n", appname, (mirror ? "Mirror " : ""));
		return;
	}

	if ((le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK) != ICBTAG_FLAG_AD_SHORT)
	{
		fprintf(stderr, "%s: Warning: Information Control Block for Metadata %sFile has unknown Allocation Descriptors type\n", appname, (mirror ? "Mirror " : ""));
		return;
	}

	if (le32_to_cpu(fe->descTag.tagLocation) != location)
	{
		fprintf(stderr, "%s: Warning: Descriptor for Metadata %sFile has invalid location\n", appname, (mirror ? "Mirror " : ""));
		return;
	}

	if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_FE)
	{
		ea_offset = sizeof(*fe);
		ea_length = le32_to_cpu(fe->lengthExtendedAttr);
		offset = ea_offset + ea_length;
		length = le32_to_cpu(fe->lengthAllocDescs);
		if (le64_to_cpu(fe->uniqueID) != 0)
			fprintf(stderr, "%s: Warning: File Entry for Metadata %sFile has invalid Unique ID\n", appname, (mirror ? "Mirror " : ""));
	}
	else if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_EFE)
	{
		efe = (struct extendedFileEntry *)&buffer;
		ea_offset = sizeof(*efe);
		ea_length = le32_to_cpu(efe->lengthExtendedAttr);
		offset = ea_offset + ea_length;
		length = le32_to_cpu(efe->lengthAllocDescs);
		if (le64_to_cpu(efe->uniqueID) != 0)
			fprintf(stderr, "%s: Warning: Extended File Entry for Metadata %sFile has invalid Unique ID\n", appname, (mirror ? "Mirror " : ""));
	}
	else
	{
		fprintf(stderr, "%s: Warning: Descriptor for Metadata %sFile has unknown Tag Identifier\n", appname, (mirror ? "Mirror " : ""));
		return;
	}

	if (length < sizeof(short_ad))
	{
		fprintf(stderr, "%s: Warning: Allocation Descriptors for Metadata %sFile are empty\n", appname, (mirror ? "Mirror " : ""));
		return;
	}

	if (ea_length > disc->blocksize || offset > disc->blocksize || length > disc->blocksize || offset + length > disc->blocksize)
	{
		fprintf(stderr, "%s: Warning: Allocation Descriptors for Metadata %sFile are larger then block size\n", appname, (mirror ? "Mirror " : ""));
		return;
	}

	sad = malloc(length);
	if (!sad)
	{
		fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
		return;
	}

    if (read_offset(fd, disc, sad, (loff_t)(start + location) * disc->blocksize + offset, length, 1) < 0)
	{
		fprintf(stderr, "%s: Warning: Cannot read Allocation Descriptors for Metadata %sFile\n", appname, (mirror ? "Mirror " : ""));
		free(sad);
		return;
	}

	disc->metadata_filemap[mirror ? 1 : 0] = sad;
	disc->metadata_filemap_count[mirror ? 1 : 0] = length / sizeof(short_ad);
}

static void read_metadata(int fd, struct udf_disc *disc)
{
	struct partitionDesc *pd;
	struct metadataPartitionMap *mpm;
	struct UDFIdentSuffix *uis;

	mpm = (struct metadataPartitionMap *)find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_METADATA, -1, NULL);
	if (!mpm)
		return;

	uis = (struct UDFIdentSuffix *)mpm->partIdent.identSuffix;
	if (disc->udf_write_rev < le16_to_cpu(uis->UDFRevision))
		disc->udf_write_rev = le16_to_cpu(uis->UDFRevision);

	pd = find_partition_descriptor(disc, le16_to_cpu(mpm->partitionNum));
	if (!pd)
	{
		fprintf(stderr, "%s: Error: Metadata Partition Map found, but corresponding Partition Descriptor not found\n", appname);
		return;
	}

	read_metadata_file(fd, disc, le32_to_cpu(pd->partitionStartingLocation), le32_to_cpu(mpm->metadataFileLoc), 0);
	read_metadata_file(fd, disc, le32_to_cpu(pd->partitionStartingLocation), le32_to_cpu(mpm->metadataMirrorFileLoc), 1);
}

static void setup_pspace(struct udf_disc *disc, int second)
{
	struct partitionDesc *pd;
	struct udf_extent *ext, *new_ext;
	uint32_t location, blocks;

	if (!second)
	{
		if (disc->udf_pd[0])
			pd = disc->udf_pd[0];
		else if (disc->udf_pd[1])
			pd = disc->udf_pd[1];
		else
		{
			fprintf(stderr, "%s: Warning: Partition Space not found\n", appname);
			return;
		}
	}
	else
	{
		if (disc->udf_pd2[0])
			pd = disc->udf_pd2[0];
		else if (disc->udf_pd2[1])
			pd = disc->udf_pd2[1];
		else
			return;
	}

	location = le32_to_cpu(pd->partitionStartingLocation);

	blocks = le32_to_cpu(pd->partitionLength);
	if (!blocks)
	{
		fprintf(stderr, "%s: Warning: %sPartition Space not found\n", appname, second ? "Second " : "");
		return;
	}

	if (location + blocks > disc->blocks && !find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_VIRTUAL, -1, NULL))
		fprintf(stderr, "%s: Warning: %sPartition Space is beyond end of disk\n", appname, second ? "Second " : "");

	ext = find_extent(disc, location);
	if (ext->space_type != USPACE)
	{
		fprintf(stderr, "%s: Warning: %sPartition Space overlaps with other blocks\n", appname, second ? "Second " : "");
		if (ext->next && ext->next->space_type == USPACE)
		{
			ext = ext->next;
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
			if (!new_ext)
			{
				fprintf(stderr, "%s: Error: malloc failed: %s\n", appname, strerror(errno));
				return;
			}
			new_ext->space_type = PSPACE;
			new_ext->start = location;
			new_ext->blocks = blocks;
			new_ext->head = new_ext->tail = NULL;
			new_ext->prev = ext;
			new_ext->next = ext->next;
			new_ext->prev->next = new_ext;
			if (new_ext->next)
				new_ext->next->prev = new_ext;
		}
	}
	else
	{
		if ((location == ext->start || (location > ext->start && location + blocks > ext->start + ext->blocks)) && blocks > ext->blocks)
		{
			if (ext != disc->tail)
				fprintf(stderr, "%s: Warning: %sPartition Space overlaps with other blocks\n", appname, second ? "Second " : "");
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
	uint32_t block, location, position, length;
	struct genericPartitionMap *pmap;
	struct udf_extent *ext;
	struct partitionDesc *pd;
	struct domainIdentSuffix *dis;
	int id;

	if (disc->udf_lvd[0])
		id = 0;
	else if (disc->udf_lvd[1])
		id = 1;
	else
		return;

	ad = (long_ad *)disc->udf_lvd[id]->logicalVolContentsUse;
	block = le32_to_cpu(ad->extLocation.logicalBlockNum);
	partition = le16_to_cpu(ad->extLocation.partitionReferenceNum);
	length = le32_to_cpu(ad->extLength) & EXT_LENGTH_MASK;

	pmap = find_partition(disc, -1, NULL, -1, &partition);
	if (!pmap)
	{
		fprintf(stderr, "%s: Warning: Incorrect Logical Volume Descriptor\n", appname);
		return;
	}

	position = find_block_position(disc, pmap, block, &partition);
	if (position == UINT32_MAX)
	{
		fprintf(stderr, "%s: Warning: File Set Descriptor cannot be read\n", appname);
		return;
	}

	pd = find_partition_descriptor(disc, partition);
	if (!pd)
	{
		fprintf(stderr, "%s: Warning: File Set Descriptor cannot be read\n", appname);
		return;
	}

	location = le32_to_cpu(pd->partitionStartingLocation) + position;

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

    if (read_offset(fd, disc, disc->udf_fsd, (loff_t)location * disc->blocksize, length, 1) < 0)
	{
		free(disc->udf_fsd);
		disc->udf_fsd = NULL;
		return;
	}

	if (le32_to_cpu(disc->udf_fsd->descTag.tagLocation) != block)
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

	if (strncmp((char *)disc->udf_fsd->domainIdent.ident, UDF_ID_COMPLIANT, sizeof(disc->udf_fsd->domainIdent.ident)) != 0)
	{
		fprintf(stderr, "%s: Warning: Unsupported File Set Descriptor\n", appname);
		free(disc->udf_fsd);
		disc->udf_fsd = NULL;
		return;
	}

	dis = (struct domainIdentSuffix *)disc->udf_fsd->domainIdent.identSuffix;
	if (disc->udf_write_rev < le16_to_cpu(dis->UDFRevision))
		disc->udf_write_rev = le16_to_cpu(dis->UDFRevision);

	ext = next_extent(disc->head, PSPACE);
	if (ext)
		set_desc(ext, TAG_IDENT_FSD, location - ext->start, length, alloc_data(disc->udf_fsd, length));
}

static void setup_total_space_blocks(struct udf_disc *disc)
{
	int id;
	int warn_beyond;

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

	warn_beyond = !find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_VIRTUAL, -1, NULL);

	if (warn_beyond && disc->total_space_blocks + le32_to_cpu(disc->udf_pd[id]->partitionStartingLocation) > disc->blocks)
	{
		fprintf(stderr, "%s: Warning: Some space blocks are beyond end of disk\n", appname);
		warn_beyond = 0;
	}

	if (disc->udf_pd2[0])
		id = 0;
	else if (disc->udf_pd2[1])
		id = 1;
	else
		return;

	if (warn_beyond && le32_to_cpu(disc->udf_pd2[id]->partitionLength) + le32_to_cpu(disc->udf_pd2[id]->partitionStartingLocation) > disc->blocks)
		fprintf(stderr, "%s: Warning: Some space blocks are beyond end of disk\n", appname);

	disc->total_space_blocks += le32_to_cpu(disc->udf_pd2[id]->partitionLength);
}

static uint32_t count_bitmap_blocks(int fd, struct udf_disc *disc, struct genericPartitionMap *pmap, uint32_t block, uint32_t length)
{
	unsigned long int buffer[512/sizeof(unsigned long int)];
	unsigned long int val;
	uint32_t location;
	uint32_t position;
	uint16_t partition;
	struct partitionDesc *pd;
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

	position = find_block_position(disc, pmap, block, &partition);
	if (position == UINT32_MAX)
		return 0;

	pd = find_partition_descriptor(disc, partition);
	if (!pd)
		return 0;

	location = le32_to_cpu(pd->partitionStartingLocation) + position;

    if (read_offset(fd, disc, &sbd, (loff_t)location * disc->blocksize, sizeof(sbd), 1) < 0)
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
		if (read_nointr(fd, &buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer))
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
		if (read_nointr(fd, &buffer, bytes) != (ssize_t)bytes)
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

static uint32_t count_table_blocks(int fd, struct udf_disc *disc, struct genericPartitionMap *pmap, uint32_t block, uint32_t length)
{
	unsigned char buffer[512];
	uint32_t location;
	uint32_t position;
	uint16_t partition;
	struct partitionDesc *pd;
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

	position = find_block_position(disc, pmap, block, &partition);
	if (position == UINT32_MAX)
		return 0;

	pd = find_partition_descriptor(disc, partition);
	if (!pd)
		return 0;

	location = le32_to_cpu(pd->partitionStartingLocation) + position;

    if (read_offset(fd, disc, &buffer, (loff_t)location * disc->blocksize, sizeof(buffer), 1) < 0)
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
		if (read_nointr(fd, (uint8_t *)use + sizeof(buffer), use_len - sizeof(buffer)) != (ssize_t)(use_len - sizeof(buffer)))
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
			{
				blocks = le32_to_cpu(sad[i].extLength) & EXT_LENGTH_MASK;
				if (blocks <= UINT64_MAX - space)
					space += blocks;
				else
					space = UINT64_MAX;
			}
			break;

		case ICBTAG_FLAG_AD_LONG:
			lad = (long_ad *)&use->allocDescs[0];
			count = (use_len-sizeof(*use)) / sizeof(*lad);
			for (i = 0; i < count; ++i)
			{
				blocks = le32_to_cpu(lad[i].extLength) & EXT_LENGTH_MASK;
				if (blocks <= UINT64_MAX - space)
					space += blocks;
				else
					space = UINT64_MAX;
			}
			break;

		default:
			fprintf(stderr, "%s: Warning: Invalid Information Control Block in Space Entry\n", appname);
			break;
	}

	free(use);

	if (space > UINT64_MAX - (disc->blocksize-1))
		return UINT32_MAX;

	blocks = (space + disc->blocksize-1) / disc->blocksize;
	if (blocks > UINT32_MAX)
		return UINT32_MAX;

	return blocks;
}

static uint32_t count_free_partition_blocks(int fd, struct udf_disc *disc, struct partitionDesc *pd)
{
	uint16_t partition;
	uint32_t blocks, location, length, value;
	struct genericPartitionMap *pmap;
	struct virtualPartitionMap *vpm;
	struct partitionHeaderDesc *phd;
	char *ident;

	/* Use only main partition maps which span whole parition descriptor, so only Type 1 and Type 2 Sparable */
	partition = -1;
	pmap = find_partition(disc, GP_PARTITION_MAP_TYPE_1, NULL, le16_to_cpu(pd->partitionNumber), &partition);
	if (!pmap)
		pmap = find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_SPARABLE, le16_to_cpu(pd->partitionNumber), &partition);
	if (!pmap)
	{
		fprintf(stderr, "%s: Warning: Determining free space blocks is not possible\n", appname);
		return 0;
	}

	if (disc->vat)
		vpm = (struct virtualPartitionMap *)find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_VIRTUAL, le16_to_cpu(pd->partitionNumber), NULL);
	else
		vpm = NULL;

	/* LVID does not contain up-to-date information when VAT is built on top of main partition */
	if (!vpm && disc->udf_lvid && partition < le32_to_cpu(disc->udf_lvid->numOfPartitions))
	{
		memcpy(&value, &disc->udf_lvid->data[sizeof(uint32_t)*partition], sizeof(value));
		blocks = le32_to_cpu(value);
		if (blocks != 0xFFFFFFFF)
			return blocks;
	}

	location = le32_to_cpu(pd->partitionStartingLocation);

	/* If VAT is built on top of main partition then expects that all space blocks after VAT block are free */
	if (vpm)
		return location + disc->total_space_blocks - (disc->vat_block+1);

	ident = (char *)pd->partitionContents.ident;
	length = sizeof(pd->partitionContents.ident);
	if (strncmp(ident, PD_PARTITION_CONTENTS_NSR02, length) != 0 && strncmp(ident, PD_PARTITION_CONTENTS_NSR03, length) != 0)
	{
		fprintf(stderr, "%s: Warning: Unknown Partition Descriptor Content, determining free space blocks is not possible\n", appname);
		return 0;
	}

	phd = (struct partitionHeaderDesc *)pd->partitionContentsUse;

	length = le32_to_cpu(phd->unallocSpaceBitmap.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		blocks = count_bitmap_blocks(fd, disc, pmap, le32_to_cpu(phd->unallocSpaceBitmap.extPosition), length);
		if (blocks)
			return blocks;
	}

	length = le32_to_cpu(phd->freedSpaceBitmap.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		blocks = count_bitmap_blocks(fd, disc, pmap, le32_to_cpu(phd->freedSpaceBitmap.extPosition), length);
		if (blocks)
			return blocks;
	}

	length = le32_to_cpu(phd->unallocSpaceTable.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		blocks = count_table_blocks(fd, disc, pmap, le32_to_cpu(phd->unallocSpaceTable.extPosition), length);
		if (blocks)
			return blocks;
	}

	length = le32_to_cpu(phd->freedSpaceTable.extLength) & EXT_LENGTH_MASK;
	if (length)
	{
		blocks = count_table_blocks(fd, disc, pmap, le32_to_cpu(phd->freedSpaceTable.extPosition), length);
		if (blocks)
			return blocks;
	}

	return 0;
}

static void scan_free_space_blocks(int fd, struct udf_disc *disc)
{
	disc->free_space_blocks = 0;

	if (disc->udf_pd[0])
		disc->free_space_blocks += count_free_partition_blocks(fd, disc, disc->udf_pd[0]);
	else if (disc->udf_pd[1])
		disc->free_space_blocks += count_free_partition_blocks(fd, disc, disc->udf_pd[1]);
	else
	{
		fprintf(stderr, "%s: Warning: Determining free space blocks is not possible\n", appname);
		return;
	}

	if (disc->udf_pd2[0])
		disc->free_space_blocks += count_free_partition_blocks(fd, disc, disc->udf_pd2[0]);
	else if (disc->udf_pd2[1])
		disc->free_space_blocks += count_free_partition_blocks(fd, disc, disc->udf_pd2[1]);
}

int read_disc(int fd, struct udf_disc *disc)
{
	if (detect_udf(fd, disc) < 0)
		return -1;

	read_mbr(fd, disc);

	scan_mvds(fd, disc);
	scan_rvds(fd, disc);

	if (!disc->udf_anchor[1] && !disc->udf_anchor[2] && !find_partition(disc, GP_PARTITION_MAP_TYPE_2, UDF_ID_VIRTUAL, -1, NULL))
		fprintf(stderr, "%s: Warning: Second and third Anchor Volume Descriptor Pointer not found\n", appname);

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
	read_vat(fd, disc);
	read_metadata(fd, disc);
	setup_pspace(disc, 0);
	setup_pspace(disc, 1);

	/* TODO: setup USPACE extents */

	read_fsd(fd, disc);

	if (!disc->udf_fsd)
		fprintf(stderr, "%s: Warning: File Set Descriptor not found\n", appname);

	setup_total_space_blocks(disc);
	scan_free_space_blocks(fd, disc);

	return 0;
}
