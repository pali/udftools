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
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/fs.h>
#include <sys/ioctl.h>

#include "libudffs.h"
#include "options.h"
#include "../udfinfo/readdisc.h"

static uint64_t get_size(int fd)
{
	struct stat st;
	uint64_t size;
	off_t offset;

	if (fstat(fd, &st) == 0)
	{
		if (S_ISBLK(st.st_mode) && ioctl(fd, BLKGETSIZE64, &size) == 0)
			return size;
		else if (S_ISREG(st.st_mode))
			return st.st_size;
	}

	offset = lseek(fd, 0, SEEK_END);
	if (offset == (off_t)-1)
	{
		fprintf(stderr, "%s: Error: Cannot detect size of disk: %s\n", appname, strerror(errno));
		exit(1);
	}

	if (lseek(fd, 0, SEEK_SET) != 0)
	{
		fprintf(stderr, "%s: Error: Cannot seek to start of disk: %s\n", appname, strerror(errno));
		exit(1);
	}

	return offset;
}

static int get_sector_size(int fd)
{
	int size;

	if (ioctl(fd, BLKSSZGET, &size) != 0)
		return 0;

	if (size < 512 || size > 32768 || (size & (size - 1)))
	{
		fprintf(stderr, "%s: Warning: Disk logical sector size (%d) is not suitable for UDF\n", appname, size);
		return 0;
	}

	return size;
}

static uint16_t compute_crc(void *desc, size_t length)
{
	return udf_crc((uint8_t *)desc + sizeof(tag), length - sizeof(tag), 0);
}

static uint8_t compute_checksum(tag *tag)
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

static int check_desc(void *desc, size_t length)
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

static void update_desc(void *desc, size_t length)
{
	tag *tag = desc;
	if (length > le16_to_cpu(tag->descCRCLength) + sizeof(*tag))
		length = le16_to_cpu(tag->descCRCLength) + sizeof(*tag);
	tag->descCRC = cpu_to_le16(compute_crc(desc, length));
	tag->tagChecksum = compute_checksum(tag);
}

static void write_desc(int fd, struct udf_disc *disc, enum udf_space_type type, uint16_t ident, void *buffer)
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

