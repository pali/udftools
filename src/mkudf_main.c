/*
 * mkudf.c
 *
 * PURPOSE
*      Create a Linux native UDF file system image.
 *
 * DESCRIPTION
 *      It is _vital_ that all possible error conditions be checked when
 *      writing to the file system image file!
 *
 *      All errors should be output on stderr (not stdout)!
 *
 * CONTACTS
 *      E-mail regarding any portion of the Linux UDF file system should be
 *      directed to the development team mailing list (run by majordomo):
 *              linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL) version 2.0. Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * HISTORY
 *
 * 11/20/98 blf  Took skeleton and started adding code to create a very simple,
 *               but legal udf filesystem
 *
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <udfdecl.h>
#include "mkudf.h"


#ifndef HAVE_LLSEEK_PROTOTYPE
extern Sint64 llseek (int fd, Sint64 offset, int origin);
#endif

/*
 * Command line option token values.
 *	0x0000-0x00ff	Single characters
 *	0x1000-0x1fff	Long switches (no arg)
 *	0x2000-0x2fff	Long settings (arg required)
 */
#define OPT_HELP		0x1000
#define OPT_VERBOSE		0x1001
#define OPT_VERSION		0x1002

#define OPT_BLOCKS		0x2000
#define OPT_MEDIA		0x2001
#define OPT_PARTITION	0x2002

#define OPT_BLK_SIZE	0x2010
#define OPT_LVOL_ID		0x2011

#define OPT_VOL_ID		0x2020
#define OPT_SET_ID		0x2021
#define OPT_ABSTRACT	0x2022
#define OPT_COPYRIGHT	0x2023
#define OPT_VOL_NUM		0x2024
#define OPT_VOL_MAX_NUM	0x2025
#define OPT_VOL_ICHANGE	0x2026
#define OPT_VOL_MAX_ICHANGE	0x2027

#define OPT_FILE_SET_ID	0x2030
#define OPT_FILE_SET_COPYRIGHT	0x2031
#define OPT_FILE_SET_ABSTRACT	0x2032

/* Long command line options */
struct option long_options[] = {
	{ "help", no_argument, NULL, OPT_HELP },
	{ "verbose", no_argument, NULL, OPT_VERBOSE },
	{ "version", no_argument, NULL, OPT_VERSION },
	/* Media Data */
	{ "blocks", required_argument, NULL, OPT_BLOCKS },
	{ "media-type", required_argument, NULL, OPT_MEDIA },
	{ "partition-type", required_argument, NULL, OPT_PARTITION },
	/* Logical Volume Descriptor */
	{ "blocksize", required_argument, NULL, OPT_BLK_SIZE },
	{ "logical-volume-id", required_argument, NULL, OPT_LVOL_ID },
	/* Primary Volume Descriptor */
	{ "volume-id", required_argument, NULL, OPT_VOL_ID },
	{ "volume-set-id", required_argument, NULL, OPT_SET_ID },
	{ "volume-abstract", required_argument, NULL, OPT_ABSTRACT },
	{ "volume-copyright", required_argument, NULL, OPT_COPYRIGHT },
	{ "volume-number", required_argument, NULL, OPT_VOL_NUM },
	{ "volume-max-number", required_argument, NULL, OPT_VOL_MAX_NUM },
	{ "volume-ichange", required_argument, NULL, OPT_VOL_ICHANGE },
	{ "volume-max-ichange", required_argument, NULL, OPT_VOL_MAX_ICHANGE },
	/* File Set Descriptor */
	{ "file-set-id", required_argument, NULL, OPT_FILE_SET_ID },
	{ "file-set-copyright", required_argument, NULL, OPT_FILE_SET_COPYRIGHT },
	{ "file-set-abstract", required_argument, NULL, OPT_FILE_SET_ABSTRACT }
};

