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
#include <udfdecl.h>

#ifndef HAVE_LLSEEK_PROTOTYPE
extern Sint64 llseek (int fd, Sint64 offset, int origin);
#endif

Uint8 sector[2048];

static Sint64 udf_lseek64(int fd, Sint64 offset, int whence)
{
#ifdef __USE_LARGEFILE64
	return lseek64(fd, offset, whence);
#else
	return llseek(fd, offset, whence);
#endif /* __USE_LARGEFILE64 */
}

int main(int argc, char **argv)
{
	int fd;
	Sint64 retval;
	Sint64 fp;
	Uint64 sec = 0;
	Uint32 bytes = 2048;

	if (argc < 2) {
		printf("usage: dump <device> <sector> <bytes>\n");
		return -1;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return -1;
	}

	if (argc > 2)
		sec = strtoul(argv[2], NULL, 0);
	if (argc > 3)
		bytes = strtoul(argv[3], NULL, 0);

	fp = sec * bytes;
	retval = udf_lseek64(fd, fp, SEEK_SET);
	if (retval < 0) {
		perror(argv[1]);
		return -1;
	} else
		printf("%Lu: seek to %Ld ok, retval %Ld\n", 
			sec, fp, retval);

	retval = read(fd, sector, bytes);
	if ( retval > 0 ) {
		int i, j;
		printf("retval= %Ld\n", retval);
		for (i = 0; i < retval; i += 16) {
			printf("%08Lx-%03x: ", sec, i);
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
