/*
 * Copyright (c) 1999,2000	Jens Axboe <axboe@suse.de>
 * Copyright (c) 2004		Peter Osterlund <petero2@telia.com>
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
#include <sys/stat.h>
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
#ifndef PACKET_CTRL_CMD
#define PACKET_CTRL_CMD		_IOWR('X', 1, struct pkt_ctrl_command)
#endif

#define MAJOR(dev)      ((dev & 0xfff00) >> 8)
#define MINOR(dev)      ((dev & 0xff) | ((dev >> 12) & 0xfff00))
#define MKDEV(ma,mi)    ((mi & 0xff) | (ma << 8) | ((mi & ~0xff) << 12))

#define MISC_MAJOR		10
#define CTL_DIR "/dev/pktcdvd"
#define CTL_DEV "control"

#define PKT_CTRL_CMD_SETUP	0
#define PKT_CTRL_CMD_TEARDOWN	1
#define PKT_CTRL_CMD_STATUS	2

struct pkt_ctrl_command {
	__u32 command;				/* in: Setup, teardown, status */
	__u32 dev_index;			/* in/out: Device index */
	__u32 dev;				/* in/out: Device nr for cdrw device */
	__u32 pkt_dev;				/* out: Device nr for packet device */
	__u32 num_devices;			/* out: Largest device index + 1 */
	__u32 padding;
};


static int init_cdrom(int fd)
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

static void setup_dev(char *pkt_device, char *device, int rem)
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

static int usage(void)
{
	printf("For pktcdvd < 0.2.0:\n");
	printf("  pktsetup /dev/pktcdvd0 /dev/cdrom  setup device\n");
	printf("  pktsetup -d /dev/pktcdvd0          tear down device\n");
	printf("For pktcdvd >= 0.2.0:\n");
	printf("  pktsetup dev_name /dev/cdrom       setup device\n");
	printf("  pktsetup -d dev_name               tear down device\n");
	printf("  pktsetup -d major:minor            tear down device\n");
	printf("  pktsetup -s                        show device mappings\n");
	return 1;
}

/*
 * Find the minor device number for the pktcdvd control device.
 */
static int get_misc_minor(void)
{
	int minor;
	char name[64];
	FILE *f;

	if ((f = fopen("/proc/misc", "r")) == NULL)
		return -1;
	while (fscanf(f, " %d %64s", &minor, name) == 2) {
		if (strcmp(name, "pktcdvd") == 0) {
			fclose(f);
			return minor;
		}
	}
	fclose(f);
	return -1;
}

static const char *pkt_dev_name(const char *dev)
{
	static char buf[128];
	snprintf(buf, sizeof(buf), "%s/%s", CTL_DIR, dev);
	return buf;
}

static void create_ctl_dev(void)
{
	int misc_minor;
	struct stat stat_buf;
	int dev;

	if ((misc_minor = get_misc_minor()) < 0) {
		system("/sbin/modprobe pktcdvd");
		misc_minor = get_misc_minor();
	}
	if (misc_minor < 0) {
		fprintf(stderr, "Can't find pktcdvd character device\n");
		return;
	}
	dev = MKDEV(MISC_MAJOR, misc_minor);

	if ((stat(pkt_dev_name(CTL_DEV), &stat_buf) >= 0) &&
	    S_ISCHR(stat_buf.st_mode) && (stat_buf.st_rdev == dev))
		return;			    /* Already set up */

	mkdir(CTL_DIR, 0755);
	unlink(pkt_dev_name(CTL_DEV));
	mknod(pkt_dev_name(CTL_DEV), S_IFCHR | 0644, dev);
}

static int remove_stale_dev_node(int ctl_fd, char *devname)
{
	struct stat stat_buf;
	int i, dev;
	struct pkt_ctrl_command c;

	if (stat(pkt_dev_name(devname), &stat_buf) < 0)
		return 0;
	if (!S_ISBLK(stat_buf.st_mode))
		return 1;
	dev = stat_buf.st_rdev;
	memset(&c, 0, sizeof(struct pkt_ctrl_command));
	for (i = 0; ; i++) {
		c.command = PKT_CTRL_CMD_STATUS;
		c.dev_index = i;
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			perror("ioctl");
			return 1;
		}
		if (i >= c.num_devices)
			break;
		if (c.pkt_dev == dev)
			return 1;	    /* busy */
	}
	unlink(pkt_dev_name(devname));
	return 0;
}

