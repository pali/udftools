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
#include "readdisc.h"

static uint32_t compute_windows_serial_num(struct udf_disc *disc)
{
	uint8_t *buffer = (uint8_t *)disc->udf_fsd;
	size_t len = sizeof(*disc->udf_fsd);
	uint8_t checksum[4] = { 0, 0, 0, 0 };

	if (!buffer)
		return 0;

	while (len--)
		checksum[len & 0x3] += *buffer++;

	return (checksum[0] << 24) | (checksum[1] << 16) | (checksum[2] << 8) | checksum[3];
}

static uint32_t compute_behind_blocks(struct udf_disc *disc)
{
	struct udf_extent *start_ext;
	uint32_t last_block = 0;

	for (start_ext = disc->head; start_ext != NULL; start_ext = start_ext->next)
	{
		if (start_ext->space_type & USPACE)
			continue;
		if (last_block < start_ext->start + start_ext->blocks)
			last_block = start_ext->start + start_ext->blocks;
	}

	if (disc->blocks > last_block)
		return disc->blocks - last_block;
	else
		return 0;
}

static void print_dstring(struct udf_disc *disc, const char *name, const charspec *charset, const dstring *string, size_t len)
{
	char buf[256];
	size_t i;

	fputs(name, stdout);
	putchar('=');
	if (string && charset && charset->charSetType == UDF_CHAR_SET_TYPE && strncmp((const char *)charset->charSetInfo, UDF_CHAR_SET_INFO, sizeof(charset->charSetInfo)) == 0)
	{
		len = decode_string(disc, string, buf, len, sizeof(buf));
		if (len != (size_t)-1)
		{
			for (i = 0; i < len; ++i)
			{
				if (buf[i] == '\n')
					buf[i] = ' ';
			}
			fwrite(buf, len, 1, stdout);
		}
	}
	putchar('\n');
}

static void print_astring(struct udf_disc *disc, const char *name, const uint8_t *astring, size_t len)
{
	dstring string[256];

	if (astring && len+2 > sizeof(string))
	{
		fprintf(stderr, "%s: Error: too small string buffer for %s\n", appname, name);
		exit(1);
	}

	if (astring)
	{
		/* Convert 7bit ASCII string into OSTA UNICODE 8bit dstring */
		string[0] = 0x08;
		string[len+1] = strnlen((const char *)astring, len) + 1;
		memcpy(string+1, astring, len);
	}

	print_dstring(disc, name, &(const charspec){ UDF_CHAR_SET_TYPE, UDF_CHAR_SET_INFO }, astring ? string : NULL, len+2);
}

static const char *udf_space_type_str[UDF_SPACE_TYPE_SIZE] = { "RESERVED", "VRS", "ANCHOR", "MVDS", "RVDS", "LVID", "STABLE", "SSPACE", "PSPACE", "USPACE", "BAD", "MBR" };

static void dump_space(struct udf_disc *disc)
{
	struct udf_extent *start_ext;
	int i;

	for (start_ext = disc->head; start_ext != NULL; start_ext = start_ext->next)
	{
		for (i = 0; i < UDF_SPACE_TYPE_SIZE; ++i)
		{
			if (!(start_ext->space_type & (1<<i)))
				continue;
			if (start_ext->space_type & (USPACE|RESERVED))
				continue;
			printf("start=%"PRIu32", blocks=%"PRIu32", type=%s\n", start_ext->start, start_ext->blocks, udf_space_type_str[i]);
		}
	}
}

