/*
 * options.c
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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <malloc.h>

#include "mkudffs.h"
#include "defaults.h"
#include "options.h"

struct option long_options[] = {
	{ "help", no_argument, NULL, OPT_HELP },
	{ "blocksize", required_argument, NULL, OPT_BLK_SIZE },
	{ "udfrev", required_argument, NULL, OPT_UDF_REV },
	{ "lvid", required_argument, NULL, OPT_LVID },
	{ "vid", required_argument, NULL, OPT_VID },
	{ "vsid", required_argument, NULL, OPT_VSID },
	{ "fsid", required_argument, NULL, OPT_FSID },
	{ "strategy", required_argument, NULL, OPT_STRATEGY },
	{ "spartable", required_argument, NULL, OPT_SPARTABLE },
	{ "packetlen", required_argument, NULL, OPT_PACKETLEN },
	{ "media-type", required_argument, NULL, OPT_MEDIA_TYPE },
	{ "space", required_argument, NULL, OPT_SPACE },
	{ "ad", required_argument, NULL, OPT_AD },
	{ "noefe", no_argument, NULL, OPT_NO_EFE },
	{ "u8", no_argument, NULL, OPT_UNICODE8 },
	{ "u16", no_argument, NULL, OPT_UNICODE16 },
	{ "utf8", no_argument, NULL, OPT_UTF8 },
	{ "bridge", no_argument, NULL, OPT_BRIDGE },
	{ "closed", no_argument, NULL, OPT_CLOSED },
	{ 0, 0, NULL, 0 },
};

void usage(void)
{
	fprintf(stderr, "mkudffs %s for UDF FS %s, %s\n"
		"Usage:\n"
		"\tmkudffs [options] device [blocks-count]\n"
		"Switches:\n"
		"\t--help\n"
		"\t--blocksize=, -b\n"
		"\t--udfrev=, -r\n"
		"\t--lvid=\n"
		"\t--vid=\n"
		"\t--vsid=\n"
		"\t--fsid=\n"
		"\t--strategy=\n"
		"\t--spartable=\n"
		"\t--packetlen=\n"
		"\t--media-type=\n"
		"\t--space=\n"
		"\t--ad=\n"
		"\t--noefe\n"
		"\t--u8\n"
		"\t--u16\n"
		"\t--utf8\n"
		"\t--bridge\n"
		"\t--closed\n",
		MKUDFFS_VERSION, UDFFS_VERSION, UDFFS_DATE
	);
	exit(1);
}

void parse_args(int argc, char *argv[], struct udf_disc *disc, char *device)
{
	int retval;
	int i;
	int media = DEFAULT_HD;
	uint16_t packetlen = 0;

	while ((retval = getopt_long(argc, argv, "b:r:h", long_options, NULL)) != EOF)
	{
		switch (retval)
		{
			case OPT_HELP:
			case 'h':
				usage();
				break;
			case OPT_BLK_SIZE:
			case 'b':
			{
				uint16_t bs;

				disc->blocksize = strtoul(optarg, NULL, 0);
				for (bs=512,disc->blocksize_bits=9;
					disc->blocksize_bits<13;
					disc->blocksize_bits++,bs<<=1)
				{
					if (disc->blocksize == bs)
						break;
				}
				if (disc->blocksize_bits == 13)
				{
					fprintf(stderr, "mkudffs: invalid blocksize\n");
					exit(1);
				}
				disc->udf_lvd[0]->logicalBlockSize = cpu_to_le32(disc->blocksize);
				break;
			}
			case OPT_UDF_REV:
			case 'r':
			{
				if (udf_set_version(disc, strtoul(optarg, NULL, 16)))
				{
					fprintf(stderr, "mkudffs: invalid udf revision\n");
					exit(1);
				}
				break;
			}
			case OPT_NO_EFE:
			{
				disc->flags &= ~FLAG_EFE;
				break;
			}
			case OPT_UNICODE8:
			{
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE8;
				break;
			}
			case OPT_UNICODE16:
			{
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE16;
				break;
			}
			case OPT_UTF8:
			{
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UTF8;
				break;
			}
			case OPT_BRIDGE:
			{
				disc->flags |= FLAG_BRIDGE;
				break;
			}
			case OPT_CLOSED:
			{
				disc->flags |= FLAG_CLOSED;
				break;
			}
			case OPT_LVID:
			{
				disc->udf_lvd[0]->logicalVolIdent[127] = encode_string(disc, disc->udf_lvd[0]->logicalVolIdent, "", optarg, 128);
				((struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse)->logicalVolIdent[127] = encode_string(disc, ((struct impUseVolDescImpUse *)disc->udf_iuvd[0]->impUse)->logicalVolIdent, "", optarg, 128);
				disc->udf_fsd->logicalVolIdent[127] = encode_string(disc, disc->udf_fsd->logicalVolIdent, "", optarg, 128);
				break;
			}
			case OPT_VID:
			{
				disc->udf_pvd[0]->volIdent[31] = encode_string(disc, disc->udf_pvd[0]->volIdent, "", optarg, 32);
				break;
			}
			case OPT_VSID:
			{
				char ts[9];
				strncpy(ts, &disc->udf_pvd[0]->volSetIdent[1], 8);
				disc->udf_pvd[0]->volSetIdent[127] = encode_string(disc, disc->udf_pvd[0]->volSetIdent, ts, optarg, 128);
				break;
			}
			case OPT_FSID:
			{
				disc->udf_fsd->fileSetIdent[31] = encode_string(disc, disc->udf_fsd->fileSetIdent, "", optarg, 32);
				break;
			}
			case OPT_STRATEGY:
			{
				uint16_t strategy;

				strategy = strtoul(optarg, NULL, 0);
				if (strategy == 4096)
					disc->flags |= FLAG_STRATEGY4096;
				else if (strategy != 4)
				{
					fprintf(stderr, "mkudffs: invalid strategy type\n");
					exit(1);
				}
				break;
			}
			case OPT_SPARTABLE:
			{
				uint8_t spartable;

				spartable = strtoul(optarg, NULL, 0);
				if (spartable > 4)
				{
					fprintf(stderr, "mkudffs: invalid spartable count\n");
					exit(1);
				}
				add_type2_sparable_partition(disc, 0, spartable, packetlen);
				media = DEFAULT_CDRW;
				break;
			}
			case OPT_PACKETLEN:
			{
				struct sparablePartitionMap *spm;

				packetlen = strtoul(optarg, NULL, 0);
				if ((spm = find_type2_sparable_partition(disc, 0)))
					spm->packetLength = cpu_to_le16(packetlen);
				break;
			}
			case OPT_MEDIA_TYPE:
			{
				if (!strncmp(optarg, "hd", 2))
					media = DEFAULT_HD;
				else if (!strcmp(optarg, "dvd"))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_READ_ONLY);
					media = DEFAULT_DVD;
				}
				else if (!strncmp(optarg, "dvdram", 6))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
					media = DEFAULT_DVDRAM;
				}
				else if (!strncmp(optarg, "dvdrw", 5))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_OVERWRITABLE);
					media = DEFAULT_DVDRW;
					packetlen = 16;
				}
				else if (!strncmp(optarg, "worm", 4))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
					media = DEFAULT_WORM;
					disc->flags |= (FLAG_STRATEGY4096 | FLAG_BLANK_TERMINAL);
				}
				else if (!strncmp(optarg, "mo", 2))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);
					media = DEFAULT_MO;
					disc->flags |= (FLAG_STRATEGY4096 | FLAG_BLANK_TERMINAL);
				}
				else if (!strncmp(optarg, "cdrw", 4))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);
					media = DEFAULT_CDRW;
				}
				else if (!strncmp(optarg, "cdr", 3))
				{
					disc->udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_WRITE_ONCE);
					media = DEFAULT_CDR;
					disc->flags |= FLAG_VAT;
					disc->flags &= ~FLAG_CLOSED;
				}
				else
				{
					fprintf(stderr, "mkudffs: invalid media type\n");
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
					fprintf(stderr, "mkudffs: invalid space type\n");
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
					fprintf(stderr, "mkudffs: invalid allocation descriptor\n");
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
	strcpy(device, argv[optind]);
	optind ++;
	if (optind < argc)
		disc->head->blocks = strtoul(argv[optind++], NULL, 0);
	else
		disc->head->blocks = 0;
	if (optind < argc)
		usage();

	if (le32_to_cpu(disc->udf_lvd[0]->numPartitionMaps) == 0)
	{
		if (media == DEFAULT_CDRW || media == DEFAULT_DVDRW)
			add_type2_sparable_partition(disc, 0, 2, packetlen);
		else if (media == DEFAULT_CDR)
		{
			add_type1_partition(disc, 0);
			add_type2_virtual_partition(disc, 0);
		}
		else
			add_type1_partition(disc, 0);
	}

	if (!(disc->flags & FLAG_SPACE))
		disc->flags |= FLAG_UNALLOC_BITMAP;

	if (media == DEFAULT_CDR)
		disc->flags &= ~FLAG_SPACE;

	for (i=0; i<UDF_ALLOC_TYPE_SIZE; i++)
	{
		if (disc->sizing[i].denomSize == 0)
			memcpy(&disc->sizing[i], &default_sizing[media][i], sizeof(default_sizing[media][i]));
	}
}
