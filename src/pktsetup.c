/*
 * Quite possibly the ugliest piece I've ever written...
 *
 * Copyright (c) 1999,2000	Jens Axboe <axboe@suse.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>
#include <bits/types.h>
#include <sys/types.h>

#include <linux/pktcdvd.h>
#include <linux/cdrom.h>

int init_cdrom(int fd)
{
	if (ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) < 0) {
		perror("drive not ready\n");
		return 1;
	}

	if (ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT) < 0) {
		perror("no disc inserted?\n");
		return 1;
	}

	/*
	 * we don't care what disc type the uniform layer thinks it
	 * is, since it may get it wrong. what matters is that the above
	 * will force a TOC read.
	 */
	return 0;
}

void setup_dev(char *pkt_device, char *device, int rem)
{
	int pkt_fd = open(pkt_device, O_RDONLY), dev_fd = 0;
	unsigned int cmd = rem ? PACKET_TEARDOWN_DEV : PACKET_SETUP_DEV;

	if (pkt_fd < 0) {
		perror("packet open");
		return;
	}

	if (!rem) {
		if ((dev_fd = open(device, O_RDONLY | O_NONBLOCK)) < 0) {
			perror("open device");
			return;
		}
		if (init_cdrom(dev_fd))
			return;
	}
		
	if (ioctl(pkt_fd, cmd, dev_fd) < 0) {
		perror("PACKET_SET_DEV");
		return;
	}
	printf("%s %s\n", rem ? "removed" : "setup", pkt_device);
	close(dev_fd);
	close(pkt_fd);
}

void usage(void)
{
	printf("pktsetup /dev/pktcdvd0 /dev/hdd\tsetup device\n");
	printf("pktsetup -d /dev/pktcdvd0\ttear down device\n");
}

int main(int argc, char **argv)
{
	int rem = 0, c;

	if (argc == 1) {
		usage();
		return 1;
	}

	while ((c = getopt(argc, argv, "d")) != EOF) {
		switch (c) {
			case 'd':
				rem = 1;
				break;
			default:
				usage();
				exit(1);
		}
	}
	setup_dev(argv[optind], argv[optind + 1], rem);
	return 0;
}
