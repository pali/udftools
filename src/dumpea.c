/*
 * dumpea.c
 *
 * PURPOSE
 *	A simple utility to dump extended attributes.
 *
 * DESCRIPTION
 *
 *	Usage: dumpea udf-file | od
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/udf_fs_i.h> /* for ioctl */

unsigned char  sector[2048];
void dump_hex(unsigned char * p, int len, char * prefix);

int main(int argc, char **argv)
{
	int fd;
	long retval;
	int size;
	int binaryflag=0;

	if (argc < 2) {
		printf("usage: dumpea [-binary] <file>\n");
		return -1;
	}

	if ( strcmp(argv[1], "-binary") == 0 ) {
		binaryflag=1;
		argc--;
		argv++;
	}

	while (argc > 1 ) {

		fd = open(argv[1], O_RDONLY);
		if (fd < 0) {
			perror(argv[1]);
			return -1;
		}

		size=0;
		
		retval=ioctl(fd, UDF_GETEASIZE, &size);

		if ( (!binaryflag) && (retval != -1) ) {
			printf("%s: ea len: %d\n", argv[1], size);
		}

		if ( retval != -1 ) {
			memset(sector, 0, 2048);
			if ( ioctl(fd, UDF_GETEABLOCK, sector) != -1 ) {
				if (!binaryflag)
					dump_hex(sector, size, "");
				else
					write(1, sector, size);
			}
		} else {
			if (!binaryflag) {
				printf("errno = %d\n", errno);
				perror(argv[1]);
			}
		}

		close(fd);
		argc--;
		argv++;
	}
	return 0;
}

void dump_hex(unsigned char * p, int len, char * prefix)
{
	int i,j;

	for (i = 0; i < len; i += 16) {
		printf("%s%03Xh: ", prefix, i);

		for (j = 0; (j < 16)&&(i+j < len); j++)
			printf((j == 7) ? "%02x-": "%02x ", p[i + j]);
		for ( ; j<16; j++)
			printf((j == 7) ? "%2s-": "%2s ", "");
			
		for (j = 0; (j < 16)&&(i+j < len); j++) {
			if ( p[i + j] < 31 ||  p[i+j] > 126)
				printf(".");
			else
				printf("%c", p[i+j]);
		}
		printf("\n");
	}
}