/*
 * usage
 *
 * DESCRIPTION
 *	Output a usage message to stderr, and exit.
 *
 * HISTORY
 *	December 2, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
usage(void)
{
	fprintf(stderr, "usage:\n"
		"\tmkudf [options] FILE\n"
		"Switches:\n"
		"\t--help\n"
		"\t--verbose\n"
		"\t--version\n"
		"Settings:\n"
		"\t--blocks=NUMBER\n"
		"\t--media-type=hd|cdr|cdrw|dvdr|dvdrw|dvdram\n"
		"\t--partition-type=normal|sparing|vat\n"

		"\t--blocksize=NUMBER\n"
		"\t--logical-volume-id=STRING\n"

		"\t--volume-id=STRING\n"
		"\t--volume-set-id=STRING\n"
		"\t--volume-abstract=FILE\n"
		"\t--volume-copyright=FILE\n"
		"\t--volume-number=NUMBER\n"
		"\t--volume-max-number=NUMBER\n"
		"\t--volume-ichange=NUMBER\n"
		"\t--volume-max-ichange=NUMBER\n"

		"\t--file-set-id=STRING\n"
		"\t--file-set-copyright=STRING\n"
		"\t--file-set-abstract=STRING\n"
	);
	exit(0);
}

/*
 * parse_args
 *
 * PURPOSE
 *	Parse the command line args.
 *
 * HISTORY
 *	December 2, 1997 - Andrew E. Mileski
 *	Written, tested, and released
 */
void
parse_args(int argc, char *argv[], struct mkudf_options *opt, char **filename)
{
	int retval;
	struct ustr instr;

	memset(opt, 0x00, sizeof(struct mkudf_options));
	opt->media = MT_HD;
	opt->partition = PT_NORMAL;

	while (argc > 1)
	{
		retval = getopt_long(argc, argv, "V", long_options, NULL);
		switch (retval)
		{
			case OPT_HELP:
				usage();
				break;
			case OPT_VERBOSE:
				break;
			case 'V':
			case OPT_VERSION:
				fprintf(stderr, "mkudf (Linux UDF tools) "
					"%d.%d (%d)\n", VERSION_MAJOR,
					VERSION_MINOR, VERSION_BUILD);
				exit(0);

			case OPT_BLOCKS:
				opt->blocks = strtoul(optarg, 0, 0);
				break;
			case OPT_MEDIA:
				break;
			case OPT_PARTITION:
				if (strlen(optarg) == 6 && !strncmp(optarg, "normal", 6))
					opt->partition = PT_NORMAL;
				else if (strlen(optarg) == 7 && !strncmp(optarg, "sparing", 7))
					opt->partition = PT_SPARING;
				else if (strlen(optarg) == 3 && !strncmp(optarg, "vat", 3))
					opt->partition = PT_VAT;
				else
				{
					fprintf(stderr, "%s --partition-type=normal|sparing|vat\n",
						argv[0]);
					exit(-1);
				}
				break;

			case OPT_BLK_SIZE:
				opt->blocksize = strtoul(optarg, 0, 0);
				break;
			case OPT_LVOL_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 126);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(opt->lvol_id, &instr, 128);
				break;

			case OPT_VOL_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 30);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(opt->vol_id, &instr, 32);
				break;
			case OPT_SET_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 126);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(opt->set_id, &instr, 128);
				break;
			case OPT_ABSTRACT:
			case OPT_COPYRIGHT:
			case OPT_VOL_NUM:
			case OPT_VOL_MAX_NUM:
			case OPT_VOL_ICHANGE:
			case OPT_VOL_MAX_ICHANGE:

			case OPT_FILE_SET_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 30);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(opt->file_set_id, &instr, 32);
			case OPT_FILE_SET_COPYRIGHT:
			case OPT_FILE_SET_ABSTRACT:
				break;

			case EOF:
				*filename = argv[optind];
				return;

			default:
				fprintf(stderr, "Try `mkudf --help' "
					"for more information\n");
				exit(-1);
		}
	}

	if (optind == argc)
		*filename = argv[optind];
	else {
		fprintf(stderr, "Try `mkudf --help' for more information\n");
		exit(-1);
	}
}

