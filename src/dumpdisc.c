/*
 * dump.c
 *
 * PURPOSE
 *	A simple utility to dump disk sectors.
 *
 * DESCRIPTION
 *	I got tired of using dd and hexdump :-)
 *
 *	Usage: dump device sector | less
 *	The sector can be specified in decimal, octal, or hex.
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

static char cdsync[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
			 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

int read_cd(int fd, int sec, unsigned char *buffer)
{
	struct cdrom_generic_command cgc;
	int len = 2352;
	int ret;

	memset(&cgc, 0, sizeof(cgc));
	memset(buffer, 0, len);
	cgc.cmd[0] = GPCMD_READ_CD;
	cgc.cmd[1] = (0 << 2) | (0 << 0);
	cgc.cmd[2] = (sec >> 24) & 0xff;
	cgc.cmd[3] = (sec >> 16) & 0xff;
	cgc.cmd[4] = (sec >>  8) & 0xff;
	cgc.cmd[5] = (sec      ) & 0xff;
	cgc.cmd[6] = (1   >> 16) & 0xff;
	cgc.cmd[7] = (1   >>  8) & 0xff;
	cgc.cmd[8] = (1        ) & 0xff;
	cgc.cmd[9] = 0xf8;

	cgc.buffer = buffer;
	cgc.buflen = len;
	cgc.data_direction = CGC_DATA_READ;
	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret >= 0)
		return len;
	else
		return 0;
}

int main(int argc, char **argv)
{
	int src, dst, err, size = 0, sec = 0, retval;
	unsigned char sector[2352];
	char eout[20];

	if (argc < 4) {
		printf("usage: dump <device> <image> <errors>\n");
		return -1;
	}
	src = open(argv[1], O_RDONLY | O_NONBLOCK);
	if (src < 0) {
		perror(argv[1]);
		return -1;
	}

	dst = open(argv[2], O_RDWR | O_TRUNC);
	if (dst < 0) {
		perror(argv[2]);
		return -1;
	}

	err = open(argv[3], O_RDWR | O_TRUNC);
	if (err < 0) {
		perror(argv[3]);
		return -1;
	}

	if (ioctl(src, CDROM_LAST_WRITTEN, (unsigned long)&size))
		exit(1);

	for (sec=0; sec<size; sec++)
	{
		retval = read_cd(src, sec, sector);
		if (memcmp(sector, cdsync, 12))
			retval = read_cd(src, sec, sector);
		if (memcmp(sector, cdsync, 12))
		{
			sprintf(eout, "%10d/%10d\n", sec, size);
			write(err, eout, strlen(eout));
		}
		write(dst, sector, 2352);
	}
	close(src);
	close(dst);
	close(err);
	return 0;
}
