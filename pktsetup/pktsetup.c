/*
 * Copyright (c) 1999,2000	Jens Axboe <axboe@suse.de>
 * Copyright (c) 2004		Peter Osterlund <petero2@telia.com>
 * Copyright (c) 2014-2017	Pali Rohár <pali.rohar@gmail.com>
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

#include "config.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>

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
		fprintf(stderr, "pktsetup: Error: Drive is not ready\n");
		return 1;
	}

	if (ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT) < 0) {
		fprintf(stderr, "pktsetup: Error: No disc inserted\n");
		return 1;
	}

	/*
	 * we don't care what disc type the uniform layer thinks it
	 * is, since it may get it wrong. what matters is that the above
	 * will force a TOC read.
	 */
	return 0;
}

static int setup_dev(char *pkt_device, char *device, int rem)
{
	int pkt_fd, dev_fd, cmd, arg, ret;

	if ((pkt_fd = open(pkt_device, O_RDONLY)) == -1) {
		fprintf(stderr, "pktsetup: Error: Can't open packet device '%s': %s\n", pkt_device, strerror(errno));
		return 1;
	}

	if (!rem) {
		cmd = PACKET_SETUP_DEV;
		if ((dev_fd = open(device, O_RDONLY | O_NONBLOCK)) == -1) {
			fprintf(stderr, "pktsetup: Error: Can't open cd-rom device '%s': %s\n", device, strerror(errno));
			close(pkt_fd);
			return 1;
		}
		if (init_cdrom(dev_fd)) {
			close(pkt_fd);
			close(dev_fd);
			return 1;
		}
		arg = dev_fd;
	} else {
		cmd = PACKET_TEARDOWN_DEV;
		dev_fd = -1;
		arg = 0;
	}
		
	ret = ioctl(pkt_fd, cmd, arg);
	if (ret == -1) {
		if (rem)
			fprintf(stderr, "pktsetup: Error: Can't tear down packet device '%s': %s\n", pkt_device, strerror(errno));
		else
			fprintf(stderr, "pktsetup: Error: Can't setup packet device '%s' for cd-rom device '%s': %s\n", pkt_device, device, strerror(errno));
	}

	if (dev_fd >= 0)
		close(dev_fd);
	close(pkt_fd);

	if (ret == -1)
		return 1;

	return 0;
}

