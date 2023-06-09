/*
 * Copyright (C) 2017-2021  Pali Rohár <pali.rohar@gmail.com>
 * Copyright (C) 2023  Johannes Truschnigg <johannes@truschnigg.info>
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
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libudffs.h"
#include "updatedisc.h"
#include "../udfinfo/readdisc.h"

uint16_t compute_crc(void *desc, size_t length)
{
	return udf_crc((uint8_t *)desc + sizeof(tag), length - sizeof(tag), 0);
}

uint8_t compute_checksum(tag *tag)
{
	uint8_t i, checksum = 0;
	for (i = 0; i < 16; i++)
	{
		if (i == 4)
			continue;
		checksum += ((uint8_t *)tag)[i];
	}
	return checksum;
}

int check_desc(void *desc, size_t length)
{
	tag *tag = desc;
	uint16_t crc_length = le16_to_cpu(tag->descCRCLength);
	if (crc_length > length - sizeof(*tag))
		return 0;
	if (compute_checksum(tag) != tag->tagChecksum)
		return 0;
	if (compute_crc(desc, sizeof(*tag) + crc_length) != le16_to_cpu(tag->descCRC))
		return 0;
	return 1;
}

void update_desc(void *desc, size_t length)
{
	tag *tag = desc;
	if (length > le16_to_cpu(tag->descCRCLength) + sizeof(*tag))
		length = le16_to_cpu(tag->descCRCLength) + sizeof(*tag);
	tag->descCRC = cpu_to_le16(compute_crc(desc, length));
	tag->tagChecksum = compute_checksum(tag);
}

void write_desc(int fd, struct udf_disc *disc, enum udf_space_type type, uint16_t ident, void *buffer)
{
	struct udf_extent *ext;
	struct udf_desc *desc;
	off_t off;
	off_t offset;
	ssize_t ret;

	ext = disc->head;
	while ((ext = next_extent(ext, type)))
	{
		desc = ext->head;
		while ((desc = next_desc(desc, ident)))
		{
			if (!desc->data || desc->data->buffer != buffer)
				continue;

			printf("  ... at block %"PRIu32"\n", ext->start + desc->offset);

			offset = (off_t)disc->blocksize * (ext->start + desc->offset);
			off = lseek(fd, offset, SEEK_SET);
			if (off != (off_t)-1 && off != offset)
			{
				errno = EIO;
				off = (off_t)-1;
			}
			if (off == (off_t)-1)
			{
				fprintf(stderr, "%s: Error: lseek failed: %s\n", appname, strerror(errno));
				return;
			}

			if (!(disc->flags & FLAG_NO_WRITE))
			{
				ret = write_nointr(fd, desc->data->buffer, desc->data->length);
				if (ret >= 0 && (size_t)ret != desc->data->length)
				{
					errno = EIO;
					ret = -1;
				}
				if (ret < 0)
				{
					fprintf(stderr, "%s: Error: write failed: %s\n", appname, strerror(errno));
					return;
				}
			}

			return;
		}
	}

	fprintf(stderr, "%s: Error: Cannot find needed block for write\n", appname);
	return;
}

int open_existing_disc(struct udf_disc *disc, char *filename, int flags, int update, char *buf)
{
	int fd;

	fd = open(filename, flags);
	if (fd < 0 && errno == EBUSY)
	{
		if (update)
		{
			fprintf(stderr, "%s: Error: Cannot open device '%s': Device is busy, maybe mounted?\n", appname, filename);
			return -1;
		}
		flags &= ~O_EXCL;
		fd = open(filename, flags);
	}
	if (fd < 0)
	{
		fprintf(stderr, "%s: Error: Cannot open device '%s': %s\n", appname, filename, (errno != EBUSY) ? strerror(errno) : "Device is busy, maybe mounted?");
		return -1;
	}

	if (!(flags & O_EXCL))
		fprintf(stderr, "%s: Warning: Device '%s' is busy, %s may report bogus information\n", appname, filename, appname);

	disc->blksize = get_size(fd);
	disc->blkssz = get_sector_size(fd);

	if (read_disc(fd, disc) < 0)
	{
		fprintf(stderr, "%s: Error: Cannot process device '%s' as UDF disk\n", appname, filename);
		return -1;
	}

	if (disc->udf_write_rev > 0x0260)
	{
		fprintf(stderr, "%s: Error: Minimal UDF Write Revision is %"PRIx16".%02"PRIx16", but %s supports only 2.60\n", appname, disc->udf_write_rev >> 8, disc->udf_write_rev & 0xFF, appname);
		return -1;
	}

	if (!disc->udf_lvid || le32_to_cpu(disc->udf_lvid->integrityType) != LVID_INTEGRITY_TYPE_CLOSE)
	{
		fprintf(stderr, "%s: Error: Logical Volume is in inconsistent state\n", appname);
		return -1;
	}

	if (update && disc->flags & FLAG_NO_WRITE)
		printf("Note: Not writing to device, just simulating\n");

	if (update)
		printf("Updating device: %s\n", filename);
	return fd;
}

int verify_lvd(struct udf_disc *disc, char *appname)
{
	int result = 1;
	if (!disc->udf_lvd[0] || !check_desc(disc->udf_lvd[0], sizeof(*disc->udf_lvd[0]) + le32_to_cpu(disc->udf_lvd[0]->mapTableLength)))
	{
		fprintf(stderr, "%s: Error: Main Logical Volume Descriptor is damaged\n", appname);
		result = 0;
	}
	if (!disc->udf_lvd[1] || !check_desc(disc->udf_lvd[1], sizeof(*disc->udf_lvd[1]) + le32_to_cpu(disc->udf_lvd[1]->mapTableLength)))
	{
		fprintf(stderr, "%s: Error: Reserve Logical Volume Descriptor is damaged\n", appname);
		result = 0;
	}
	return result;
}

int verify_fsd(struct udf_disc *disc, char *appname)
{
	int result = 1;
	if (!disc->udf_fsd || !check_desc(disc->udf_fsd, sizeof(*disc->udf_fsd)))
	{
		fprintf(stderr, "%s: Error: File Set Descriptor is damaged\n", appname);
		result = 0;
	}
	return result;
}

int check_wr_lvd(struct udf_disc *disc, char *appname, int force)
{
	int result = 1;
	struct domainIdentSuffix *dis;

	dis = (struct domainIdentSuffix *)disc->udf_lvd[0]->domainIdent.identSuffix;
	if (dis->domainFlags & (DOMAIN_FLAGS_SOFT_WRITE_PROTECT|DOMAIN_FLAGS_HARD_WRITE_PROTECT))
	{
		if (!force)
		{
			fprintf(stderr, "%s: Error: Cannot overwrite write protected Logical Volume Descriptor\n", appname);
			result = 0;
		}
		else
		{
			fprintf(stderr, "%s: Warning: Trying to overwrite write protected Logical Volume Descriptor\n", appname);
		}
	}
	return result;
}

int check_wr_fsd(struct udf_disc *disc, char *appname, int force)
{
	int result = 1;
	struct domainIdentSuffix *dis;

	dis = (struct domainIdentSuffix *)disc->udf_fsd->domainIdent.identSuffix;
	if (dis->domainFlags & (DOMAIN_FLAGS_SOFT_WRITE_PROTECT|DOMAIN_FLAGS_HARD_WRITE_PROTECT))
	{
		if (!force)
		{
			fprintf(stderr, "%s: Error: Cannot overwrite write protected File Set Descriptor\n", appname);
			result = 0;
		}
		else
		{
			fprintf(stderr, "%s: Warning: Trying to overwrite write protected File Set Descriptor\n", appname);
		}
	}
	return result;
}

int sync_device(struct udf_disc *disc, int fd, char *appname, char *filename)
{
	int result = 1;

	printf("Synchronizing...\n");
	if (!(disc->flags & FLAG_NO_WRITE))
	{
		if (fsync(fd) != 0)
		{
			fprintf(stderr, "%s: Error: Synchronization to device '%s' failed: %s\n", appname, filename, strerror(errno));
			result = 0;
		}
	}

	if (close(fd) != 0 && errno != EINTR)
	{
		fprintf(stderr, "%s: Error: Closing device '%s' failed: %s\n", appname, filename, strerror(errno));
		result = 0;
	}

	printf("Done\n");
	return result;
}

int check_access_type(struct udf_disc *disc, struct partitionDesc *pd, char *appname, int force, int update_vid)
{
	int result = 1;

	switch (le32_to_cpu(pd->accessType))
	{
		case PD_ACCESS_TYPE_OVERWRITABLE:
		case PD_ACCESS_TYPE_REWRITABLE:
			break;

		case PD_ACCESS_TYPE_NONE: /* Pseudo OverWrite */
			if (force)
				fprintf(stderr, "%s: Warning: Trying to overwrite pseudo-overwrite partition\n", appname);
			break;

		case PD_ACCESS_TYPE_WRITE_ONCE:
			if (!force && update_vid)
			{
				fprintf(stderr, "%s: Error: Cannot update --vid, --vsid, --uuid or --fullvid on writeonce partition\n", appname);
				result = 0;
			}
			else if (!force && !disc->vat_block)
			{
				fprintf(stderr, "%s: Error: Cannot update writeonce partition without Virtual Allocation Table\n", appname);
				result = 0;
			}
			else if (force)
			{
				fprintf(stderr, "%s: Warning: Trying to overwrite writeonce partition\n", appname);
			}
			break;

		case PD_ACCESS_TYPE_READ_ONLY:
			if (!force)
			{
				fprintf(stderr, "%s: Error: Cannot overwrite readonly partition\n", appname);
				result = 0;
			}
			else
			{
				fprintf(stderr, "%s: Warning: Trying to overwrite readonly partition\n", appname);
			}
			break;

		default:
			if (!force)
			{
				fprintf(stderr, "%s: Error: Partition Access Type is unknown\n", appname);
				result = 0;
			}
			else
			{
				fprintf(stderr, "%s: Warning: Trying to overwrite partition with unknown access type\n", appname);
			}
			break;
	}
	return result;
}