int main(int argc, char *argv[])
{
	struct udf_disc disc;
	dstring vsid[128];
	char uuid[17];
	char *filename;
	struct logicalVolDesc *lvd;
	struct primaryVolDesc *pvd;
	struct partitionDesc *pd;
	struct impUseVolDescImpUse *iuvdiu;
	struct domainIdentSuffix *dis;
	uint32_t serial_num;
	uint32_t used_blocks;
	uint32_t behind_blocks;
	int soft_write_protect;
	int hard_write_protect;
	size_t ret;
	int fd;

	appname = "udfinfo";

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

	parse_args(argc, argv, &disc, &filename);

	fd = open(filename, O_RDONLY|O_EXCL);
	if (fd < 0 && errno == EBUSY)
	{
		fprintf(stderr, "%s: Warning: Device '%s' is busy, %s may report bogus information\n", appname, filename, appname);
		fd = open(filename, O_RDONLY);
	}
	if (fd < 0)
	{
		fprintf(stderr, "%s: Error: Cannot open device '%s': %s\n", appname, filename, strerror(errno));
		exit(1);
	}

	disc.blksize = get_size(fd);
	disc.blkssz = get_sector_size(fd);

	if (read_disc(fd, &disc) < 0)
	{
		fprintf(stderr, "%s: Error: Cannot process device '%s' as UDF disk\n", appname, filename);
		exit(1);
	}

	close(fd);

	if (disc.udf_lvd[0])
		lvd = disc.udf_lvd[0];
	else if (disc.udf_lvd[1])
		lvd = disc.udf_lvd[1];
	else
		lvd = NULL;

	if (disc.udf_pvd[0])
		pvd = disc.udf_pvd[0];
	else if (disc.udf_pvd[1])
		pvd = disc.udf_pvd[1];
	else
		pvd = NULL;

	if (disc.udf_pd[0])
		pd = disc.udf_pd[0];
	else if (disc.udf_pd[1])
		pd = disc.udf_pd[1];
	else
		pd = NULL;

	if (disc.udf_iuvd[0])
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[0]->impUse;
	else if (disc.udf_iuvd[1])
		iuvdiu = (struct impUseVolDescImpUse *)disc.udf_iuvd[1]->impUse;
	else
		iuvdiu = NULL;

	if (disc.total_space_blocks < disc.free_space_blocks)
		used_blocks = disc.total_space_blocks;
	else
		used_blocks = disc.total_space_blocks - disc.free_space_blocks;

	behind_blocks = compute_behind_blocks(&disc);

	serial_num = compute_windows_serial_num(&disc);

	if (pvd && pvd->descCharSet.charSetType == UDF_CHAR_SET_TYPE && strncmp((const char *)pvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO, sizeof(pvd->descCharSet.charSetInfo)) == 0)
	{
		ret = gen_uuid_from_vol_set_ident(uuid, pvd->volSetIdent, sizeof(pvd->volSetIdent));
		if (ret < 8)
		{
			memcpy(vsid, pvd->volSetIdent, 128);
		}
		else
		{
			vsid[0] = pvd->volSetIdent[0];
			vsid[127] = 0;
			if (ret < 16)
				ret = 1;
			else
				ret = 2;
			if (pvd->volSetIdent[127] > vsid[0]*ret)
			{
				vsid[127] = pvd->volSetIdent[127] - vsid[0]*ret;
				memcpy(vsid + 1, pvd->volSetIdent + vsid[0]*ret + 1, vsid[127] - 1);
			}
		}
	}
	else
	{
		uuid[0] = 0;
		vsid[0] = 0;
		vsid[127] = 0;
	}

	if (!disc.udf_lvid || le32_to_cpu(disc.udf_lvid->integrityType) != LVID_INTEGRITY_TYPE_CLOSE)
		fprintf(stderr, "%s: Warning: Logical Volume is in inconsistent state\n", appname);

	printf("filename=%s\n", filename);
	print_dstring(&disc, "label", lvd ? &lvd->descCharSet : NULL, lvd ? lvd->logicalVolIdent : NULL, sizeof(lvd->logicalVolIdent));
	printf("uuid=%s\n", uuid);
	print_dstring(&disc, "lvid", lvd ? &lvd->descCharSet : NULL, lvd ? lvd->logicalVolIdent : NULL, sizeof(lvd->logicalVolIdent));
	print_dstring(&disc, "vid", pvd ? &pvd->descCharSet : NULL, pvd ? pvd->volIdent : NULL, sizeof(pvd->volIdent));
	print_dstring(&disc, "vsid", pvd ? &pvd->descCharSet : NULL, vsid, sizeof(vsid));
	print_dstring(&disc, "fsid", disc.udf_fsd ? &disc.udf_fsd->fileSetCharSet : NULL, disc.udf_fsd ? disc.udf_fsd->fileSetIdent : NULL, sizeof(disc.udf_fsd->fileSetIdent));
	print_dstring(&disc, "fullvsid", pvd ? &pvd->descCharSet : NULL, pvd ? pvd->volSetIdent : NULL, sizeof(pvd->volSetIdent));
	print_dstring(&disc, "owner", iuvdiu ? &iuvdiu->LVICharset : NULL, iuvdiu ? iuvdiu->LVInfo1 : NULL, sizeof(iuvdiu->LVInfo1));
	print_dstring(&disc, "organization", iuvdiu ? &iuvdiu->LVICharset : NULL, iuvdiu ? iuvdiu->LVInfo2 : NULL, sizeof(iuvdiu->LVInfo2));
	print_dstring(&disc, "contact", iuvdiu ? &iuvdiu->LVICharset : NULL, iuvdiu ? iuvdiu->LVInfo3 : NULL, sizeof(iuvdiu->LVInfo3));
	print_astring(&disc, "appid", pvd ? pvd->appIdent.ident : NULL, sizeof(pvd->appIdent.ident));
	print_astring(&disc, "impid", pvd ? pvd->impIdent.ident : NULL, sizeof(pvd->impIdent.ident));
	printf("winserialnum=0x%08"PRIx32"\n", serial_num);
	printf("blocksize=%"PRIu32"\n", disc.blocksize);
	printf("blocks=%"PRIu32"\n", disc.blocks);
	printf("usedblocks=%"PRIu32"\n", used_blocks);
	printf("freeblocks=%"PRIu32"\n", disc.free_space_blocks);
	printf("behindblocks=%"PRIu32"\n", behind_blocks);
	printf("numfiles=%"PRIu32"\n", disc.num_files);
	printf("numdirs=%"PRIu32"\n", disc.num_dirs);
	printf("udfrev=%"PRIx16".%02"PRIx16"\n", disc.udf_rev >> 8, disc.udf_rev & 0xFF);
	printf("udfwriterev=%"PRIx16".%02"PRIx16"\n", disc.udf_write_rev >> 8, disc.udf_write_rev & 0xFF);

	if (disc.start_block && disc.start_block != (uint32_t)-1)
		printf("startblock=%"PRIu32"\n", disc.start_block);
	if (disc.last_block && disc.last_block != disc.blocks-1)
		printf("lastblock=%"PRIu32"\n", disc.last_block);

	if (disc.vat_block)
		printf("vatblock=%"PRIu32"\n", disc.vat_block);

	if (disc.udf_lvid)
	{
		switch (le32_to_cpu(disc.udf_lvid->integrityType))
		{
			case LVID_INTEGRITY_TYPE_OPEN:
				printf("integrity=opened\n");
				break;
			case LVID_INTEGRITY_TYPE_CLOSE:
				printf("integrity=closed\n");
				break;
			default:
				printf("integrity=unknown\n");
				break;
		}
	}
	else
		printf("integrity=unknown\n");

	if (pd)
	{
		switch (le32_to_cpu(pd->accessType))
		{
			case PD_ACCESS_TYPE_OVERWRITABLE:
				printf("accesstype=overwritable\n");
				break;
			case PD_ACCESS_TYPE_REWRITABLE:
				printf("accesstype=rewritable\n");
				break;
			case PD_ACCESS_TYPE_WRITE_ONCE:
				printf("accesstype=writeonce\n");
				break;
			case PD_ACCESS_TYPE_READ_ONLY:
				printf("accesstype=readonly\n");
				break;
			case PD_ACCESS_TYPE_NONE:
				printf("accesstype=pseudo-overwritable\n");
				break;
			default:
				printf("accesstype=unknown\n");
				break;
		}
	}
	else
		printf("accesstype=unknown\n");

	soft_write_protect = hard_write_protect = 0;

	if (disc.udf_fsd)
	{
		dis = (struct domainIdentSuffix *)disc.udf_fsd->domainIdent.identSuffix;
		if (dis->domainFlags & DOMAIN_FLAGS_HARD_WRITE_PROTECT)
			soft_write_protect = hard_write_protect = 1;
		else if (dis->domainFlags & DOMAIN_FLAGS_SOFT_WRITE_PROTECT)
			soft_write_protect = 1;
	}

	if (lvd)
	{
		dis = (struct domainIdentSuffix *)lvd->domainIdent.identSuffix;
		if (dis->domainFlags & DOMAIN_FLAGS_HARD_WRITE_PROTECT)
			soft_write_protect = hard_write_protect = 1;
		else if (dis->domainFlags & DOMAIN_FLAGS_SOFT_WRITE_PROTECT)
			soft_write_protect = 1;
	}

	printf("softwriteprotect=%s\n", soft_write_protect ? "yes" : "no");
	printf("hardwriteprotect=%s\n", hard_write_protect ? "yes" : "no");

	dump_space(&disc);

	return 0;
}
