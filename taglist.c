/*
 * taglist.c
 *
 * PURPOSE
 *	A simple utility to dump disk sectors tags.
 *
 * DESCRIPTION
 *	I got tired of using dd and hexdump :-)
 *
 *	Usage: taglist device start_sector end_sector| less
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
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

typedef unsigned char Uint8;

Uint8 sector[2048];

int main(int argc, char **argv)
{
	int fd, retval;
	unsigned long sec = 0;
	unsigned long endsec = 0;
	unsigned short tag;

	if (argc < 4) {
		printf("usage: taglist <device> <start_sector> <end_sector>\n");
		return -1;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		return -1;

	sec = strtoul(argv[2], NULL, 0);
	endsec = strtoul(argv[3], NULL, 0);

	retval = lseek(fd, sec << 11, SEEK_SET);
	if (retval < 0) 
		return -1;

	retval = read(fd, sector, 2048);
	while ((retval > 0) && (sec <= endsec)) {
		tag=*(unsigned short *)sector;
		if ( (tag>0) && (tag <= 0x109) ) {
		    printf("%8u: 0x%04x - ", sec, tag);
		    switch (tag) {
			case 1:  printf("PrimaryVolDesc\n"); break;
			case 2:  printf("AnchorVolDescPtr\n"); break;
			case 3:  printf("VolDescPtr\n"); break;
			case 4:  printf("ImpUseVolDesc\n"); break;
			case 5:  printf("PartitionDesc\n"); break;
			case 6:  printf("LogicalVolDesc\n"); break;
			case 7:  printf("UnallocatedSpaceDesc\n"); break;
			case 8:  printf("TerminatingDesc\n"); break;
			case 9:  printf("LogicalVolIntegrityDesc\n"); break;
			case 0x100:  printf("FileSetDesc\n"); break;
			case 0x101:  printf("FileIdentDesc\n"); break;
			case 0x102:  printf("AllocExtDesc\n"); break;
			case 0x103:  printf("IndirectEntry\n"); break;
			case 0x104:  printf("TerminalEntry\n"); break;
			case 0x105:  printf("FileEntry\n"); break;
			case 0x106:  printf("ExtendedAttrHeaderDesc\n"); break;
			case 0x107:  printf("UnallocatedSpaceEntry\n"); break;
			case 0x108:  printf("SpaceBitmap\n"); break;
			case 0x109:  printf("PartitionIntegrityEntry\n"); break;
			default: printf("\n"); break;
		    }
		}
		retval = read(fd, sector, 2048);
		sec++;
	}
	
	close(fd);
	return 0;
}
