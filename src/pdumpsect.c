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
 *		linux_udf@hootie.lvld.hp.com
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
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

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
	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret >= 0)
		return len;
	else
		return 0;
}

int main(int argc, char **argv)
{
	int fd, sec = 0, retval;
	unsigned char sector[2352];

	if (argc < 2) {
		printf("usage: dump <device> <sector>\n");
		return -1;
	}
	fd = open(argv[1], O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror(argv[1]);
		return -1;
	}

	if (argc == 3)
		sec = strtoul(argv[2], NULL, 0);

	retval = read_cd(fd, sec, sector);

	if ( retval > 0 ) {
		int i, j;
		printf("retval= %d\n", retval);
		for (i = 0; i < retval; i += 16) {
			printf("%08x-%03x: ", sec, i);
			for (j = 0; j < 16; j++)
				printf((j == 7) ? "%02x-": "%02x ",
					sector[i + j]);
			for (j = 0; j < 16; j++) {
				if (sector[i + j] < 31 || sector[i+j] > 126)
					printf(".");
				else
					printf("%c", sector[i+j]);
			}
			printf("\n");
		}
		/*retval = read(fd, sector, 2048);*/
		sec++;
	} else {
		printf("**** errno=%d ****\n", errno);
	}
	
	close(fd);
	return 0;
}
