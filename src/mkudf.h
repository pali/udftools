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

#ifndef _MKUDF_H
#define _MKUDF_H

/* These should be changed if you make any code changes */
#define VERSION_MAJOR	1
#define VERSION_MINOR	4
#define VERSION_BUILD	5

#include <sys/time.h>
#include <udfdecl.h>

typedef enum media_types
{
	MT_HD,
	MT_CDR,
	MT_CDRW,
	MT_DVDR,
	MT_DVDRW,
	MT_DVDRAM
} media_types;

typedef enum partition_types
{
	PT_NORMAL,
	PT_SPARING,
	PT_VAT
} partition_types;

typedef struct mkudf_options
{
	Uint32 blocks;
	media_types media;
	partition_types partition;
	Uint32 blocksize;
	Uint32 blocksize_bits;
	dstring lvol_id[128];
	dstring vol_id[32];
	dstring set_id[128];
	dstring file_set_id[32];
	int device;
} mkudf_options;

typedef void (*write_func)(mkudf_options *,int,void *,int,char *);

void write_anchor(write_func, mkudf_options *, int, int, int, int, int);
void write_primaryvoldesc(write_func, mkudf_options *, int, int, timestamp);
void write_logicalvoldesc(write_func, mkudf_options *, int, int, int, int, int, int, int, int);
timestamp query_timestamp(struct timeval *, struct timezone *);
tag query_tag(Uint16, Uint16, Uint16, Uint32, void *, size_t);
icbtag query_icbtag(Uint32, Uint16, Uint16, Uint16, Uint8, Uint32, Uint16, Uint16);

int compute_ident_length(int);

int mkudf(write_func, mkudf_options *);

#endif /* _MKUDF_H */
