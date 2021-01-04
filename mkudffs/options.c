/*
 * options.c
 *
 * Copyright (c) 2001-2002  Ben Fennema
 * Copyright (c) 2014-2021  Pali Rohár <pali.rohar@gmail.com>
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
 * mkudffs option handling functions
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

#include "mkudffs.h"
#include "defaults.h"
#include "options.h"

static struct option long_options[] = {
	{ "help", no_argument, NULL, OPT_HELP },
	{ "label", required_argument, NULL, OPT_LABEL },
	{ "uuid", required_argument, NULL, OPT_UUID },
	{ "blocksize", required_argument, NULL, OPT_BLK_SIZE },
	{ "udfrev", required_argument, NULL, OPT_UDF_REV },
	{ "lvid", required_argument, NULL, OPT_LVID },
	{ "vid", required_argument, NULL, OPT_VID },
	{ "vsid", required_argument, NULL, OPT_VSID },
	{ "fsid", required_argument, NULL, OPT_FSID },
	{ "fullvsid", required_argument, NULL, OPT_FULLVSID },
	{ "owner", required_argument, NULL, OPT_OWNER },
	{ "organization", required_argument, NULL, OPT_ORG },
	{ "contact", required_argument, NULL, OPT_CONTACT },
	{ "uid", required_argument, NULL, OPT_UID },
	{ "gid", required_argument, NULL, OPT_GID },
	{ "mode", required_argument, NULL, OPT_MODE },
	{ "bootarea", required_argument, NULL, OPT_BOOTAREA },
	{ "strategy", required_argument, NULL, OPT_STRATEGY },
	{ "spartable", optional_argument, NULL, OPT_SPARTABLE },
	{ "sparspace", required_argument, NULL, OPT_SPARSPACE },
	{ "packetlen", required_argument, NULL, OPT_PACKETLEN },
	{ "media-type", required_argument, NULL, OPT_MEDIA_TYPE },
	{ "space", required_argument, NULL, OPT_SPACE },
	{ "ad", required_argument, NULL, OPT_AD },
	{ "vat", no_argument, NULL, OPT_VAT },
	{ "noefe", no_argument, NULL, OPT_NO_EFE },
	{ "locale", no_argument, NULL, OPT_LOCALE },
	{ "u8", no_argument, NULL, OPT_UNICODE8 },
	{ "u16", no_argument, NULL, OPT_UNICODE16 },
	{ "utf8", no_argument, NULL, OPT_UTF8 },
	{ "startblock", required_argument, NULL, OPT_START_BLOCK },
	{ "minblocks", required_argument, NULL, OPT_MIN_BLOCKS },
	{ "closed", no_argument, NULL, OPT_CLOSED },
	{ "new-file", no_argument, NULL, OPT_NEW_FILE },
	{ "no-write", no_argument, NULL, OPT_NO_WRITE },
	{ "read-only", no_argument, NULL, OPT_READ_ONLY },
	{ 0, 0, NULL, 0 },
};

void usage(void)
{
	fprintf(stderr, "mkudffs from " PACKAGE_NAME " " PACKAGE_VERSION "\n"
		"Usage:\n"
		"\tmkudffs [options] device [blocks-count]\n"
		"Options:\n"
		"\t--help, -h         Display this help\n"
		"\t--label=, -l       UDF label, synonym for both --lvid and --vid (default: LinuxUDF)\n"
		"\t--uuid=, -u        UDF uuid, first 16 characters of Volume set identifier (default: random)\n"
		"\t--blocksize=, -b   Size of blocks in bytes (512, 1024, 2048, 4096, 8192, 16384, 32768; default: detect)\n"
		"\t--media-type=, -m  Media type (hd, dvd, dvdram, dvdrw, dvdr, worm, mo, cdrw, cdr, cd, bdr; default: detect)\n"
		"\t--udfrev=, -r      UDF revision (1.01, 1.02, 1.50, 2.00, 2.01, 2.50, 2.60; default: 2.01)\n"
		"\t--no-write, -n     Not really, do not write to device, just simulate\n"
		"\t--new-file         Create new image file, fail if already exists\n"
		"\t--lvid=            Logical Volume Identifier (default: LinuxUDF)\n"
		"\t--vid=             Volume Identifier (default: LinuxUDF)\n"
		"\t--vsid=            17.-127. character of Volume Set Identifier (default: LinuxUDF)\n"
		"\t--fsid=            File Set Identifier (default: LinuxUDF)\n"
		"\t--fullvsid=        Full Volume Set Identifier, overwrite --uuid and --vsid\n"
		"\t--owner=           Owner name, person creating the medium or filesystem (default: empty)\n"
		"\t--organization=    Organization name responsible for creating the medium or filesystem (default: empty)\n"
		"\t--contact=         Contact information for the medium or filesystem (default: empty)\n"
		"\t--uid=             Uid of the root directory (default: 0)\n"
		"\t--gid=             Gid of the root directory (default: 0)\n"
		"\t--mode=            Permissions (octal mode bits) of the root directory (default: 0755)\n"
		"\t--read-only        Set UDF disk to read-only mode\n"
		"\t--bootarea=        UDF boot area (preserve, erase, mbr; default: based on media type)\n"
		"\t--strategy=        Allocation strategy to use (4, 4096; default: based on media type)\n"
		"\t--spartable        Use Sparing Table (default: based on media type) and set its count (1 - 4; default: 2)\n"
		"\t--sparspace=       Number of entries in Sparing Table (default: 1024, but based on media type)\n"
		"\t--packetlen=       Packet length in number of blocks used for alignment (default: based on media type)\n"
		"\t--vat              Use Virtual Allocation Table (default: based on media type)\n"
		"\t--startblock=      Block location where the UDF filesystem starts (default: 0)\n"
		"\t--minblocks=       Minimal number of blocks to write on disc with VAT (default: based on media type)\n"
		"\t--closed           Close disc with Virtual Allocation Table (default: do not close)\n"
		"\t--space=           Space (freedbitmap, freedtable, unallocbitmap, unalloctable; default: unallocbitmap)\n"
		"\t--ad=              Allocation descriptor (inicb, short, long; default: inicb)\n"
		"\t--noefe            Don't Use Extended File Entries (default: use for UDF revision >= 2.00)\n"
		"\t--locale           String options are encoded according to current locale (default)\n"
		"\t--u8               String options are encoded in Latin1\n"
		"\t--u16              String options are encoded in UTF-16BE\n"
		"\t--utf8             String options are encoded in UTF-8\n"
	);
	exit(1);
}

void parse_args(int argc, char *argv[], struct udf_disc *disc, char **device, int *create_new_file, int *blocksize, int *media_ptr)
{
	int retval;
	int i;
	struct domainIdentSuffix *dis;
	int media = MEDIA_TYPE_NONE;
	int read_only = 0;
	int use_vat = 0;
	int use_sparable = 0;
	int closed_vat = 0;
	int strategy = 0;
	int no_efe = 0;
	uint16_t rev = 0;
	uint32_t spartable = 2;
	uint32_t sparspace = 0;
	uint16_t packetlen = 0;
	uint16_t sparable_packetlen = 0;
	int failed;

	while ((retval = getopt_long(argc, argv, "l:u:b:m:r:nh", long_options, NULL)) != EOF)
	{
		switch (retval)
		{
			case OPT_HELP:
			case 'h':
				usage();
				break;
			case OPT_BLK_SIZE:
			case 'b':
				disc->blocksize = strtou32(optarg, 0, &failed);
				if (failed || disc->blocksize < 512 || disc->blocksize > 32768 || (disc->blocksize & (disc->blocksize - 1)))
				{
					fprintf(stderr, "%s: Error: Invalid value for option --blocksize\n", appname);
					exit(1);
				}
				disc->udf_lvd[0]->logicalBlockSize = cpu_to_le32(disc->blocksize);
				*blocksize = disc->blocksize;
				break;
			case OPT_UDF_REV:
			case 'r':
			{
				unsigned char maj = 0;
				unsigned char min = 0;
				int len = 0;
				if (sscanf(optarg, "%*[0-9].%*[0-9]%n", &len) >= 0 && !optarg[len] && sscanf(optarg, "%hhx.%hhx%n", &maj, &min, &len) >= 2 && !optarg[len])
				{
					rev = (maj << 8) | min;
				}
				else
				{
					rev = strtou16(optarg, 16, &failed);
					if (failed)
						rev = 0;
				}
				if (!rev || udf_set_version(disc, rev))
				{
					fprintf(stderr, "%s: Error: Invalid value for option --udfrev\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_NO_WRITE:
			case 'n':
			{
				disc->flags |= FLAG_NO_WRITE;
				break;
			}
			case OPT_NO_EFE:
			{
				no_efe = 1;
				break;
			}
			case OPT_NEW_FILE:
			{
				*create_new_file = 1;
				break;
			}
			case OPT_READ_ONLY:
			{
				read_only = 1;
				break;
			}
			case OPT_UNICODE8:
			{
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE8;
				if (strcmp(argv[1], "--u8") != 0)
				{
					fprintf(stderr, "%s: Error: Option --u8 must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_UNICODE16:
			{
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE16;
				if (strcmp(argv[1], "--u16") != 0)
				{
					fprintf(stderr, "%s: Error: Option --u16 must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_UTF8:
			{
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UTF8;
				if (strcmp(argv[1], "--utf8") != 0)
				{
					fprintf(stderr, "%s: Error: Option --utf8 must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_LOCALE:
			{
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_LOCALE;
				if (strcmp(argv[1], "--locale") != 0)
				{
					fprintf(stderr, "%s: Error: Option --locale must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_START_BLOCK:
			{
				disc->start_block = strtou32(optarg, 0, &failed);
				if (failed)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --startblock\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_MIN_BLOCKS:
			{
				/* At this time disc->last_block contains --minblock value */
				disc->last_block = strtou32(optarg, 0, &failed);
				if (failed)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --minblocks\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_CLOSED:
			{
				closed_vat = 1;
				break;
			}
			case OPT_VAT:
			{
				use_vat = 1;
				break;
			}
			case OPT_LVID:
			case OPT_VID:
			case OPT_LABEL:
			case 'l':
			{
				if (retval != OPT_VID)
				{
					struct impUseVolDescImpUse *iuvdiu;
					if (encode_string(disc, disc->udf_lvd[0]->logicalVolIdent, optarg, 128) == (size_t)-1)
					{
						fprintf(stderr, "%s: Error: Option %s is too long\n", appname, (retval == OPT_LVID) ? "--lvid" : "--label");
						exit(1);
					}
					iuvdiu = (struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse;
					memcpy(iuvdiu->logicalVolIdent, disc->udf_lvd[0]->logicalVolIdent, 128);
					memcpy(disc->udf_fsd->logicalVolIdent, disc->udf_lvd[0]->logicalVolIdent, 128);
				}
				if (retval != OPT_LVID)
				{
					if (encode_string(disc, disc->udf_pvd[0]->volIdent, optarg, 32) == (size_t)-1)
					{
						if (retval == OPT_VID)
						{
							fprintf(stderr, "%s: Error: Option --vid is too long\n", appname);
							exit(1);
						}
						/* This code was not triggered by --vid option, do not throw error but rather store truncated --lvid */
						memcpy(disc->udf_pvd[0]->volIdent, disc->udf_lvd[0]->logicalVolIdent, 32);
						disc->udf_pvd[0]->volIdent[31] = 31;
					}
				}
				break;
			}
			case OPT_VSID:
			{
				dstring ts[128];
				size_t len = encode_string(disc, ts, optarg, 128);
				if (len == (size_t)-1 || len > 127-16 || (ts[0] == 16 && len > 127-16*2) || (ts[0] == 8 && disc->udf_pvd[0]->volSetIdent[0] == 16 && len > (127-16*2-1)/2+1)) // 2*(len-1)+1 > 127-16*2
				{
					fprintf(stderr, "%s: Error: Option --vsid is too long\n", appname);
					exit(1);
				}
				if (ts[0] == disc->udf_pvd[0]->volSetIdent[0])
				{
					for (i = 0; i < 16; ++i)
					{
						if (ts[0] == 8)
						{
							if (!disc->udf_pvd[0]->volSetIdent[i+1])
								disc->udf_pvd[0]->volSetIdent[i+1] = '0';
						}
						else if (ts[0] == 16)
						{
							if (!disc->udf_pvd[0]->volSetIdent[(2*i)+1] && !disc->udf_pvd[0]->volSetIdent[(2*i)+2])
								disc->udf_pvd[0]->volSetIdent[(2*i)+2] = '0';
						}
					}
				}
				else if(ts[0] == 16)
				{
					disc->udf_pvd[0]->volSetIdent[0] = 16;
					for (i = 17; i > 0; --i)
					{
						disc->udf_pvd[0]->volSetIdent[(2*(i-1))+1] = 0;
						disc->udf_pvd[0]->volSetIdent[(2*(i-1))+2] = disc->udf_pvd[0]->volSetIdent[i];
						if (!disc->udf_pvd[0]->volSetIdent[(2*(i-1))+2])
							disc->udf_pvd[0]->volSetIdent[(2*(i-1))+2] = '0';
					}
				}
				else if(ts[0] == 8 && disc->udf_pvd[0]->volSetIdent[0] == 16)
				{
					ts[0] = 16;
					for (i = len; i > 0; --i)
					{
						ts[(2*(i-1))+2] = ts[i];
						ts[(2*(i-1))+1] = 0;
					}
					len = (len-1)*2+1;
				}
				if (len)
					memcpy(&disc->udf_pvd[0]->volSetIdent[1+16*(ts[0]/8)], &ts[1], len-1+(ts[0]/8));
				disc->udf_pvd[0]->volSetIdent[127] = 16*(ts[0]/8)+len;
				for (i = disc->udf_pvd[0]->volSetIdent[127]; i < 127; ++i)
					disc->udf_pvd[0]->volSetIdent[i] = 0;
				break;
			}
			case OPT_UUID:
			case 'u':
			{
				if (strlen(optarg) != 16)
				{
					fprintf(stderr, "%s: Error: Option --uuid is not 16 bytes long\n", appname);
					exit(1);
				}
				for (i = 0; i < 16; ++i)
				{
					if (!isxdigit(optarg[i]) || (!isdigit(optarg[i]) && !islower(optarg[i])))
					{
						fprintf(stderr, "%s: Error: Option --uuid is not in lowercase hexadecimal digit format\n", appname);
						exit(1);
					}
				}
				if (disc->udf_pvd[0]->volSetIdent[0] == 8)
				{
					memcpy(&disc->udf_pvd[0]->volSetIdent[1], optarg, 16);
					if (disc->udf_pvd[0]->volSetIdent[127] < 17)
					{
						disc->udf_pvd[0]->volSetIdent[17] = 0;
						disc->udf_pvd[0]->volSetIdent[127] = 17;
					}
				}
				else if (disc->udf_pvd[0]->volSetIdent[0] == 16)
				{
					for (i = 0; i < 16; ++i)
					{
						disc->udf_pvd[0]->volSetIdent[2*i+1] = 0;
						disc->udf_pvd[0]->volSetIdent[2*i+2] = optarg[i];
					}
					if (disc->udf_pvd[0]->volSetIdent[127] < 2*16+1)
					{
						disc->udf_pvd[0]->volSetIdent[2*16+1] = 0;
						disc->udf_pvd[0]->volSetIdent[2*16+2] = 0;
						disc->udf_pvd[0]->volSetIdent[127] = 2*16+1;
					}
				}
				break;
			}
			case OPT_FULLVSID:
			{
				if (encode_string(disc, disc->udf_pvd[0]->volSetIdent, optarg, 128) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --fullvsid is too long\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_FSID:
			{
				if (encode_string(disc, disc->udf_fsd->fileSetIdent, optarg, 32) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --fsid is too long\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_OWNER:
			{
				struct impUseVolDescImpUse *iuvdiu = (struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse;
				if (encode_string(disc, iuvdiu->LVInfo1, optarg, sizeof(iuvdiu->LVInfo1)) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --owner is too long\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_ORG:
			{
				struct impUseVolDescImpUse *iuvdiu = (struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse;
				if (encode_string(disc, iuvdiu->LVInfo2, optarg, sizeof(iuvdiu->LVInfo2)) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --organization is too long\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_CONTACT:
			{
				struct impUseVolDescImpUse *iuvdiu = (struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse;
				if (encode_string(disc, iuvdiu->LVInfo3, optarg, sizeof(iuvdiu->LVInfo3)) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --contact is too long\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_UID:
			{
				if (strcmp(optarg, "-1") == 0)
				{
					disc->uid = UINT32_MAX;
				}
				else
				{
					disc->uid = strtou32(optarg, 0, &failed);
					if (failed)
					{
						fprintf(stderr, "%s: Error: Invalid value for option --uid\n", appname);
						exit(1);
					}
				}
				break;
			}
			case OPT_GID:
			{
				if (strcmp(optarg, "-1") == 0)
				{
					disc->gid = UINT32_MAX;
				}
				else
				{
					disc->gid = strtou32(optarg, 0, &failed);
					if (failed)
					{
						fprintf(stderr, "%s: Error: Invalid value for option --gid\n", appname);
						exit(1);
					}
				}
				break;
			}
			case OPT_MODE:
			{
				uint16_t mode = strtou16(optarg, 8, &failed);
				if (failed || (mode & ~(S_IRWXU|S_IRWXG|S_IRWXO)))
				{
					fprintf(stderr, "%s: Error: Invalid value for option --mode\n", appname);
					exit(1);
				}
				disc->mode = mode;
				break;
			}
			case OPT_BOOTAREA:
			{
				disc->flags &= ~FLAG_BOOTAREA_MASK;
				if (!strcmp(optarg, "preserve"))
					disc->flags |= FLAG_BOOTAREA_PRESERVE;
				else if (!strcmp(optarg, "erase"))
					disc->flags |= FLAG_BOOTAREA_ERASE;
				else if (!strcmp(optarg, "mbr"))
					disc->flags |= FLAG_BOOTAREA_MBR;
				else if (!strncmp(optarg, "mbr:", 4))
				{
					disc->flags |= FLAG_BOOTAREA_MBR;
					disc->blkssz = strtou16(optarg+4, 0, &failed);
					if (failed || disc->blkssz < 512 || disc->blkssz > 32768 || (disc->blkssz & (disc->blkssz - 1)))
					{
						fprintf(stderr, "%s: Error: Invalid value for option --bootarea=mbr:\n", appname);
						exit(1);
					}
				}
				else
				{
					fprintf(stderr, "%s: Error: Invalid value for option --bootarea\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_STRATEGY:
			{
				if (strcmp(optarg, "4096") == 0)
					strategy = 4096;
				else if (strcmp(optarg, "4") == 0)
					strategy = 4;
				else
				{
					fprintf(stderr, "%s: Error: Invalid value for option --strategy\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_SPARTABLE:
			{
				if (optarg)
				{
					spartable = strtou32(optarg, 0, &failed);
					if (failed || spartable == 0 || spartable > 4)
					{
						fprintf(stderr, "%s: Error: Invalid value for option --spartable\n", appname);
						exit(1);
					}
				}
				if (use_vat)
				{
					fprintf(stderr, "%s: Error: Cannot use Sparing Table when VAT is enabled\n", appname);
					exit(1);
				}
				use_sparable = 1;
				break;
			}
			case OPT_SPARSPACE:
			{
				if (!use_sparable)
				{
					fprintf(stderr, "%s: Error: Option --spartable must be specified before option --sparspace\n", appname);
					exit(1);
				}
				sparspace = strtou32(optarg, 0, &failed);
				if (failed)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --sparspace\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_PACKETLEN:
			{
				packetlen = strtou16(optarg, 0, &failed);
				if (failed || packetlen == 0)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --packetlen\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_MEDIA_TYPE:
			case 'm':
			{
				if (media != MEDIA_TYPE_NONE)
				{
					fprintf(stderr, "%s: Error: Option --media-type was specified more times\n", appname);
					exit(1);
				}
				if (rev)
				{
					fprintf(stderr, "%s: Error: Option --media-type must be specified before option --udfrev\n", appname);
					exit(1);
				}
				if (!strcmp(optarg, "hd"))
				{
					media = MEDIA_TYPE_HD;
				}
				else if (!strcmp(optarg, "dvd"))
				{
					media = MEDIA_TYPE_DVD;
				}
				else if (!strcmp(optarg, "dvdram"))
				{
					media = MEDIA_TYPE_DVDRAM;
				}
				else if (!strcmp(optarg, "dvdrw"))
				{
					media = MEDIA_TYPE_DVDRW;
					use_sparable = 1;
				}
				else if (!strcmp(optarg, "dvdr"))
				{
					media = MEDIA_TYPE_DVDR;
					use_vat = 1;
				}
				else if (!strcmp(optarg, "worm"))
				{
					media = MEDIA_TYPE_WORM;
					strategy = 4096;
				}
				else if (!strcmp(optarg, "mo"))
				{
					media = MEDIA_TYPE_MO;
					strategy = 4096;
				}
				else if (!strcmp(optarg, "cdrw"))
				{
					media = MEDIA_TYPE_CDRW;
					use_sparable = 1;
				}
				else if (!strcmp(optarg, "cdr"))
				{
					media = MEDIA_TYPE_CDR;
					use_vat = 1;
				}
				else if (!strcmp(optarg, "cd"))
				{
					media = MEDIA_TYPE_CD;
				}
				else if (!strcmp(optarg, "bdr"))
				{
					media = MEDIA_TYPE_BDR;
					use_vat = 1;
					udf_set_version(disc, 0x0250);
				}
				else
				{
					fprintf(stderr, "%s: Error: Invalid value for option --media-type\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_SPACE:
			{
				if (!strncmp(optarg, "freedbitmap", 11))
					disc->flags |= FLAG_FREED_BITMAP;
				else if (!strncmp(optarg, "freedtable", 10))
					disc->flags |= FLAG_FREED_TABLE;
				else if (!strncmp(optarg, "unallocbitmap", 13))
					disc->flags |= FLAG_UNALLOC_BITMAP;
				else if (!strncmp(optarg, "unalloctable", 12))
					disc->flags |= FLAG_UNALLOC_TABLE;
				else
				{
					fprintf(stderr, "%s: Error: Invalid value for option --space\n", appname);
					exit(1);
				}
				break;
			}
			case OPT_AD:
			{
				if (!strncmp(optarg, "inicb", 5))
				{
					default_fe.icbTag.flags = cpu_to_le16(ICBTAG_FLAG_AD_IN_ICB);
					default_efe.icbTag.flags = cpu_to_le16(ICBTAG_FLAG_AD_IN_ICB);
				}
				else if (!strncmp(optarg, "short", 5))
				{
					default_fe.icbTag.flags = cpu_to_le16(ICBTAG_FLAG_AD_SHORT);
					default_efe.icbTag.flags = cpu_to_le16(ICBTAG_FLAG_AD_SHORT);
				}
				else if (!strncmp(optarg, "long", 4))
				{
					default_fe.icbTag.flags = cpu_to_le16(ICBTAG_FLAG_AD_LONG);
					default_efe.icbTag.flags = cpu_to_le16(ICBTAG_FLAG_AD_LONG);
				}
				else
				{
					fprintf(stderr, "%s: Error: Invalid value for option --ad\n", appname);
					exit(1);
				}
				break;
			}
			default:
				exit(1);
		}
	}
	if (optind == argc)
		usage();
	*device = argv[optind];
	optind ++;
	if (optind < argc)
	{
		disc->blocks = strtou32(argv[optind++], 0, &failed);
		if (failed || disc->blocks == 0)
		{
			fprintf(stderr, "%s: Error: Invalid value for block-count\n", appname);
			exit(1);
		}
	}
	if (optind < argc)
		usage();

	/* Autodetect media type */
	if (media == MEDIA_TYPE_NONE)
	{
		int fd;
		long last;
		struct stat st;
		int mmc_profile;

		mmc_profile = -1;

		fd = open(*device, O_RDONLY);

		/* Check if device is optical disc
		 * pktcdvd.ko accepts only these ioctls:
		 * CDROMEJECT CDROMMULTISESSION CDROMREADTOCENTRY
		 * CDROM_LAST_WRITTEN CDROM_SEND_PACKET SCSI_IOCTL_SEND_COMMAND */
		if (fd >= 0 && fstat(fd, &st) == 0 && S_ISBLK(st.st_mode) && ioctl(fd, CDROM_LAST_WRITTEN, &last) == 0)
		{
			struct cdrom_generic_command cgc;
			struct request_sense sense;
			struct feature_header features;
			disc_information discinfo;

			memset(&cgc, 0, sizeof(cgc));
			memset(&sense, 0, sizeof(sense));
			memset(&features, 0, sizeof(features));
			cgc.cmd[0] = GPCMD_GET_CONFIGURATION;
			cgc.cmd[8] = sizeof(features);
			cgc.buffer = (unsigned char *)&features;
			cgc.buflen = sizeof(features);
			cgc.sense = &sense;
			cgc.data_direction = CGC_DATA_READ;
			cgc.quiet = 1;
			cgc.timeout = 500;

			if (ioctl(fd, CDROM_SEND_PACKET, &cgc) == 0)
			{
				mmc_profile = be16_to_cpu(features.curr_profile);
			}
			else
			{
				memset(&cgc, 0, sizeof(cgc));
				memset(&sense, 0, sizeof(sense));
				memset(&discinfo, 0, sizeof(discinfo));
				cgc.cmd[0] = GPCMD_READ_DISC_INFO;
				cgc.cmd[8] = sizeof(discinfo.disc_information_length);
				cgc.buffer = (unsigned char *)&discinfo.disc_information_length;
				cgc.buflen = sizeof(discinfo.disc_information_length);
				cgc.sense = &sense;
				cgc.data_direction = CGC_DATA_READ;
				cgc.quiet = 1;
				cgc.timeout = 500;

				if (ioctl(fd, CDROM_SEND_PACKET, &cgc) == 0)
				{
					/* Not all drives have the same disc_info length, so requeue
					 * packet with the length the drive tells us it can supply */
					cgc.buffer = (unsigned char *)&discinfo;
					cgc.buflen = be16_to_cpu(discinfo.disc_information_length) + sizeof(discinfo.disc_information_length);
					if (cgc.buflen > sizeof(discinfo))
						cgc.buflen = sizeof(discinfo);
					cgc.cmd[8] = cgc.buflen;

					if (ioctl(fd, CDROM_SEND_PACKET, &cgc) == 0)
					{
						if (discinfo.erasable) /* Erasable bit set = CD-RW */
							mmc_profile = 0x0A;
						else if (discinfo.disc_status == 2) /* Complete disc status = CD-ROM */
							mmc_profile = 0x08;
						else /* All other disc statuses means that medium is recordable = CD-R */
							mmc_profile = 0x09;
					}
				}
				else
				{
					mmc_profile = 0x00; /* Unknown optical disc */
				}
			}
		}

		if (fd >= 0)
			close(fd);

		switch (mmc_profile)
		{
			case 0x03: /* Magneto-Optical Erasable */
			case 0x05: /* Advance Storage Magneto-Optical */
				printf("Detected Magneto-Optical disc\n");
				media = MEDIA_TYPE_MO;
				break;

			case 0x0A: /* CD-RW */
			case 0x22: /* DDCD-RW */
				printf("Detected CD-RW optical disc\n");
				media = MEDIA_TYPE_CDRW;
				break;

			case 0x12: /* DVD-RAM */
				printf("Detected DVD-RAM optical disc\n");
				media = MEDIA_TYPE_DVDRAM;
				break;

			case 0x13: /* DVD-RW Restricted Overwrite */
			case 0x14: /* DVD-RW Sequential Recording */
			case 0x17: /* DVD-RW DL */
			case 0x1A: /* DVD+RW */
			case 0x2A: /* DVD+RW DL */
				printf("Detected DVD-RW optical disc\n");
				media = MEDIA_TYPE_DVDRW;
				break;

			case 0x08: /* CD-ROM */
			case 0x10: /* DVD-ROM */
			case 0x20: /* DDCD-ROM */
			case 0x40: /* BD-ROM */
			case 0x50: /* HDDVD-ROM */
				fprintf(stderr, "%s: Error: Detected read-only optical disc, use --media-type option to specify media type\n", appname);
				exit(1);

			case 0x04: /* Magneto-Optical Write Once */
			case 0x09: /* CD-R */
			case 0x11: /* DVD-R */
			case 0x15: /* DVD-R DL Sequential Recording */
			case 0x16: /* DVD-R DL Jump Recording */
			case 0x18: /* DVD-R Download Disc Recording */
			case 0x1B: /* DVD+R */
			case 0x21: /* DDCD-R */
			case 0x2B: /* DVD+R DL */
			case 0x41: /* BD-R Sequential Recording Mode */
			case 0x42: /* BD-R Random Recording Mode */
			case 0x51: /* HDDVD-R */
			case 0x58: /* HDDVD-R DL */
				fprintf(stderr, "%s: Error: Detected write-once optical disc, use --media-type option to specify media type\n", appname);
				exit(1);

			case 0x43: /* BD-RE */
			case 0x52: /* HDDVD-RAM */
			case 0x53: /* HDDVD-RW */
			case 0x5A: /* HDDVD-RW DL */
				fprintf(stderr, "%s: Error: Detected unsupported optical disc, use --media-type option to specify media type\n", appname);
				exit(1);

			case 0x01: /* Non-removable Re-writable disk, Random Writable */
			case 0x02: /* Removable Re-writable disk, Random Writable */
				printf("Detected optical disc with random access write support\n");
				media = MEDIA_TYPE_HD; /* hd media type is the best match for random access write support */
				break;

			default:
				fprintf(stderr, "%s: Error: Detected unknown optical disc, use --media-type option to specify media type\n", appname);
				exit(1);

			case -1: /* Not an optical disc */
				media = MEDIA_TYPE_HD;
				break;
		}
	}

	switch (media)
	{
		case MEDIA_TYPE_HD:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
			break;

		case MEDIA_TYPE_DVD:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_READ_ONLY);
			break;

		case MEDIA_TYPE_DVDRAM:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
			break;

		case MEDIA_TYPE_DVDRW:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
			use_sparable = 1;
			break;

		case MEDIA_TYPE_DVDR:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
			use_vat = 1;
			break;

		case MEDIA_TYPE_WORM:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
			disc->flags |= FLAG_BLANK_TERMINAL;
			if (!strategy)
				strategy = 4096;
			break;

		case MEDIA_TYPE_MO:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);
			disc->flags |= FLAG_BLANK_TERMINAL;
			if (!strategy)
				strategy = 4096;
			break;

		case MEDIA_TYPE_CDRW:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);
			use_sparable = 1;
			break;

		case MEDIA_TYPE_CDR:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
			use_vat = 1;
			/* At this time disc->last_block contains --minblock value */
			if (!disc->last_block)
				disc->last_block = 300 * 2048 / disc->blocksize; /* On optical TAO discs one track has minimal size of 300 sectors which are 2048 bytes long */
			break;

		case MEDIA_TYPE_CD:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_READ_ONLY);
			break;

		case MEDIA_TYPE_BDR:
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
			use_vat = 1;
			if (!rev)
				udf_set_version(disc, 0x0250);
			break;

		default:
			fprintf(stderr, "%s: Error: Unhandled media type\n", appname);
			exit(1);
	}

	if (disc->udf_rev < 0x0150 && (media == MEDIA_TYPE_DVDRW || media == MEDIA_TYPE_DVDR || media == MEDIA_TYPE_CDRW || media == MEDIA_TYPE_CDR))
	{
		fprintf(stderr, "%s: Error: At least UDF revision 1.50 is needed for CD-R/CD-RW/DVD-R/DVD-RW discs\n", appname);
		exit(1);
	}
	if (disc->udf_rev < 0x0250 && media == MEDIA_TYPE_BDR)
	{
		fprintf(stderr, "%s: Error: At least UDF revision 2.50 is needed for BD-R discs\n", appname);
		exit(1);
	}

	if (no_efe)
		disc->flags &= ~FLAG_EFE;

	if (strategy == 4096)
		disc->flags |= FLAG_STRATEGY4096;

	if (read_only)
	{
		if (le32_to_cpu(disc->udf_pd[0]->accessType) == PD_ACCESS_TYPE_OVERWRITABLE)
			disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_READ_ONLY);

		dis = (struct domainIdentSuffix *)disc->udf_fsd->domainIdent.identSuffix;
		dis->domainFlags |= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;
		dis = (struct domainIdentSuffix *)disc->udf_lvd[0]->domainIdent.identSuffix;
		dis->domainFlags |= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;

		dis = (struct domainIdentSuffix *)default_fsd.domainIdent.identSuffix;
		dis->domainFlags |= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;
		dis = (struct domainIdentSuffix *)default_lvd.domainIdent.identSuffix;
		dis->domainFlags |= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;
	}

	if (disc->start_block && (disc->flags & FLAG_BOOTAREA_MBR))
	{
		fprintf(stderr, "%s: Error: Option --startblock cannot be used together with option --bootarea=mbr\n", appname);
		exit(1);
	}

	if (closed_vat && !use_vat)
	{
		fprintf(stderr, "%s: Error: Option --closed cannot be used without --vat or --media-type cdr/dvdr/bdr\n", appname);
		exit(1);
	}

	if (disc->last_block && !use_vat)
	{
		fprintf(stderr, "%s: Error: Option --minblocks cannot be used without --vat or --media-type cdr/dvdr/bdr\n", appname);
		exit(1);
	}

	if (use_vat)
	{
		/* disc->last_block contains --minblocks value, convert it to real minimal last block value */
		if (disc->last_block)
			disc->last_block = disc->start_block + disc->last_block - 1;
		if (disc->udf_rev < 0x0150)
		{
			fprintf(stderr, "%s: Error: At least UDF revision 1.50 is needed for VAT\n", appname);
			exit(1);
		}
		disc->flags |= FLAG_VAT;
		if (closed_vat)
			disc->flags |= FLAG_CLOSED;
		add_type1_partition(disc, 0);
		add_type2_virtual_partition(disc, 0);
	}
	else if (use_sparable)
	{
		if (disc->udf_rev < 0x0150)
		{
			fprintf(stderr, "%s: Error: At least UDF revision 1.50 is needed for Sparing Table\n", appname);
			exit(1);
		}
		if (packetlen)
			sparable_packetlen = packetlen;
		else if (disc->udf_rev <= 0x0200)
			/* UDF 2.00 Errata, DCN-5163, Packet Length for Sparable Partition:
			 * For UDF 1.50 and 2.00 should be set to fixed value 32. */
			sparable_packetlen = 32;
		else
			sparable_packetlen = default_sizing[default_media[media]][STABLE_SIZE].align;
		add_type2_sparable_partition(disc, 0, spartable, sparable_packetlen);
	}
	else
	{
		add_type1_partition(disc, 0);
	}

	/* TODO: UDF 2.50+ require for non-VAT disks Metadata partition which mkudffs cannot create yet */
	if (disc->udf_rev >= 0x0250 && !(disc->flags & FLAG_VAT))
	{
		fprintf(stderr, "%s: Error: UDF revision above 2.01 is not currently supported for specified media type\n", appname);
		exit(1);
	}

	if ((disc->flags & FLAG_VAT) && (disc->flags & FLAG_SPACE))
	{
		fprintf(stderr, "%s: Error: Option --space cannot be used for VAT\n", appname);
		exit(1);
	}

	if (!(disc->flags & FLAG_VAT) && !(disc->flags & FLAG_SPACE))
		disc->flags |= FLAG_UNALLOC_BITMAP;

	if ((disc->flags & FLAG_STRATEGY4096) && (disc->flags & FLAG_VAT))
	{
		fprintf(stderr, "%s: Error: Cannot use strategy type 4096 for VAT\n", appname);
		exit(1);
	}

	for (i=0; i<UDF_ALLOC_TYPE_SIZE; i++)
	{
		if (disc->sizing[i].denomSize == 0)
			disc->sizing[i] = default_sizing[default_media[media]][i];
		if (packetlen)
			disc->sizing[i].align = packetlen;
	}

	if (use_sparable)
	{
		if (sparspace)
		{
			disc->sizing[SSPACE_SIZE].minSize = sparspace;
			disc->sizing[STABLE_SIZE].minSize = 0; // recalculation is implemented in split_space()
		}
		if (disc->sizing[SSPACE_SIZE].minSize == 0)
			disc->sizing[SSPACE_SIZE].minSize = 1024;
	}

	*media_ptr = media;
}
