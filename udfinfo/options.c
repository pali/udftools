/*
 * Copyright (C) 2017-2018  Pali Rohár <pali.rohar@gmail.com>
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

#include <getopt.h>

#include "libudffs.h"
#include "options.h"

static struct option long_options[] = {
	{ "help", no_argument, NULL, OPT_HELP },
	{ "blocksize", required_argument, NULL, OPT_BLK_SIZE },
	{ "vatblock", required_argument, NULL, OPT_VAT_BLOCK },
	{ "locale", no_argument, NULL, OPT_LOCALE },
	{ "u8", no_argument, NULL, OPT_UNICODE8 },
	{ "u16", no_argument, NULL, OPT_UNICODE16 },
	{ "utf8", no_argument, NULL, OPT_UTF8 },
	{ 0, 0, NULL, 0 },
};

static void usage(void)
{
	fprintf(stderr, "udfinfo from " PACKAGE_NAME " " PACKAGE_VERSION "\n"
		"Usage:\n"
		"\tudfinfo [--locale|--u8|--u16|--utf8] [-b|--blocksize=block-size] [--vatblock=block] device\n"
	);
	exit(1);
}

void parse_args(int argc, char *argv[], struct udf_disc *disc, char **filename)
{
	int failed;
	int ret;

	while ((ret = getopt_long(argc, argv, "b:h", long_options, NULL)) != EOF)
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
			case OPT_VAT_BLOCK:
				disc->vat_block = strtou32(optarg, 0, &failed);
				if (failed)
				{
					fprintf(stderr, "%s: Error: Invalid value for option --vatblock\n", appname);
					exit(1);
				}
				break;
			case OPT_UNICODE8:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE8;
				break;
			case OPT_UNICODE16:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UNICODE16;
				break;
			case OPT_UTF8:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_UTF8;
				break;
			case OPT_LOCALE:
				disc->flags &= ~FLAG_CHARSET;
				disc->flags |= FLAG_LOCALE;
				break;
			default:
				usage();
				break;
		}
	}

	if (optind+1 != argc)
		usage();

	*filename = argv[optind];
}
