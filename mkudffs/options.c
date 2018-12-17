/*
 * options.c
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
#include <sys/stat.h>

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
	{ "closed", no_argument, NULL, OPT_CLOSED },
	{ "new-file", no_argument, NULL, OPT_NEW_FILE },
	{ "no-write", no_argument, NULL, OPT_NO_WRITE },
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
		"\t--media-type=, -m  Media type (hd, dvd, dvdram, dvdrw, dvdr, worm, mo, cdrw, cdr, cd, bdr; default: hd)\n"
		"\t--udfrev=, -r      UDF revision (1.01, 1.02, 1.50, 2.00, 2.01, 2.50, 2.60; default: 2.01)\n"
		"\t--no-write, -n     Not really, do not write to device, just simulate\n"
		"\t--new-file         Create new image file, fail if already exists\n"
		"\t--lvid=            Logical Volume Identifier (default: LinuxUDF)\n"
		"\t--vid=             Volume Identifier (default: LinuxUDF)\n"
		"\t--vsid=            17.-127. character of Volume Set Identifier (default: LinuxUDF)\n"
		"\t--fsid=            File Set Identifier (default: LinuxUDF)\n"
		"\t--fullvsid=        Full Volume Set Identifier, overwrite --uuid and --vsid\n"
		"\t--uid=             Uid of the root directory (default: 0)\n"
		"\t--gid=             Gid of the root directory (default: 0)\n"
		"\t--mode=            Permissions (octal mode bits) of the root directory (default: 0755)\n"
		"\t--bootarea=        UDF boot area (preserve, erase, mbr; default: based on media type)\n"
		"\t--strategy=        Allocation strategy to use (4, 4096; default: based on media type)\n"
		"\t--spartable        Use Sparing Table (default: based on media type) and set its count (1 - 4; default: 2)\n"
		"\t--sparspace=       Number of entries in Sparing Table (default: 1024, but based on media type)\n"
		"\t--packetlen=       Packet length in number of blocks used for alignment (default: based on media type)\n"
		"\t--vat              Use Virtual Allocation Table (default: based on media type)\n"
		"\t--closed           Close disc with Virtual Allocation Table (default: do not close)\n"
		"\t--space=           Space (freedbitmap, freedtable, unallocbitmap, unalloctable; default: unallocbitmap)\n"
		"\t--ad=              Allocation descriptor (inicb, short, long; default: inicb)\n"
		"\t--noefe            Don't Use Extended File Entries (default: use for UDF revision >= 2.00)\n"
		"\t--locale           String options are encoded according to current locale (default)\n"
		"\t--u8               String options are encoded in 8-bit OSTA Compressed Unicode format\n"
		"\t--u16              String options are encoded in 16-bit OSTA Compressed Unicode format\n"
		"\t--utf8             String options are encoded in UTF-8\n"
	);
	exit(1);
}

void parse_args(int argc, char *argv[], struct udf_disc *disc, char **device, int *create_new_file, int *blocksize, int *media_ptr)
{
	int retval;
	int i;
	int media = MEDIA_TYPE_NONE;
	int use_sparable = 0;
	uint16_t rev = 0;
	uint32_t spartable = 2;
	uint32_t sparspace = 0;
	uint16_t packetlen = 0;
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
				if (rev < 0x0150 && (media == MEDIA_TYPE_DVDRW || media == MEDIA_TYPE_DVDR || media == MEDIA_TYPE_CDRW || media == MEDIA_TYPE_CDR))
				{
					fprintf(stderr, "%s: Error: At least UDF revision 1.50 is needed for CD-R/CD-RW/DVD-R/DVD-RW discs\n", appname);
					exit(1);
				}
				if (rev < 0x0250 && media == MEDIA_TYPE_BDR)
				{
					fprintf(stderr, "%s: Error: At least UDF revision 2.50 is needed for BD-R discs\n", appname);
					exit(1);
				}
				if (rev < 0x0150 && use_sparable)
				{
					fprintf(stderr, "%s: Error: At least UDF revision 1.50 is needed for Sparing Table\n", appname);
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
				disc->flags &= ~FLAG_EFE;
				break;
			}
			case OPT_NEW_FILE:
			{
				*create_new_file = 1;
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
			case OPT_CLOSED:
			{
				if (!(disc->flags & FLAG_VAT))
				{
					fprintf(stderr, "%s: Error: Option --vat must be specified before option --closed\n", appname);
					exit(1);
				}
				disc->flags |= FLAG_CLOSED;
				break;
			}
			case OPT_VAT:
			{
				disc->flags |= FLAG_VAT;
				disc->flags &= ~FLAG_CLOSED;
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
					disc->flags |= FLAG_STRATEGY4096;
				else if (strcmp(optarg, "4") == 0)
					disc->flags &= ~FLAG_STRATEGY4096;
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
				if (disc->flags & FLAG_VAT)
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
				if (packetlen)
				{
					fprintf(stderr, "%s: Error: Option --media-type must be specified before option --packetlen\n", appname);
					exit(1);
				}
				if (!strcmp(optarg, "hd"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
					media = MEDIA_TYPE_HD;
					packetlen = 1;
				}
				else if (!strcmp(optarg, "dvd"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_READ_ONLY);
					media = MEDIA_TYPE_DVD;
					packetlen = 16;
				}
				else if (!strcmp(optarg, "dvdram"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
					media = MEDIA_TYPE_DVDRAM;
					packetlen = 16;
				}
				else if (!strcmp(optarg, "dvdrw"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
					media = MEDIA_TYPE_DVDRW;
					use_sparable = 1;
					packetlen = 16;
				}
				else if (!strcmp(optarg, "dvdr"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
					media = MEDIA_TYPE_DVDR;
					disc->flags |= FLAG_VAT;
					disc->flags &= ~FLAG_CLOSED;
					packetlen = 16;
				}
				else if (!strcmp(optarg, "worm"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
					media = MEDIA_TYPE_WORM;
					disc->flags |= (FLAG_STRATEGY4096 | FLAG_BLANK_TERMINAL);
					packetlen = 1;
				}
				else if (!strcmp(optarg, "mo"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);
					media = MEDIA_TYPE_MO;
					disc->flags |= (FLAG_STRATEGY4096 | FLAG_BLANK_TERMINAL);
					packetlen = 1;
				}
				else if (!strcmp(optarg, "cdrw"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);
					media = MEDIA_TYPE_CDRW;
					use_sparable = 1;
					packetlen = 32;
				}
				else if (!strcmp(optarg, "cdr"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
					media = MEDIA_TYPE_CDR;
					disc->flags |= FLAG_VAT | FLAG_MIN_300_BLOCKS;
					disc->flags &= ~FLAG_CLOSED;
					packetlen = 32;
				}
				else if (!strcmp(optarg, "cd"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_READ_ONLY);
					media = MEDIA_TYPE_CD;
					packetlen = 32;
				}
				else if (!strcmp(optarg, "bdr"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
					media = MEDIA_TYPE_BDR;
					disc->flags |= FLAG_VAT;
					disc->flags &= ~FLAG_CLOSED;
					udf_set_version(disc, 0x0250);
					packetlen = 32;
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
		if (failed)
		{
			fprintf(stderr, "%s: Error: Invalid value for block-count\n", appname);
			exit(1);
		}
	}
	if (optind < argc)
		usage();

	/* TODO: autodetection */
	if (media == MEDIA_TYPE_NONE)
	{
		media = MEDIA_TYPE_HD;
		packetlen = 1;
	}

	if (disc->flags & FLAG_VAT)
	{
		if (disc->udf_rev < 0x0150)
		{
			fprintf(stderr, "%s: Error: At least UDF revision 1.50 is needed for VAT\n", appname);
			exit(1);
		}
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
		add_type2_sparable_partition(disc, 0, spartable, packetlen);
	}
	else
	{
		add_type1_partition(disc, 0);
	}

	/* TODO: UDF 2.50+ require for non-VAT disks Metadata partition which mkudffs cannot create yet */
	if (rev > 0x0201 && !(disc->flags & FLAG_VAT))
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
		disc->sizing[i].align = packetlen;
		if (disc->sizing[i].denomSize == 0)
			disc->sizing[i] = default_sizing[default_media[media]][i];
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