static Sint64 udf_lseek64(int fd, Sint64 offset, int whence)
{
#ifdef __USE_LARGEFILE64
	return lseek64(fd, offset, whence);
#else
	return llseek(fd, offset, whence);
#endif /* __USE_LARGEFILE64 */
}

void udf_write_data(mkudf_options *opt, int block, void *buffer, int size, char *type)
{
	static int last = 0;
	ssize_t retval = 0;
	static char empty_buffer[4096];
	int bsize;

	if (block > last)
		last = block;
	else
		printf("seeking back! last was %d, want %d (%s)\n", last, block, type);

	udf_lseek64(opt->device, (Sint64)block << opt->blocksize_bits, SEEK_SET);
	bsize = size & ~(opt->blocksize - 1);
	if (bsize)
		retval = write(opt->device, buffer, bsize);
	if (retval != -1 && (size - bsize))
	{
		memcpy(empty_buffer, buffer + bsize, size - bsize);
		retval = write(opt->device, empty_buffer, opt->blocksize);
		memset(empty_buffer, 0x00, opt->blocksize);
	}
			
	if (retval == -1)
	{
		printf("error writing %s: %s\n", type, sys_errlist[errno]);
		exit(-1);
	}
}

int get_blocksize(int *blocksize)
{
	int blocksize_bits;

	switch (*blocksize)
	{
		case 512:
			*blocksize = 512;
			blocksize_bits = 9;
			break;
		case 1024:
			*blocksize = 1024;
			blocksize_bits = 10;
			break;
		case 4096:
			*blocksize = 4096;
			blocksize_bits = 12;
			break;
		case 2048:
		default:
			*blocksize = 2048;
			blocksize_bits = 11;
			break;
	}
	return blocksize_bits;
}

static int valid_offset(int fd, Sint64 offset)
{
	char ch;

	if (udf_lseek64(fd, offset, SEEK_SET) < 0)
		return 0;
	if (read(fd, &ch, 1) < 1)
		return 0;
	return 1;
}

int get_blocks(int device, int blocksize, int opt_blocks)
{
	int blocks;
#ifdef BLKGETSIZE
	long size;
#endif
#ifdef FDGETPRM
	struct floppy_struct this_floppy;
#endif

#ifdef BLKGETSIZE
	if (ioctl(device, BLKGETSIZE, &size) >= 0)
		blocks = size / (blocksize / 512);
	else 
#endif
#ifdef FDGETPRM
	if (ioctl(device, FDGETPRM, &this_floppy) >= 0)
		blocks = this_floppy.size / (blocksize / 512);
	else
#endif
	{
		Sint64 high, low;

		for (low=0, high = 1024; valid_offset(device, high); high *= 2)
			low = high;
		while (low < high - 1)
		{
			const Sint64 mid = (low + high) / 2;

			if (valid_offset(device, mid))
				low = mid;
			else
				high = mid;
		}

		valid_offset(device, 0);
		blocks = (low + 1) / blocksize;
	}

	if (opt_blocks)
		blocks = opt_blocks;
	return blocks;
}
		
int
main(int argc, char *argv[])
{
	struct mkudf_options opt;
	char *filename;

	if ( argc < 2 )
	{
		usage();
		return 0;
	}
	parse_args(argc, argv, &opt, &filename);

	opt.device = open(filename, O_RDWR, 0660);
	if (opt.device == -1)
	{
		perror("error opening image file");
		return -1;
	}

	opt.blocksize_bits = get_blocksize(&(opt.blocksize));
	opt.blocks = get_blocks(opt.device, opt.blocksize, opt.blocks);

	return mkudf(udf_write_data, &opt);
}
