/*
 * cdinfo.c
 *
 * PURPOSE
 *	A simple utility to display CDROM and block device info.
 *
 * DESCRIPTION
 *	Usage: cdinfo device 
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/cdrom.h>

int iso_get_last_session(int fd);


int iso_get_last_session(int fd)
{
  struct cdrom_multisession ms_info;
  unsigned int vol_desc_start;
  int i;

  vol_desc_start=0;
  ms_info.addr_format=CDROM_LBA;
  i=ioctl(fd, CDROMMULTISESSION, (unsigned long) &ms_info);
  if (i==0) {
	  printf("XA disk: %s\n", ms_info.xa_flag ? "yes":"no");
	  printf("vol_desc_start = %d\n", ms_info.addr.lba);
  } else
      	  printf("CDROMMULTISESSION not supported: rc=%d\n",i);
  if ( ms_info.xa_flag )
	vol_desc_start=ms_info.addr.lba;
  return vol_desc_start;
}

int main(int argc, char **argv)
{
	int fd;
	long value;
	struct cdrom_tochdr toc_h;
	struct cdrom_tocentry toc_e;
	int i,j;

	if (argc < 2) {
		printf("usage: cdinfo <device>\n");
		return -1;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return -1;
	}

	printf("CDINFO %s\n", argv[1]);
	iso_get_last_session(fd);


	i=ioctl(fd, BLKGETSIZE, (unsigned long) &value);
	if ( i == 0 ) {
		printf("BLKGETSIZE = %ld\n", value);
	} else {
		printf("BLKGETSIZE = not supported\n");
	}
	i=ioctl(fd, CDROMREADTOCHDR, (unsigned long)&toc_h);
	if ( i == 0 ) {
		printf("cdtracks first: %u last: %u\n",
			toc_h.cdth_trk0, toc_h.cdth_trk1);
		/* check the cdrom tracks */
		for (j=toc_h.cdth_trk0; j<= toc_h.cdth_trk1; j++) {
			toc_e.cdte_track = j;
			toc_e.cdte_format = CDROM_LBA;

			i=ioctl(fd, CDROMREADTOCENTRY, (unsigned long)&toc_e);
			if ( i == 0 ) {
				printf("track(%d): ctrl %x start: %u\n",
						j, toc_e.cdte_ctrl,
						toc_e.cdte_addr.lba);
			}
		} /* end for */
		j=0xAA; /* leadout track */
		toc_e.cdte_track = j;
		toc_e.cdte_format = CDROM_LBA;

		i=ioctl(fd, CDROMREADTOCENTRY, (unsigned long)&toc_e);
		if ( i == 0 ) {
				printf("track(leadout): ctrl %x start: %u\n",
						toc_e.cdte_ctrl,
						toc_e.cdte_addr.lba);
		}
	}

	close(fd);
	return 0;
}
