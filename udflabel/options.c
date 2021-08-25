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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <getopt.h>

#include "libudffs.h"
#include "options.h"

static struct option long_options[] = {
	{ "help", no_argument, NULL, OPT_HELP },
	{ "blocksize", required_argument, NULL, OPT_BLK_SIZE },
	{ "startblock", required_argument, NULL, OPT_START_BLOCK },
	{ "lastblock", required_argument, NULL, OPT_LAST_BLOCK },
	{ "vatblock", required_argument, NULL, OPT_VAT_BLOCK },
	{ "force", no_argument, NULL, OPT_FORCE },
	{ "no-write", no_argument, NULL, OPT_NO_WRITE },
	{ "uuid", required_argument, NULL, OPT_UUID },
	{ "lvid", required_argument, NULL, OPT_LVID },
	{ "vid", required_argument, NULL, OPT_VID },
	{ "vsid", required_argument, NULL, OPT_VSID },
	{ "fsid", required_argument, NULL, OPT_FSID },
	{ "fullvsid", required_argument, NULL, OPT_FULLVSID },
	{ "owner", required_argument, NULL, OPT_OWNER },
	{ "organization", required_argument, NULL, OPT_ORG },
	{ "contact", required_argument, NULL, OPT_CONTACT },
	{ "appid", required_argument, NULL, OPT_APPID },
	{ "impid", required_argument, NULL, OPT_IMPID },
	{ "locale", no_argument, NULL, OPT_LOCALE },
	{ "u8", no_argument, NULL, OPT_UNICODE8 },
	{ "u16", no_argument, NULL, OPT_UNICODE16 },
	{ "utf8", no_argument, NULL, OPT_UTF8 },
	{ 0, 0, NULL, 0 },
};

static void usage(void)
{
	fprintf(stderr, "udflabel from " PACKAGE_NAME " " PACKAGE_VERSION "\n"
		"Usage:\n"
		"\tudflabel [encoding-options] [block-options] [identifier-options] device [new-label]\n"
		"\n"
		"When all Identifier Options and new UDF Label are omitted then show current UDF Label.\n"
		"Otherwise set new Identifier Options. New UDF Label is synonym for both --lvid and --vid.\n"
		"\n"
		"Options:\n"
		"\t--help, -h         Display this help\n"
		"\n"
		"Block Options:\n"
		"\t--blocksize=, -b   Size of blocks in bytes (512, 1024, 2048, 4096, 8192, 16384, 32768; default: detect)\n"
		"\t--startblock=      Block location where the UDF filesystem starts (default: last session from TOC or 0)\n"
		"\t--lastblock=       Block location where the UDF filesystem ends (default: VAT block or last device block)\n"
		"\t--vatblock=        Block location of the Virtual Allocation Table (default: detect)\n"
		"\t--force            Force updating UDF disks without write support (useful only for disk images)\n"
		"\t--no-write, -n     Not really, do not write to device, just simulate\n"
		"\n"
		"Identifier Options:\n"
		"\t--uuid=, -u        New UDF UUID, first 16 characters of Volume Set Identifier\n"
		"\t--lvid=            New Logical Volume Identifier\n"
		"\t--vid=             New Volume Identifier\n"
		"\t--vsid=            New 17.-127. character of Volume Set Identifier\n"
		"\t--fsid=            New File Set Identifier\n"
		"\t--fullvsid=        New full Volume Set Identifier, overwrite --uuid and --vsid\n"
		"\t--owner=           New Owner name\n"
		"\t--organization=    New Organization name\n"
		"\t--contact=         New Contact information\n"
		"\t--appid=           New Application Identifier\n"
		"\t--impid=           New Developer Identifier for Implementation Identifier\n"
		"\n"
		"Encoding Options:\n"
		"\t--locale           Identifier options are encoded according to current locale (default)\n"
		"\t--u8               Identifier options are encoded in Latin1\n"
		"\t--u16              Identifier options are encoded in UTF-16BE\n"
		"\t--utf8             Identifier options are encoded in UTF-8\n"
	);
	exit(1);
}

static void process_uuid_arg(const char *arg, char *new_uuid)
{
	int i;
	time_t cur_time;
	uint32_t uuid_time;

	if (strcmp(arg, "random") == 0)
	{
		cur_time = time(NULL);
		if (cur_time != (time_t)-1 && cur_time >= 0)
			uuid_time = cur_time & 0xFFFFFFFF;
		else
			uuid_time = randu32();
		snprintf(new_uuid, 17, "%08"PRIx32"%08"PRIx32"", uuid_time, randu32());
		return;
	}

	if (strlen(arg) != 16)
	{
		fprintf(stderr, "%s: Error: Option --uuid is not 16 bytes long\n", appname);
		exit(1);
	}

	for (i = 0; i < 16; ++i)
	{
		if (!isxdigit((unsigned char)arg[i]) || (!isdigit((unsigned char)arg[i]) && !islower((unsigned char)arg[i])))
		{
			fprintf(stderr, "%s: Error: Option --uuid is not in lowercase hexadecimal digit format\n", appname);
			exit(1);
		}
	}

	memcpy(new_uuid, arg, 17);
}

