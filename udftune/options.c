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
	{ "mark-ro", no_argument, NULL, OPT_MARK_RO },
	{ "mark-rw", no_argument, NULL, OPT_MARK_RW },
	{ 0, 0, NULL, 0 },
};

static void usage(void)
{
	fprintf(stderr, "udftune from " PACKAGE_NAME " " PACKAGE_VERSION "\n"
		"Usage:\n"
		"\tudftune [block-options] device [filesystem-options]\n"
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
		"Filesystem Options:\n"
		"\t--mark-ro          Set target to read-only mode\n"
		"\t--mark-rw          Set target to read-write mode\n"
	);
	exit(1);
}

void parse_args(int argc, char *argv[], struct udf_disc *disc, char **filename, int *force, int *mark_ro, int *mark_rw)
{
	int failed;
	int ret;

	while ((ret = getopt_long(argc, argv, "nh", long_options, NULL)) != EOF) // XXX
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
			case OPT_MARK_RO:
				if (*mark_rw == 1)
				{
					fprintf(stderr, "%s: Cannot mark a filesystem read-only and re-write at the same time\n", appname);
					exit(1);
				}
				*mark_ro = 1;
				break;
			case OPT_MARK_RW:
				if (*mark_ro == 1)
				{
					fprintf(stderr, "%s: Cannot mark a filesystem read-only and re-write at the same time\n", appname);
					exit(1);
				}
				*mark_rw = 1;
				break;
			default:
				usage();
				break;
		}
	}

	if (optind+1 != argc && optind+2 != argc)
		usage();

	*filename = argv[optind];

}
