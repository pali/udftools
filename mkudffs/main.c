/*
 * main.c
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

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>

#include "mkudffs.h"
#include "defaults.h"
#include "config.h"
#include "options.h"

static int64_t udf_lseek64(int fd, int64_t offset, int whence)
{
#if defined(HAVE_LSEEK64)
	return lseek64(fd, offset, whence);
#elif defined(HAVE_LLSEEK)
	return llseek(fd, offset, whence);
#else
	return lseek(fd, offset, whence);
#endif
}

static int valid_offset(int fd, int64_t offset)
{
	char ch;

	if (udf_lseek64(fd, offset, SEEK_SET) < 0)
		return 0;
	if (read(fd, &ch, 1) < 1)
		return 0;
	return 1;
}

int get_blocks(int fd, int blocksize, int opt_blocks)
{
	int blocks;
#ifdef BLKGETSIZE
	long size;
#endif
#ifdef FDGETPRM
	struct floppy_struct this_floppy;
#endif

#ifdef BLKGETSIZE
	if (ioctl(fd, BLKGETSIZE, &size) >= 0)
		blocks = size / (blocksize / 512);
	else
#endif
#ifdef FDGETPRM
	if (ioctl(fd, FDGETPRM, &this_floppy) >= 0)
		blocks = this_floppy.size / (blocksize / 512);
	else
#endif
	{
		int64_t high, low;

		for (low=0, high = 1024; valid_offset(fd, high); high *= 2)
			low = high;
		while (low < high - 1)
		{
			const int64_t mid = (low + high) / 2;

			if (valid_offset(fd, mid))
				low = mid;
			else
				high = mid;
		}

		valid_offset(fd, 0);
		blocks = (low + 1) / blocksize;
	}

	if (opt_blocks)
		blocks = opt_blocks;
	return blocks;
}

int write_func(struct udf_disc *disc, struct udf_extent *ext)
{
	static char *buffer = NULL;
	static int bufferlen = 0, length, offset;
	int fd = *(int *)disc->write_data;
	int ret;
	struct udf_desc *desc;
	struct udf_data *data;

	if (buffer == NULL)
	{
		bufferlen = disc->blocksize;
		buffer = calloc(bufferlen, 1);
	}

	if (!(ext->space_type & (USPACE|RESERVED)))
	{
		desc = ext->head;
		while (desc != NULL)
		{
			ret = udf_lseek64(fd, (uint64_t)(ext->start + desc->offset) << disc->blocksize_bits, SEEK_SET);
			data = desc->data;
			offset = 0;
			while (data != NULL)
			{
				if (data->length + offset > bufferlen)
				{
					bufferlen = (data->length + offset + disc->blocksize - 1) & ~(disc->blocksize - 1);
					buffer = realloc(buffer, bufferlen);
				}
				memcpy(buffer + offset, data->buffer, data->length);
				offset += data->length;
				data = data->next;
			}
			length = (offset + disc->blocksize - 1) & ~(disc->blocksize - 1);
			if (offset != length)
				memset(buffer + offset, 0x00, length - offset);
			if (write(fd, buffer, length) != length)
				return -1;
			desc = desc->next;
		}
	}
	return 0;
}
	
int main(int argc, char *argv[])
{
	struct udf_disc	disc;
	char filename[NAME_MAX];
	int fd;

	memset(&disc, 0x00, sizeof(disc));
	udf_init_disc(&disc);
	parse_args(argc, argv, &disc, filename);
#ifdef HAVE_OPEN64
	fd = open64(filename, O_RDWR | O_CREAT, 0660);
#else
	fd = open(filename, O_RDWR | O_CREAT | O_LARGEFILE, 0660);
#endif
	disc.head->blocks = get_blocks(fd, disc.blocksize, disc.head->blocks);
	disc.write = write_func;
	disc.write_data = &fd;

	split_space(&disc);

	setup_vrs(&disc);
	setup_anchor(&disc);
	setup_partition(&disc);
	setup_vds(&disc);

	dump_space(&disc);

	if (write_disc(&disc) < 0)
		return -1;
	else
		return 0;
}
