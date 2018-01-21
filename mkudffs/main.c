/*
 * main.c
 *
 * Copyright (c) 2001-2002  Ben Fennema <bfennema@falcon.csc.calpoly.edu>
 * Copyright (c) 2014-2017  Pali Rohár <pali.rohar@gmail.com>
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
 * mkudffs main program and I/O functions
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fd.h>
#include <sys/sysmacros.h>

#include "mkudffs.h"
#include "defaults.h"
#include "options.h"

static int valid_offset(int fd, off_t offset)
{
	char ch;

	if (lseek(fd, offset, SEEK_SET) < 0)
		return 0;
	if (read(fd, &ch, 1) < 1)
		return 0;
	return 1;
}

static uint32_t get_blocks(int fd, int blocksize, uint32_t opt_blocks)
{
	uint64_t blocks;
#ifdef BLKGETSIZE64
	uint64_t size64;
#endif
#ifdef BLKGETSIZE
	long size;
#endif
#ifdef FDGETPRM
	struct floppy_struct this_floppy;
#endif
	struct stat buf;

	if (opt_blocks)
		return opt_blocks;

	if (fd <= 0)
		return 0;

#ifdef BLKGETSIZE64
	if (ioctl(fd, BLKGETSIZE64, &size64) >= 0)
		blocks = size64 / blocksize;
	else
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
	if (fstat(fd, &buf) == 0 && S_ISREG(buf.st_mode))
		blocks = buf.st_size / blocksize;
	else
	{
		off_t high, low;

		for (low=0, high = 1024; valid_offset(fd, high); high *= 2)
			low = high;
		while (low < high - 1)
		{
			const off_t mid = (low + high) / 2;

			if (valid_offset(fd, mid))
				low = mid;
			else
				high = mid;
		}

		valid_offset(fd, 0);
		blocks = (low + 1) / blocksize;
	}

	if (blocks > UINT32_MAX)
	{
		fprintf(stderr, "%s: Warning: Disk is too big, using only %lu blocks\n", appname, (unsigned long int)UINT32_MAX);
		return UINT32_MAX;
	}

	return blocks;
}

static void detect_blocksize(int fd, struct udf_disc *disc, int *blocksize)
{
#ifdef BLKSSZGET
	int size;

	if (ioctl(fd, BLKSSZGET, &size) != 0 || size <= 0)
		return;

	disc->blkssz = size;

	if (*blocksize != -1)
		return;

	if (size < 512 || size > 32768 || (size & (size - 1)))
	{
		fprintf(stderr, "%s: Warning: Disk logical sector size (%d) is not suitable for UDF\n", appname, size);
		return;
	}

	disc->blocksize = size;
	*blocksize = disc->blocksize;
	disc->udf_lvd[0]->logicalBlockSize = cpu_to_le32(disc->blocksize);
#endif
}

static int is_whole_disk(int fd)
{
	struct stat st;
	char buf[512];
	DIR *dir;
	struct dirent *d;
	int maj;
	int min;
	int has_slave;
	int slave_errno;

	if (fstat(fd, &st) != 0)
		return -1;

	if (!S_ISBLK(st.st_mode))
		return 1;

	maj = major(st.st_rdev);
	min = minor(st.st_rdev);

	if (snprintf(buf, sizeof(buf), "/sys/dev/block/%d:%d/partition", maj, min) >= (int)sizeof(buf))
		return -1;

	if (stat(buf, &st) == 0)
		return 0;
	else if (errno != ENOENT)
		return -1;

	if (snprintf(buf, sizeof(buf), "/sys/dev/block/%d:%d/slaves/", maj, min) >= (int)sizeof(buf))
		return -1;

	dir = opendir(buf);
	if (!dir && errno != ENOENT)
		return -1;

	if (dir)
	{
		errno = 0;
		has_slave = 0;
		while ((d = readdir(dir)))
		{
			if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
				continue;
			has_slave = 1;
			break;
		}
		slave_errno = errno;

		closedir(dir);

		if (slave_errno)
			return -1;

		if (has_slave)
			return 0;
	}

	if (snprintf(buf, sizeof(buf), "/sys/dev/block/%d:%d/", maj, min) >= (int)sizeof(buf))
		return -1;

	if (stat(buf, &st) != 0)
		return -1;

	return 1;
}

static int is_removable_disk(int fd)
{
	struct stat st;
	char buf[512];
	ssize_t ret;

	if (fstat(fd, &st) != 0)
		return -1;

	if (!S_ISBLK(st.st_mode))
		return -1;

	if (snprintf(buf, sizeof(buf), "/sys/dev/block/%d:%d/removable", major(st.st_rdev), minor(st.st_rdev)) >= (int)sizeof(buf))
		return -1;

	int rem_fd = open(buf, O_RDONLY);
	if (rem_fd < 0)
		return -1;

	ret = read(rem_fd, buf, sizeof(buf)-1);
	close(rem_fd);

	if (ret < 0)
		return -1;

	buf[ret] = 0;
	if (strcmp(buf, "1\n") == 0)
		return 1;
	else if (strcmp(buf, "0\n") == 0)
		return 0;

	return -1;
}

static int write_func(struct udf_disc *disc, struct udf_extent *ext)
{
	static char *buffer = NULL;
	static size_t bufferlen = 0;
	int fd = *(int *)disc->write_data;
	ssize_t length, offset;
	uint32_t blocks;
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
			if (!(disc->flags & FLAG_NO_WRITE) || fd >= 0)
			{
				if (lseek(fd, (off_t)(ext->start + desc->offset) * disc->blocksize, SEEK_SET) < 0)
					return -1;
			}
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
			if (!(disc->flags & FLAG_NO_WRITE))
			{
				if (write(fd, buffer, length) != length)
					return -1;
			}
			desc = desc->next;
		}
	}
	else if (!(disc->flags & FLAG_BOOTAREA_PRESERVE))
	{
		length = disc->blocksize;
		blocks = ext->blocks;
		memset(buffer, 0, length);
		if (!(disc->flags & FLAG_NO_WRITE) || fd >= 0)
		{
			if (lseek(fd, (off_t)(ext->start) * disc->blocksize, SEEK_SET) < 0)
				return -1;
		}
		while (blocks-- > 0)
		{
			if (!(disc->flags & FLAG_NO_WRITE))
			{
				if (write(fd, buffer, length) != length)
					return -1;
			}
		}
	}
	return 0;
}
	
int main(int argc, char *argv[])
{
	struct udf_disc	disc;
	struct stat stat;
	char *filename;
	char buf[128*3];
	int fd;
	int create_new_file = 0;
	int blocksize = -1;
	int media;
	size_t len;

	if (fcntl(0, F_GETFL) < 0 && open("/dev/null", O_RDONLY) < 0)
		_exit(1);
	if (fcntl(1, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);
	if (fcntl(2, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);

	setlocale(LC_CTYPE, "");
	appname = "mkudffs";

	udf_init_disc(&disc);
	parse_args(argc, argv, &disc, &filename, &create_new_file, &blocksize, &media);

	if (disc.flags & FLAG_NO_WRITE)
		printf("Note: Not writing to device, just simulating\n");

	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		if (errno != ENOENT)
		{
			fprintf(stderr, "%s: Error: Cannot open device '%s': %s\n", appname, filename, strerror(errno));
			exit(1);
		}

		if (!disc.blocks)
		{
			fprintf(stderr, "%s: Error: Cannot create new image file '%s': block-count was not specified\n", appname, filename);
			exit(1);
		}
	}
	else
	{
		int fd2;
		int flags2;
		char filename2[64];
		const char *error;

		if (create_new_file)
		{
			fprintf(stderr, "%s: Error: Cannot create new image file '%s': %s\n", appname, filename, strerror(EEXIST));
			exit(1);
		}

		if (fstat(fd, &stat) != 0)
		{
			fprintf(stderr, "%s: Error: Cannot stat device '%s': %s\n", appname, filename, strerror(errno));
			exit(1);
		}

		if (!(disc.flags & FLAG_NO_WRITE))
			flags2 = O_RDWR;
		else
			flags2 = O_RDONLY;

		if (snprintf(filename2, sizeof(filename2), "/proc/self/fd/%d", fd) >= (int)sizeof(filename2))
		{
			fprintf(stderr, "%s: Error: Cannot open device '%s': %s\n", appname, filename, strerror(ENAMETOOLONG));
			exit(1);
		}

		// Re-open block device with O_EXCL mode which fails when device is already mounted
		if (S_ISBLK(stat.st_mode))
			flags2 |= O_EXCL;

		fd2 = open(filename2, flags2);
		if (fd2 < 0)
		{
			if (errno != ENOENT)
			{
				error = (errno != EBUSY) ? strerror(errno) : "Device is mounted or mkudffs is already running";
				fprintf(stderr, "%s: Error: Cannot open device '%s': %s\n", appname, filename, error);
				exit(1);
			}

			// Fallback to orignal filename when /proc is not available, but this introduce race condition between stat and open
			fd2 = open(filename, flags2);
			if (fd2 < 0)
			{
				error = (errno != EBUSY) ? strerror(errno) : "Device is mounted or mkudffs is already running";
				fprintf(stderr, "%s: Error: Cannot open device '%s': %s\n", appname, filename, error);
				exit(1);
			}
		}

		close(fd);
		fd = fd2;
	}

	if (fd >= 0)
		detect_blocksize(fd, &disc, &blocksize);

	if (blocksize == -1 && media == MEDIA_TYPE_HD)
	{
		disc.blocksize = 512;
		disc.udf_lvd[0]->logicalBlockSize = cpu_to_le32(disc.blocksize);
	}

	disc.blocks = get_blocks(fd, disc.blocksize, disc.blocks);
	disc.head->blocks = disc.blocks;
	disc.write = write_func;
	disc.write_data = &fd;

	if (!(disc.flags & FLAG_BOOTAREA_MASK))
	{
		if (media != MEDIA_TYPE_HD)
			disc.flags |= FLAG_BOOTAREA_PRESERVE;
		else if (fd >= 0 && is_removable_disk(fd) == 0 && is_whole_disk(fd) == 1)
			disc.flags |= FLAG_BOOTAREA_MBR;
		else
			disc.flags |= FLAG_BOOTAREA_ERASE;
	}

	printf("filename=%s\n", filename);

	memset(buf, 0, sizeof(buf));
	len = decode_string(&disc, disc.udf_lvd[0]->logicalVolIdent, buf, 128, sizeof(buf));
	printf("label=%s\n", buf);

	memset(buf, 0, sizeof(buf));
	len = gen_uuid_from_vol_set_ident(buf, disc.udf_pvd[0]->volSetIdent, 128);
	printf("uuid=%s\n", buf);

	printf("blocksize=%u\n", (unsigned int)disc.blocksize);
	printf("blocks=%lu\n", (unsigned long int)disc.blocks);
	printf("udfrev=%x.%02x\n", (unsigned int)(disc.udf_rev >> 8), (unsigned int)(disc.udf_rev & 0xFF));

	split_space(&disc);

	setup_mbr(&disc);
	setup_vrs(&disc);
	setup_anchor(&disc);
	setup_partition(&disc);
	setup_vds(&disc);

	if (disc.vat_block)
		printf("vatblock=%lu\n", (unsigned long int)disc.vat_block);

	dump_space(&disc);

	if (disc.blocks <= 257)
		fprintf(stderr, "%s: Warning: UDF filesystem has less then 258 blocks, it can cause problems\n", appname);

	if (len == (size_t)-1)
		fprintf(stderr, "%s: Warning: Volume Set Identifier must be at least 8 characters long\n", appname);
	else if (len < 16)
		fprintf(stderr, "%s: Warning: First 16 characters of Volume Set Identifier are not hexadecimal lowercase digits\n%s: Warning: This would cause problems for UDF uuid\n", appname, appname);

	if (fd >= 0 && is_whole_disk(fd) == 0)
		fprintf(stderr, "%s: Warning: Creating new UDF filesystem on partition, and not on whole disk device\n%s: Warning: UDF filesystem on partition cannot be read on Apple systems\n", appname, appname);

	if (fd < 0 && !(disc.flags & FLAG_NO_WRITE))
	{
		// Create new file disk image
		fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0660);
		if (fd < 0)
		{
			fprintf(stderr, "%s: Error: Cannot create new image file '%s': %s\n", appname, filename, strerror(errno));
			exit(1);
		}
	}

	if (write_disc(&disc) < 0)
	{
		fprintf(stderr, "%s: Error: Cannot write to device '%s': %s\n", appname, filename, strerror(errno));
		return 1;
	}

	return 0;
}