static void process_vid_lvid_arg(struct udf_disc *disc, int option, const char *arg, dstring *new_lvid, dstring *new_vid)
{
	if (option != OPT_VID)
	{
		if (encode_string(disc, new_lvid, arg, 128) == (size_t)-1)
		{
			if (option == OPT_LVID)
				fprintf(stderr, "%s: Error: Option --lvid is too long\n", appname);
			else
				fprintf(stderr, "%s: Error: Label is too long\n", appname);
			exit(1);
		}
	}

	if (option != OPT_LVID)
	{
		if (encode_string(disc, new_vid, arg, 32) == (size_t)-1)
		{
			if (option == OPT_VID)
			{
				fprintf(stderr, "%s: Error: Option --vid is too long\n", appname);
				exit(1);
			}
			/* This code was not triggered by --vid option, do not throw error but rather store truncated --lvid */
			memcpy(new_vid, new_lvid, 32);
			new_vid[31] = 31;
		}
	}
}

void parse_args(int argc, char *argv[], struct udf_disc *disc, char **filename, int *force, dstring *new_lvid, dstring *new_vid, dstring *new_fsid, dstring *new_fullvsid, char *new_uuid, dstring *new_vsid, dstring *new_owner, dstring *new_org, dstring *new_contact, char *new_appid, char *new_impid)
{
	int failed;
	int ret;
	size_t len;

	while ((ret = getopt_long(argc, argv, "b:nu:h", long_options, NULL)) != EOF)
	{
		switch (ret)
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
				break;
			case OPT_START_BLOCK:
				disc->start_block = strtou32(optarg, 0, &failed);
				if (failed)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --startblock\n", appname);
					exit(1);
				}
				break;
			case OPT_LAST_BLOCK:
				disc->last_block = strtou32(optarg, 0, &failed);
				if (failed)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --lastblock\n", appname);
					exit(1);
				}
				break;
			case OPT_VAT_BLOCK:
				disc->vat_block = strtou32(optarg, 0, &failed);
				if (failed)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --vatblock\n", appname);
					exit(1);
				}
				break;
			case OPT_FORCE:
				*force = 1;
				break;
			case OPT_NO_WRITE:
			case 'n':
				disc->flags |= FLAG_NO_WRITE;
				break;
			case OPT_UUID:
			case 'u':
				process_uuid_arg(optarg, new_uuid);
				new_fullvsid[0] = 0xFF;
				break;
			case OPT_VID:
			case OPT_LVID:
				process_vid_lvid_arg(disc, ret, optarg, new_lvid, new_vid);
				break;
			case OPT_VSID:
				len = encode_string(disc, new_vsid, optarg, 128);
				if (len == (size_t)-1 || len > 127-16 || (new_vsid[0] == 16 && len > 127-16*2))
				{
					fprintf(stderr, "%s: Error: Option --vsid is too long\n", appname);
					exit(1);
				}
				new_fullvsid[0] = 0xFF;
				break;
			case OPT_FULLVSID:
				if (encode_string(disc, new_fullvsid, optarg, 128) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --fullvsid is too long\n", appname);
					exit(1);
				}
				new_uuid[0] = 0;
				new_vsid[0] = 0xFF;
				break;
			case OPT_FSID:
				if (encode_string(disc, new_fsid, optarg, 32) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --fsid is too long\n", appname);
					exit(1);
				}
				break;
			case OPT_OWNER:
				if (encode_string(disc, new_owner, optarg, 36) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --owner is too long\n", appname);
					exit(1);
				}
				break;
			case OPT_ORG:
				if (encode_string(disc, new_org, optarg, 36) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --organization is too long\n", appname);
					exit(1);
				}
				break;
			case OPT_CONTACT:
				if (encode_string(disc, new_contact, optarg, 36) == (size_t)-1)
				{
					fprintf(stderr, "%s: Error: Option --contact is too long\n", appname);
					exit(1);
				}
				break;
			case OPT_APPID:
			case OPT_IMPID:
				len = strlen(optarg);
				if (len > 23)
				{
					fprintf(stderr, "%s: Error: Option --%s is too long\n", appname, ret == OPT_APPID ? "appid" : "impid");
					exit(1);
				}
				if (len > 0)
				{
					if (len < 2)
					{
						fprintf(stderr, "%s: Error: Option --%s is too short\n", appname, ret == OPT_APPID ? "appid" : "impid");
						exit(1);
					}
					if (optarg[0] != '*')
					{
						fprintf(stderr, "%s: Error: Option --%s does not start with '*'\n", appname, ret == OPT_APPID ? "appid" : "impid");
						exit(1);
					}
				}
				memcpy(ret == OPT_APPID ? new_appid : new_impid, optarg, len < 23 ? len+1 : 23);
				for (; len > 0; len--)
				{
					if ((unsigned char)optarg[len-1] > 127)
					{
						fprintf(stderr, "%s: Error: Option --%s is not 7bit ASCII string\n", appname, ret == OPT_APPID ? "appid" : "impid");
						exit(1);
					}
				}
				break;
			case OPT_UNICODE8:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE8;
				if (strcmp(argv[1], "--u8") != 0)
				{
					fprintf(stderr, "%s: Error: Option --u8 must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			case OPT_UNICODE16:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE16;
				if (strcmp(argv[1], "--u16") != 0)
				{
					fprintf(stderr, "%s: Error: Option --u16 must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			case OPT_UTF8:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UTF8;
				if (strcmp(argv[1], "--utf8") != 0)
				{
					fprintf(stderr, "%s: Error: Option --utf8 must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			case OPT_LOCALE:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_LOCALE;
				if (strcmp(argv[1], "--locale") != 0)
				{
					fprintf(stderr, "%s: Error: Option --locale must be specified as first argument\n", appname);
					exit(1);
				}
				break;
			default:
				usage();
				break;
		}
	}

	if (optind+1 != argc && optind+2 != argc)
		usage();

	*filename = argv[optind];

	if (optind+2 == argc)
		process_vid_lvid_arg(disc, -1, argv[optind+1], new_lvid, new_vid);
}
