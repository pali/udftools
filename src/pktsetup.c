/*
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

#include <linux/cdrom.h>

/*
 * if we don't have one, we probably have neither
 */
#ifndef PACKET_SETUP_DEV
#define PACKET_SETUP_DEV	_IOW('X', 1, unsigned int)
#define PACKET_TEARDOWN_DEV	_IOW('X', 2, unsigned int)
#endif

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
	int pkt_fd, dev_fd, cmd;

	if ((pkt_fd = open(pkt_device, O_RDONLY | O_CREAT)) == -1) {
		perror("open packet device");
		return;
	}

	if (!rem) {
		cmd = PACKET_SETUP_DEV;
		if ((dev_fd = open(device, O_RDONLY | O_NONBLOCK)) == -1) {
			perror("open cd-rom");
			close(pkt_fd);
			return;
		}
		if (init_cdrom(dev_fd)) {
			close(pkt_fd);
			close(dev_fd);
			return;
		}
	} else {
		cmd = PACKET_TEARDOWN_DEV;
		dev_fd = 0; /* silence gcc */
	}
		
	if (ioctl(pkt_fd, cmd, dev_fd) == -1)
		perror("ioctl");

	if (dev_fd)
		close(dev_fd);
	close(pkt_fd);
}

int usage(void)
{
	printf("pktsetup /dev/pktcdvd0 /dev/cdrom\tsetup device\n");
	printf("pktsetup -d /dev/pktcdvd0\t\ttear down device\n");
	return 1;
}

int main(int argc, char **argv)
{
	int rem = 0, c;

	if (argc == 1)
		return usage();

	while ((c = getopt(argc, argv, "d")) != EOF) {
		switch (c) {
			case 'd':
				rem = 1;
				break;
			default:
				return usage();
		}
	}
	setup_dev(argv[optind], argv[optind + 1], rem);
	return 0;
}