static void setup_dev_chardev(char *pkt_device, char *device, int rem)
{
	struct pkt_ctrl_command c;
	struct stat stat_buf;
	int ctl_fd, dev_fd;

	memset(&c, 0, sizeof(struct pkt_ctrl_command));

	create_ctl_dev();
	if ((ctl_fd = open(pkt_dev_name(CTL_DEV), O_RDONLY)) < 0) {
		perror("ctl open");
		return;
	}

	if (!rem) {
		if ((dev_fd = open(device, O_RDONLY | O_NONBLOCK)) == -1) {
			perror("open cd-rom");
			goto out_close;
		}
		if (init_cdrom(dev_fd)) {
			close(dev_fd);
			goto out_close;
		}
		close(dev_fd);

		if (stat(device, &stat_buf) < 0) {
			perror("stat cd-rom");
			goto out_close;
		}
		if (!S_ISBLK(stat_buf.st_mode)) {
			fprintf(stderr, "Not a block device\n");
			goto out_close;
		}
		c.command = PKT_CTRL_CMD_SETUP;
		c.dev = stat_buf.st_rdev;

		if (remove_stale_dev_node(ctl_fd, pkt_device) != 0) {
			fprintf(stderr, "Device node '%s' already in use\n", pkt_device);
			goto out_close;
		}
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			perror("ioctl");
			goto out_close;
		}
		mknod(pkt_dev_name(pkt_device), S_IFBLK | 0640, c.pkt_dev);
	} else {
		int major, minor, remove_node;

		if ((stat(pkt_dev_name(pkt_device), &stat_buf) >= 0) &&
		    S_ISBLK(stat_buf.st_mode)) {
			major = MAJOR(stat_buf.st_rdev);
			minor = MINOR(stat_buf.st_rdev);
			remove_node = 1;
		} else if (sscanf(pkt_device, "%d:%d", &major, &minor) == 2) {
			remove_node = 0;
		} else {
			fprintf(stderr, "Can't find major/minor numbers\n");
			goto out_close;
		}

		c.command = PKT_CTRL_CMD_TEARDOWN;
		c.pkt_dev = MKDEV(major, minor);
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			perror("ioctl");
			goto out_close;
		}
		if (remove_node)
			unlink(pkt_dev_name(pkt_device));
	}

out_close:
	close(ctl_fd);
}

static void show_mappings(void)
{
	struct pkt_ctrl_command c;
	int ctl_fd, i;

	memset(&c, 0, sizeof(struct pkt_ctrl_command));

	create_ctl_dev();
	if ((ctl_fd = open(pkt_dev_name(CTL_DEV), O_RDONLY)) < 0) {
		perror("ctl open");
		return;
	}

	for (i = 0; ; i++) {
		c.command = PKT_CTRL_CMD_STATUS;
		c.dev_index = i;
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			perror("ioctl");
			goto out_close;
		}
		if (i >= c.num_devices)
			break;
		if (c.dev) {
			printf("%2d : %d:%d -> %d:%d\n",
			       i, MAJOR(c.pkt_dev), MINOR(c.pkt_dev),
			       MAJOR(c.dev), MINOR(c.dev));
		}
	}

out_close:
	close(ctl_fd);
}

int main(int argc, char **argv)
{
	int rem = 0, c;
	char *pkt_device;
	char *device;

	if (argc == 1)
		return usage();

	while ((c = getopt(argc, argv, "ds?")) != EOF) {
		switch (c) {
			case 'd':
				rem = 1;
				break;
			case 's':
				show_mappings();
				return 0;
			default:
				return usage();
		}
	}
	pkt_device = argv[optind];
	device = argv[optind + 1];
	if (strchr(pkt_device, '/'))
		setup_dev(pkt_device, device, rem);
	else
		setup_dev_chardev(pkt_device, device, rem);
	return 0;
}