static int usage(void)
{
	printf("pktsetup from " PACKAGE_NAME " " PACKAGE_VERSION "\n");
	printf("Set up and tear down packet device associations\n");
#if 0
	/* pktcdvd >= 0.2.0 is in linux kernel since 2.6.10, so hide help for old interface */
	printf("For pktcdvd < 0.2.0:\n");
	printf("  pktsetup /dev/pktcdvd0 /dev/cdrom  setup device\n");
	printf("  pktsetup -d /dev/pktcdvd0          tear down device\n");
	printf("For pktcdvd >= 0.2.0:\n");
#endif
	printf("  pktsetup [dev_name] /dev/cdrom     setup device\n");
	printf("  pktsetup [dev_name] major:minor    setup device\n");
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
	while (fscanf(f, " %d %63s", &minor, name) == 2) {
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
	static char buf[512];
	if (snprintf(buf, sizeof(buf), "%s/%s", CTL_DIR, dev) >= (int)sizeof(buf)) {
		fprintf(stderr, "pktsetup: Error: Can't process device '%s': %s\n", dev, strerror(ENAMETOOLONG));
		exit(1);
	}
	return buf;
}

static int create_ctl_dev(void)
{
	int misc_minor;
	struct stat stat_buf;
	dev_t dev;

	if ((misc_minor = get_misc_minor()) < 0) {
		if (system("/sbin/modprobe pktcdvd") != 0) {
			fprintf(stderr, "pktsetup: Error: Can't load pktcdvd kernel module\n");
		}
		misc_minor = get_misc_minor();
	}
	if (misc_minor < 0) {
		fprintf(stderr, "pktsetup: Error: Can't find pktcdvd character device number\n");
		return -1;
	}
	dev = MKDEV(MISC_MAJOR, misc_minor);

	if ((stat(pkt_dev_name(CTL_DEV), &stat_buf) >= 0) &&
	    S_ISCHR(stat_buf.st_mode) && (stat_buf.st_rdev == dev))
		return 0;			    /* Already set up */

	mkdir(CTL_DIR, 0755);
	unlink(pkt_dev_name(CTL_DEV));

	if (mknod(pkt_dev_name(CTL_DEV), S_IFCHR | 0644, dev) < 0) {
		perror("pktsetup: Error: Can't create pktcdvd control device");
		return -1;
	}

	return 0;
}

static int remove_stale_dev_node(int ctl_fd, char *devname)
{
	struct stat stat_buf;
	unsigned int i;
	dev_t dev;
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
			perror("pktsetup: Error: Can't check status of device");
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

static dev_t find_pkdev_for_dev(int ctl_fd, dev_t blk_dev)
{
	struct pkt_ctrl_command c;
	unsigned int i;

	memset(&c, 0, sizeof(struct pkt_ctrl_command));

	for (i = 0; ; i++) {
		c.command = PKT_CTRL_CMD_STATUS;
		c.dev_index = i;
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			perror("pktsetup: Error: Can't get device mapping");
			return 0;
		}
		if (i >= c.num_devices)
			return 0;
		if (c.dev == blk_dev)
			return c.pkt_dev;
	}
	return 0;
}

static int setup_dev_chardev(char *pkt_device, char *device, int rem)
{
	struct pkt_ctrl_command c;
	struct stat stat_buf;
	int ctl_fd, dev_fd;
	int ret = 1;

	memset(&c, 0, sizeof(struct pkt_ctrl_command));

	if (create_ctl_dev() < 0)
		return 1;

	if ((ctl_fd = open(pkt_dev_name(CTL_DEV), O_RDONLY)) < 0) {
		perror("pktsetup: Error: Can't open pktcdvd control device");
		return 1;
	}

	if (!rem) {
		if ((dev_fd = open(device, O_RDONLY | O_NONBLOCK)) == -1) {
			int major, minor, old_errno;
			old_errno = errno;
			if (sscanf(device, "%d:%d", &major, &minor) != 2) {
				fprintf(stderr, "pktsetup: Error: Can't open cd-rom device '%s': %s\n", device, strerror(old_errno));
				goto out_close;
			}
			c.dev = MKDEV(major, minor);
		} else {
			if (fstat(dev_fd, &stat_buf) < 0) {
				fprintf(stderr, "pktsetup: Error: Can't stat cd-rom device '%s': %s\n", device, strerror(errno));
				close(dev_fd);
				goto out_close;
			}
			if (!S_ISBLK(stat_buf.st_mode)) {
				fprintf(stderr, "pktsetup: Error: Node '%s' is not a block device\n", device);
				close(dev_fd);
				goto out_close;
			}
			if (init_cdrom(dev_fd)) {
				close(dev_fd);
				goto out_close;
			}
			close(dev_fd);
			c.dev = stat_buf.st_rdev;
		}

		c.command = PKT_CTRL_CMD_SETUP;

		if (pkt_device && remove_stale_dev_node(ctl_fd, pkt_device) != 0) {
			fprintf(stderr, "pktsetup: Error: Device node '%s' already in use\n", pkt_device);
			goto out_close;
		}
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			fprintf(stderr, "pktsetup: Error: Can't setup packet device for cd-rom device '%s': %s\n", device, strerror(errno));
			goto out_close;
		}
		if (pkt_device && mknod(pkt_dev_name(pkt_device), S_IFBLK | 0640, c.pkt_dev) < 0 && errno != EEXIST) {
			fprintf(stderr, "pktsetup: Error: Can't create device node '%s': %s\n", pkt_device, strerror(errno));
			goto out_close;
		}
		ret = 0;
	} else {
		int major, minor, remove_node;

		if ((stat(pkt_dev_name(pkt_device), &stat_buf) >= 0) &&
		    S_ISBLK(stat_buf.st_mode)) {
			major = MAJOR(stat_buf.st_rdev);
			minor = MINOR(stat_buf.st_rdev);
			if (strncmp(pkt_device, "pktcdvd", sizeof("pktcdvd")-1) == 0)
				remove_node = 0;
			else
				remove_node = 1;
		} else if (sscanf(pkt_device, "%d:%d", &major, &minor) == 2) {
			remove_node = 0;
		} else {
			fprintf(stderr, "pktsetup: Error: Can't find major/minor numbers for packet device '%s'\n", pkt_device);
			goto out_close;
		}

		c.command = PKT_CTRL_CMD_TEARDOWN;
		c.pkt_dev = MKDEV(major, minor);
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			int cur_errno = errno;
			if (cur_errno != ENXIO)
				goto out_error;
			remove_node = 0;
			c.pkt_dev = find_pkdev_for_dev(ctl_fd, MKDEV(major, minor));
			if (!c.pkt_dev)
				goto out_error;
			if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
				cur_errno = errno;
out_error:
				fprintf(stderr, "pktsetup: Error: Can't tear down packet device '%s': %s\n", pkt_device, strerror(cur_errno));
				goto out_close;
			}
		}
		if (remove_node) {
			if (unlink(pkt_dev_name(pkt_device)) != 0)
				fprintf(stderr, "pktsetup: Error: Can't unlink device node '%s': %s\n", pkt_device, strerror(errno));
			else
				ret = 0;
		} else {
			ret = 0;
		}
	}

out_close:
	close(ctl_fd);
	return ret;
}

static int show_mappings(void)
{
	struct pkt_ctrl_command c;
	unsigned int i;
	int ctl_fd;
	int ret;

	ret = 0;
	memset(&c, 0, sizeof(struct pkt_ctrl_command));

	if (create_ctl_dev() < 0)
		return 1;

	if ((ctl_fd = open(pkt_dev_name(CTL_DEV), O_RDONLY)) < 0) {
		perror("pktsetup: Error: Can't open pktcdvd control device");
		return 1;
	}

	for (i = 0; ; i++) {
		c.command = PKT_CTRL_CMD_STATUS;
		c.dev_index = i;
		if (ioctl(ctl_fd, PACKET_CTRL_CMD, &c) < 0) {
			perror("pktsetup: Error: Can't show device mapping");
			ret = 1;
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

	return ret;
}

int main(int argc, char **argv)
{
	int rem = 0, c, old_api;
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
				return show_mappings();
			default:
				return usage();
		}
	}

	if (optind == argc || (!rem && optind+2 < argc) || (rem && optind+1 < argc))
		return usage();

	if (!rem && optind+1 == argc) {
		device = argv[optind];
		pkt_device = NULL;
		old_api = 0;
	} else {
		pkt_device = argv[optind];
		device = rem ? NULL : argv[optind + 1];
		old_api = (strchr(pkt_device, '/') != NULL);
	}

	if (old_api)
		return setup_dev(pkt_device, device, rem);
	else
		return setup_dev_chardev(pkt_device, device, rem);
}