int main(int argc, char *argv[])
{
	struct udf_disc disc;
	char *filename;
	struct partitionDesc *pd;
	struct logicalVolDesc *lvd;
	struct impUseVolDescImpUse *iuvdiu;
	struct domainIdentSuffix *dis;
	size_t len;
	int fd;
	int flags;
	int i;
	char buf[256];
	dstring new_lvid[128];
	dstring new_vid[32];
	dstring new_fsid[32];
	dstring new_fullvsid[128];
	char new_uuid[17];
	dstring new_vsid[128];
	dstring new_owner[36];
	dstring new_org[36];
	dstring new_contact[36];
	char new_appid[23];
	char new_impid[23];
	int force = 0;
	int update = 0;
	int update_pvd = 0;
	int update_lvd = 0;
	int update_iuvd = 0;
	int update_fsd = 0;

	if (fcntl(0, F_GETFL) < 0 && open("/dev/null", O_RDONLY) < 0)
		_exit(1);
	if (fcntl(1, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);
	if (fcntl(2, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);

	appname = "udflabel";

	if (!setlocale(LC_CTYPE, ""))
		fprintf(stderr, "%s: Error: Cannot set locale/codeset, fallback to default 7bit C ASCII\n", appname);

	memset(&disc, 0, sizeof(disc));

	disc.head = calloc(1, sizeof(struct udf_extent));
	if (!disc.head)
	{
		fprintf(stderr, "%s: Error: calloc failed: %s\n", appname, strerror(errno));
		exit(1);
	}

	disc.start_block = (uint32_t)-1;
	disc.flags = FLAG_LOCALE;
	disc.tail = disc.head;
	disc.head->space_type = USPACE;

	memset(new_lvid, 0, sizeof(new_lvid));
	memset(new_vid, 0, sizeof(new_vid));
	memset(new_fsid, 0, sizeof(new_fsid));
	memset(new_fullvsid, 0, sizeof(new_fullvsid));
	memset(new_vsid, 0, sizeof(new_vsid));
	memset(new_owner, 0, sizeof(new_owner));
	memset(new_org, 0, sizeof(new_org));
	memset(new_contact, 0, sizeof(new_contact));
	memset(new_appid, 0, sizeof(new_appid));
	memset(new_impid, 0, sizeof(new_impid));

	new_lvid[0] = 0xFF;
	new_vid[0] = 0xFF;
	new_fsid[0] = 0xFF;
	new_fullvsid[0] = 0xFF;
	new_uuid[0] = 0;
	new_vsid[0] = 0xFF;
	new_owner[0] = 0xFF;
	new_org[0] = 0xFF;
	new_contact[0] = 0xFF;
	new_appid[0] = (char)-1;
	new_impid[0] = (char)-1;

	parse_args(argc, argv, &disc, &filename, &force, new_lvid, new_vid, new_fsid, new_fullvsid, new_uuid, new_vsid, new_owner, new_org, new_contact, new_appid, new_impid);

	if (disc.flags & FLAG_NO_WRITE)
		printf("Note: Not writing to device, just simulating\n");

	if (new_lvid[0] != 0xFF)
	{
		update_lvd = 1;
		update_iuvd = 1;
		update_fsd = 1;
	}

	if (new_vid[0] != 0xFF)
		update_pvd = 1;

	if (new_fsid[0] != 0xFF)
		update_fsd = 1;

	if (new_owner[0] != 0xFF || new_org[0] != 0xFF || new_contact[0] != 0xFF)
		update_iuvd = 1;

	if (new_uuid[0] || new_vsid[0] != 0xFF || new_fullvsid[0] != 0xFF || new_appid[0] != (char)-1 || new_impid[0] != (char)-1)
		update_pvd = 1;

	if (update_pvd || update_lvd || update_iuvd || update_fsd)
		update = 1;

	if (update && !(disc.flags & FLAG_NO_WRITE))
		flags = O_RDWR | O_EXCL;
	else
		flags = O_RDONLY | O_EXCL;

	fd = open(filename, flags);
	if (fd < 0 && errno == EBUSY)
	{
		if (update)
		{
			fprintf(stderr, "%s: Error: Cannot open device '%s': Device is busy, maybe mounted?\n", appname, filename);
			exit(1);
		}
		flags &= ~O_EXCL;
		fd = open(filename, flags);
	}
	if (fd < 0)
	{
		fprintf(stderr, "%s: Error: Cannot open device '%s': %s\n", appname, filename, (errno != EBUSY) ? strerror(errno) : "Device is busy, maybe mounted?");
		exit(1);
	}

	if (!(flags & O_EXCL))
		fprintf(stderr, "%s: Warning: Device '%s' is busy, %s may report bogus information\n", appname, filename, appname);

	disc.blksize = get_size(fd);
	disc.blkssz = get_sector_size(fd);

	if (read_disc(fd, &disc) < 0)
	{
		fprintf(stderr, "%s: Error: Cannot process device '%s' as UDF disk\n", appname, filename);
		exit(1);
	}

	if (!update)
	{
		close(fd);

		if (disc.udf_lvd[0])
			lvd = disc.udf_lvd[0];
		else if (disc.udf_lvd[1])
			lvd = disc.udf_lvd[1];
		else
		{
			fprintf(stderr, "%s: Error: Logical Volume Descriptor is needed for label\n", appname);
			exit(1);
		}

		if (lvd->descCharSet.charSetType != UDF_CHAR_SET_TYPE || strncmp((const char *)lvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO, sizeof(lvd->descCharSet.charSetInfo)) != 0)
		{
			fprintf(stderr, "%s: Error: Label is not encoded in OSTA Unicode dstring\n", appname);
			exit(1);
		}

		len = decode_string(&disc, lvd->logicalVolIdent, buf, sizeof(lvd->logicalVolIdent), sizeof(buf));
		if (len == (size_t)-1)
		{
			fprintf(stderr, "%s: Error: Cannot decode label from OSTA Unicode dstring\n", appname);
			exit(1);
		}

		fwrite(buf, len, 1, stdout);
		putchar('\n');
		return 0;
	}

	printf("Updating device: %s\n", filename);

	if (!disc.udf_lvid || le32_to_cpu(disc.udf_lvid->integrityType) != LVID_INTEGRITY_TYPE_CLOSE)
	{
		fprintf(stderr, "%s: Error: Logical Volume is in inconsistent state\n", appname);
		exit(1);
	}

	if (disc.udf_write_rev > 0x0260)
	{
		fprintf(stderr, "%s: Error: Minimal UDF Write Revision is %"PRIx16".%02"PRIx16", but udflabel supports only 2.60\n", appname, disc.udf_write_rev >> 8, disc.udf_write_rev & 0xFF);
		exit(1);
	}

	if (disc.udf_pd[0])
		pd = disc.udf_pd[0];
	else if (disc.udf_pd[1])
		pd = disc.udf_pd[1];
	else
	{
		fprintf(stderr, "%s: Error: Both Main and Reserve Partition Descriptor are damaged\n", appname);
		exit(1);
	}

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
			if (!force && (new_fullvsid[0] != 0xFF || new_vid[0] != 0xFF))
			{
				fprintf(stderr, "%s: Error: Cannot update --vid, --vsid, --uuid or --fullvid on writeonce partition\n", appname);
				exit(1);
			}
			else if (!force && !disc.vat_block)
			{
				fprintf(stderr, "%s: Error: Cannot update writeonce partition without Virtual Allocation Table\n", appname);
				exit(1);
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
				exit(1);
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
				exit(1);
			}
			else
			{
				fprintf(stderr, "%s: Warning: Trying to overwrite partition with unknown access type\n", appname);
			}
			break;
	}

	/* TODO: VAT mode */
	if ((le32_to_cpu(pd->accessType) == PD_ACCESS_TYPE_WRITE_ONCE && !force) || (disc.vat && (new_lvid[0] != 0xFF || new_fsid[0] != 0xFF)))
	{
		fprintf(stderr, "%s: Error: Updating Virtual Allocation Table is not supported yet\n", appname);
		exit(1);
	}

	/* TODO: Pseudo OverWrite mode */
	if (le32_to_cpu(pd->accessType) == PD_ACCESS_TYPE_NONE && !force)
	{
		fprintf(stderr, "%s: Error: Updating pseudo-overwrite partition is not supported yet\n", appname);
		exit(1);
	}

	if (disc.udf_lvd[0] && update_lvd)
	{
		dis = (struct domainIdentSuffix *)disc.udf_lvd[0]->domainIdent.identSuffix;
		if (dis->domainFlags & (DOMAIN_FLAGS_SOFT_WRITE_PROTECT|DOMAIN_FLAGS_HARD_WRITE_PROTECT))
		{
			if (!force)
			{
				fprintf(stderr, "%s: Error: Cannot overwrite write protected Logical Volume Descriptor\n", appname);
				exit(1);
			}
			else
			{
				fprintf(stderr, "%s: Warning: Trying to overwrite write protected Logical Volume Descriptor\n", appname);
			}
		}
	}

	if (disc.udf_fsd && update_fsd)
	{
		dis = (struct domainIdentSuffix *)disc.udf_fsd->domainIdent.identSuffix;
		if (dis->domainFlags & (DOMAIN_FLAGS_SOFT_WRITE_PROTECT|DOMAIN_FLAGS_HARD_WRITE_PROTECT))
		{
			if (!force)
			{
				fprintf(stderr, "%s: Error: Cannot overwrite write protected File Set Descriptor\n", appname);
				exit(1);
			}
			else
			{
				fprintf(stderr, "%s: Warning: Trying to overwrite write protected File Set Descriptor\n", appname);
			}
		}
	}

	if (update_pvd)
	{
		if (!disc.udf_pvd[0] || !check_desc(disc.udf_pvd[0], sizeof(*disc.udf_pvd[0])))
		{
			fprintf(stderr, "%s: Error: Main Primary Volume Descriptor is damaged\n", appname);
			exit(1);
		}
		if (!disc.udf_pvd[1] || !check_desc(disc.udf_pvd[1], sizeof(*disc.udf_pvd[1])))
		{
			fprintf(stderr, "%s: Error: Reserve Primary Volume Descriptor is damaged\n", appname);
			exit(1);
		}
	}

	if ((new_uuid[0] || new_vsid[0] != 0xFF) && new_fullvsid[0] == 0xFF)
	{
		if (!new_uuid[0] || new_vsid[0] == 0xFF)
		{
			if (disc.udf_pvd[0]->descCharSet.charSetType != UDF_CHAR_SET_TYPE || strncmp((const char *)disc.udf_pvd[0]->descCharSet.charSetInfo, UDF_CHAR_SET_INFO, sizeof(disc.udf_pvd[0]->descCharSet.charSetInfo)) != 0)
			{
				fprintf(stderr, "%s: Error: Volume Set Identifier is not encoded in OSTA Unicode dstring\n", appname);
				fprintf(stderr, "%s: Error: In this case it is needed to specify both --vsid and --uuid\n", appname);
				exit(1);
			}
			memset(buf, 0, sizeof(buf));
			len = gen_uuid_from_vol_set_ident(buf, disc.udf_pvd[0]->volSetIdent, sizeof(disc.udf_pvd[0]->volSetIdent));
			if (len < 16)
			{
				fprintf(stderr, "%s: Error: First 16 characters of Volume Set Identifier are not hexadecimal lowercase digits\n", appname);
				fprintf(stderr, "%s: Error: In this case it is needed to specify both --vsid and --uuid\n", appname);
				exit(1);
			}
			if (!new_uuid[0])
				memcpy(new_uuid, buf, sizeof(new_uuid));
			else if (disc.udf_pvd[0]->volSetIdent[0] == 16)
			{
				new_vsid[0] = 16;
				new_vsid[127] = disc.udf_pvd[0]->volSetIdent[127]-32;
				memcpy(new_vsid+1, disc.udf_pvd[0]->volSetIdent+1+32, 127-1-32);
			}
			else
			{
				new_vsid[0] = 8;
				new_vsid[127] = disc.udf_pvd[0]->volSetIdent[127]-16;
				memcpy(new_vsid+1, disc.udf_pvd[0]->volSetIdent+1+16, 127-1-16);
			}
		}

		new_fullvsid[0] = new_vsid[0];
		if (new_vsid[0] != 16)
		{
			new_fullvsid[127] = 16+1;
			memcpy(new_fullvsid+1, new_uuid, 16);
			if (new_vsid[127])
			{
				memcpy(new_fullvsid+1+16, new_vsid+1, 127-1-16);
				new_fullvsid[127] += new_vsid[127]-1;
			}
		}
		else
		{
			new_fullvsid[127] = 32+1;
			for (i = 0; i < 16; ++i)
			{
				new_fullvsid[2*i+1] = 0;
				new_fullvsid[2*i+2] = new_uuid[i];
			}
			if (new_vsid[127])
			{
				memcpy(new_fullvsid+1+32, new_vsid+1, 127-1-32);
				new_fullvsid[127] += new_vsid[127]-1;
			}
		}
	}

	if (update_lvd)
	{
		if (!disc.udf_lvd[0] || !check_desc(disc.udf_lvd[0], sizeof(*disc.udf_lvd[0]) + le32_to_cpu(disc.udf_lvd[0]->mapTableLength)))
		{
			fprintf(stderr, "%s: Error: Main Logical Volume Descriptor is damaged\n", appname);
			exit(1);
		}
		if (!disc.udf_lvd[1] || !check_desc(disc.udf_lvd[1], sizeof(*disc.udf_lvd[1]) + le32_to_cpu(disc.udf_lvd[1]->mapTableLength)))
		{
			fprintf(stderr, "%s: Error: Reserve Logical Volume Descriptor is damaged\n", appname);
			exit(1);
		}
	}

	if (update_iuvd)
	{
		if (!disc.udf_iuvd[0] || !check_desc(disc.udf_iuvd[0], sizeof(*disc.udf_iuvd[0])))
		{
			fprintf(stderr, "%s: Error: Main Implementation Use Volume Descriptor is damaged\n", appname);
			exit(1);
		}
		if (!disc.udf_iuvd[1] || !check_desc(disc.udf_iuvd[1], sizeof(*disc.udf_iuvd[1])))
		{
			fprintf(stderr, "%s: Error: Reserve Implementation Use Volume Descriptor is damaged\n", appname);
			exit(1);
		}
	}

	if (update_fsd)
	{
		if (!disc.udf_fsd || !check_desc(disc.udf_fsd, sizeof(*disc.udf_fsd)))
		{
			fprintf(stderr, "%s: Error: File Set Descriptor is damaged\n", appname);
			exit(1);
		}
	}

	if (new_lvid[0] != 0xFF)
	{
		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_lvid, buf, sizeof(new_lvid), sizeof(buf));
		printf("Using new Logical Volume Identifier: %s\n", buf);
		memset(&disc.udf_lvd[0]->descCharSet, 0, sizeof(disc.udf_lvd[0]->descCharSet));
		memset(&disc.udf_lvd[1]->descCharSet, 0, sizeof(disc.udf_lvd[1]->descCharSet));
		disc.udf_lvd[0]->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
		disc.udf_lvd[1]->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)disc.udf_lvd[0]->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
		strcpy((char *)disc.udf_lvd[1]->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(disc.udf_lvd[0]->logicalVolIdent, new_lvid, sizeof(new_lvid));
		memcpy(disc.udf_lvd[1]->logicalVolIdent, new_lvid, sizeof(new_lvid));
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[0]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->logicalVolIdent, new_lvid, sizeof(new_lvid));
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[1]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->logicalVolIdent, new_lvid, sizeof(new_lvid));
		memset(&disc.udf_fsd->logicalVolIdentCharSet, 0, sizeof(disc.udf_fsd->logicalVolIdentCharSet));
		disc.udf_fsd->logicalVolIdentCharSet.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)disc.udf_fsd->logicalVolIdentCharSet.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(disc.udf_fsd->logicalVolIdent, new_lvid, sizeof(new_lvid));
	}

	if (new_vid[0] != 0xFF)
	{
		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_vid, buf, sizeof(new_vid), sizeof(buf));
		printf("Using new Volume Identifier: %s\n", buf);
		memset(&disc.udf_pvd[0]->descCharSet, 0, sizeof(disc.udf_pvd[0]->descCharSet));
		memset(&disc.udf_pvd[1]->descCharSet, 0, sizeof(disc.udf_pvd[1]->descCharSet));
		disc.udf_pvd[0]->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
		disc.udf_pvd[1]->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)disc.udf_pvd[0]->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
		strcpy((char *)disc.udf_pvd[1]->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(disc.udf_pvd[0]->volIdent, new_vid, sizeof(new_vid));
		memcpy(disc.udf_pvd[1]->volIdent, new_vid, sizeof(new_vid));
	}

	if (new_fsid[0] != 0xFF)
	{
		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_fsid, buf, sizeof(new_fsid), sizeof(buf));
		printf("Using new File Set Identifier: %s\n", buf);
		memset(&disc.udf_fsd->fileSetCharSet, 0, sizeof(disc.udf_fsd->fileSetCharSet));
		disc.udf_fsd->fileSetCharSet.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)disc.udf_fsd->fileSetCharSet.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(disc.udf_fsd->fileSetIdent, new_fsid, sizeof(new_fsid));
	}

	if (new_owner[0] != 0xFF)
	{
		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_owner, buf, sizeof(new_owner), sizeof(buf));
		printf("Using new Owner name: %s\n", buf);
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[0]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->LVInfo1, new_owner, sizeof(new_owner));
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[1]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->LVInfo1, new_owner, sizeof(new_owner));
	}

	if (new_org[0] != 0xFF)
	{
		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_org, buf, sizeof(new_org), sizeof(buf));
		printf("Using new Oranization name: %s\n", buf);
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[0]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->LVInfo2, new_org, sizeof(new_org));
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[1]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->LVInfo2, new_org, sizeof(new_org));
	}

	if (new_contact[0] != 0xFF)
	{
		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_contact, buf, sizeof(new_contact), sizeof(buf));
		printf("Using new Contact information: %s\n", buf);
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[0]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->LVInfo3, new_contact, sizeof(new_contact));
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[1]->impUse;
		memset(&iuvdiu->LVICharset, 0, sizeof(iuvdiu->LVICharset));
		iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);
		memcpy(iuvdiu->LVInfo3, new_contact, sizeof(new_contact));
	}

	if (new_appid[0] != (char)-1)
	{
		printf("Using new Application Identifier: %.*s\n", (int)sizeof(new_appid), new_appid);
		memset(&disc.udf_pvd[0]->appIdent, 0, sizeof(disc.udf_pvd[0]->appIdent));
		memset(&disc.udf_pvd[1]->appIdent, 0, sizeof(disc.udf_pvd[1]->appIdent));
		memcpy(disc.udf_pvd[0]->appIdent.ident, new_appid, sizeof(new_appid));
		memcpy(disc.udf_pvd[1]->appIdent.ident, new_appid, sizeof(new_appid));
	}

	if (new_impid[0] != (char)-1)
	{
		printf("Using new Implementation Identifier: %.*s\n", (int)sizeof(new_impid), new_impid);
		/* Do not erase identSuffix which contains Operating System Class and Id */
		memcpy(disc.udf_pvd[0]->impIdent.ident, new_impid, sizeof(new_impid));
		memcpy(disc.udf_pvd[1]->impIdent.ident, new_impid, sizeof(new_impid));
	}

	if (new_fullvsid[0] != 0xFF)
	{
		memset(buf, 0, sizeof(buf));
		len = gen_uuid_from_vol_set_ident(buf, new_fullvsid, sizeof(new_fullvsid));
		printf("Using new UUID: %s\n", buf);

		memcpy(new_vsid, new_fullvsid, sizeof(new_vsid));
		if (len >= 8)
		{
			if (len < 16)
				len = 1;
			else
				len = 2;
			if (new_vsid[127] > new_vsid[0]*len)
			{
				new_vsid[127] -= new_vsid[0]*len;
				memmove(new_vsid + 1, new_vsid + new_vsid[0]*len + 1, new_vsid[127] - 1);
			}
			else
			{
				new_vsid[0] = 0;
			}
		}
		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_vsid, buf, sizeof(new_vsid), sizeof(buf));
		printf("Using new Volume Set Identifier: %s\n", buf);

		memset(buf, 0, sizeof(buf));
		decode_string(&disc, new_fullvsid, buf, sizeof(new_fullvsid), sizeof(buf));
		printf("Using new full Volume Set Identifier: %s\n", buf);

		memset(&disc.udf_pvd[0]->descCharSet, 0, sizeof(disc.udf_pvd[0]->descCharSet));
		memset(&disc.udf_pvd[1]->descCharSet, 0, sizeof(disc.udf_pvd[1]->descCharSet));
		disc.udf_pvd[0]->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
		disc.udf_pvd[1]->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
		strcpy((char *)disc.udf_pvd[0]->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
		strcpy((char *)disc.udf_pvd[1]->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);

		memcpy(disc.udf_pvd[0]->volSetIdent, new_fullvsid, sizeof(new_fullvsid));
		memcpy(disc.udf_pvd[1]->volSetIdent, new_fullvsid, sizeof(new_fullvsid));
	}

	if (update_pvd)
	{
		printf("Updating Main Primary Volume Descriptor...\n");
		update_desc(disc.udf_pvd[0], sizeof(*disc.udf_pvd[0]));
		write_desc(fd, &disc, MVDS, TAG_IDENT_PVD, disc.udf_pvd[0]);
	}

	if (update_lvd)
	{
		printf("Updating Main Logical Volume Descriptor...\n");
		update_desc(disc.udf_lvd[0], sizeof(*disc.udf_lvd[0]) + le32_to_cpu(disc.udf_lvd[0]->mapTableLength));
		write_desc(fd, &disc, MVDS, TAG_IDENT_LVD, disc.udf_lvd[0]);
	}

	if (update_iuvd)
	{
		printf("Updating Main Implementation Use Volume Descriptor...\n");
		update_desc(disc.udf_iuvd[0], sizeof(*disc.udf_iuvd[0]));
		write_desc(fd, &disc, MVDS, TAG_IDENT_IUVD, disc.udf_iuvd[0]);
	}

	if (update_pvd || update_lvd || update_iuvd)
	{
		printf("Synchronizing...\n");
		if (!(disc.flags & FLAG_NO_WRITE))
		{
			if (fdatasync(fd) != 0)
			{
				fprintf(stderr, "%s: Error: Synchronization to device '%s' failed: %s\n", appname, filename, strerror(errno));
				exit(1);
			}
		}
	}

	if (update_fsd)
	{
		printf("Updating File Set Descriptor...\n");
		update_desc(disc.udf_fsd, sizeof(*disc.udf_fsd));
		write_desc(fd, &disc, PSPACE, TAG_IDENT_FSD, disc.udf_fsd);
	}

	if (update_pvd && disc.udf_pvd[1] != disc.udf_pvd[0])
	{
		printf("Updating Reserve Primary Volume Descriptor...\n");
		update_desc(disc.udf_pvd[1], sizeof(*disc.udf_pvd[1]));
		write_desc(fd, &disc, RVDS, TAG_IDENT_PVD, disc.udf_pvd[1]);
	}

	if (update_lvd && disc.udf_lvd[1] != disc.udf_lvd[0])
	{
		printf("Updating Reserve Logical Volume Descriptor...\n");
		update_desc(disc.udf_lvd[1], sizeof(*disc.udf_lvd[1]) + le32_to_cpu(disc.udf_lvd[1]->mapTableLength));
		write_desc(fd, &disc, RVDS, TAG_IDENT_LVD, disc.udf_lvd[1]);
	}

	if (update_iuvd && disc.udf_iuvd[1] != disc.udf_iuvd[0])
	{
		printf("Updating Reserve Implementation Use Volume Descriptor...\n");
		update_desc(disc.udf_iuvd[1], sizeof(*disc.udf_iuvd[1]));
		write_desc(fd, &disc, RVDS, TAG_IDENT_IUVD, disc.udf_iuvd[1]);
	}

	printf("Synchronizing...\n");
	if (!(disc.flags & FLAG_NO_WRITE))
	{
		if (fsync(fd) != 0)
		{
			fprintf(stderr, "%s: Error: Synchronization to device '%s' failed: %s\n", appname, filename, strerror(errno));
			exit(1);
		}
	}

	if (close(fd) != 0 && errno != EINTR)
	{
		fprintf(stderr, "%s: Error: Closing device '%s' failed: %s\n", appname, filename, strerror(errno));
		exit(1);
	}

	printf("Done\n");
	return 0;
}
