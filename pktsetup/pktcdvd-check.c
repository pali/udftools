/*
 * Copyright (C) 2020-2021  Pali Rohár <pali.rohar@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/cdrom.h>

#include "bswap.h"

int main(int argc, char *argv[]) {
	int fd;
	int ret;
	int quiet;
	char *device;
	struct stat st;
	int capability;
	int mmc_profile;
	unsigned char inquiry[36];
	disc_information discinfo;
	track_information trackinfo;
	struct request_sense sense;
	struct feature_header features;
	struct cdrom_generic_command cgc;

	if (fcntl(0, F_GETFL) < 0 && open("/dev/null", O_RDONLY) < 0)
		_exit(1);
	if (fcntl(1, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);
	if (fcntl(2, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);

	if (argc == 3 && (strcmp(argv[1], "-q") == 0 || strcmp(argv[1], "--quiet") == 0)) {
		quiet = 1;
		device = argv[2];
	} else if (argc == 2) {
		quiet = 0;
		device = argv[1];
	} else {
		printf("pktcdvd-check from " PACKAGE_NAME " " PACKAGE_VERSION "\n");
		printf("Check if medium in optical device can be used by pktcdvd\n");
		printf("Usage: %s [-q|--quiet] device\n", argv[0]);
		return 1;
	}

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		if (!quiet)
			fprintf(stderr, "Error: Cannot open device '%s': %s\n", device, strerror(errno));
		return 1;
	}

	ret = fstat(fd, &st);
	if (ret != 0) {
		if (!quiet)
			fprintf(stderr, "Error: Cannot stat device '%s': %s\n", device, strerror(errno));
		close(fd);
		return 1;
	}

	if (!S_ISBLK(st.st_mode)) {
		if (!quiet)
			fprintf(stderr, "Error: Filepath '%s' is not block device\n", device);
		close(fd);
		return 1;
	}

	/* CDROM_GET_CAPABILITY is not supported by pktcdvd devices therefore this prevent marking pktcdvd devices as compatible */
	capability = ioctl(fd, CDROM_GET_CAPABILITY, NULL);
	if (capability < 0) {
		if (!quiet)
			fprintf(stderr, "Error: Filepath '%s' is not optical device\n", device);
		close(fd);
		return 1;
	}

	if (!(capability & CDC_GENERIC_PACKET)) {
		if (!quiet)
			printf("Optical device '%s' does not support sending generic packets\n", device);
		close(fd);
		return 1;
	}

	memset(&cgc, 0, sizeof(cgc));
	memset(&sense, 0, sizeof(sense));
	memset(&inquiry, 0, sizeof(inquiry));
	cgc.cmd[0] = GPCMD_INQUIRY;
	cgc.cmd[4] = sizeof(inquiry);
	cgc.buffer = inquiry;
	cgc.buflen = sizeof(inquiry);
	cgc.sense = &sense;
	cgc.data_direction = CGC_DATA_READ;
	cgc.quiet = 1;
	cgc.timeout = 500;

	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret != 0) {
		if (!quiet)
			fprintf(stderr, "Error: INQUIRY with size %hhu on device '%s' failed: %s (0x%02X,0x%02X,0x%02X)\n", cgc.cmd[4], device, strerror(errno), sense.sense_key, sense.asc, sense.ascq);
		close(fd);
		return 1;
	}

	if ((inquiry[0] & 0x1F) != 0x05) {
		if (!quiet)
			printf("Optical device '%s' is not MMC-compatible\n", device);
		close(fd);
		return 1;
	}

	memset(&cgc, 0, sizeof(cgc));
	memset(&sense, 0, sizeof(sense));
	memset(&features, 0, sizeof(features));
	cgc.cmd[0] = GPCMD_GET_CONFIGURATION;
	cgc.cmd[8] = sizeof(features);
	cgc.buffer = (unsigned char *)&features;
	cgc.buflen = sizeof(features);
	cgc.sense = &sense;
	cgc.data_direction = CGC_DATA_READ;
	cgc.quiet = 1;
	cgc.timeout = 500;

	/* GET_CONFIGURATION is not supported by pre-MMC2 drives */
	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret == 0)
		mmc_profile = be16_to_cpu(features.curr_profile);
	else
		mmc_profile = -1;

	memset(&cgc, 0, sizeof(cgc));
	memset(&sense, 0, sizeof(sense));
	memset(&discinfo, 0, sizeof(discinfo));
	cgc.cmd[0] = GPCMD_READ_DISC_INFO;
	cgc.cmd[8] = sizeof(discinfo.disc_information_length);
	cgc.buffer = (unsigned char *)&discinfo.disc_information_length;
	cgc.buflen = sizeof(discinfo.disc_information_length);
	cgc.sense = &sense;
	cgc.data_direction = CGC_DATA_READ;
	cgc.quiet = 1;
	cgc.timeout = 500;

	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret != 0) {
		if (!quiet)
			fprintf(stderr, "Error: READ_DISC_INFO with size %hhu on device '%s' failed: %s (0x%02X,0x%02X,0x%02X)\n", cgc.cmd[8], device, strerror(errno), sense.sense_key, sense.asc, sense.ascq);
		close(fd);
		return 1;
	}

	/* Not all drives have the same disc_info length, so requeue packet with the length the drive tells us it can supply */
	cgc.buffer = (unsigned char *)&discinfo;
	cgc.buflen = be16_to_cpu(discinfo.disc_information_length) + sizeof(discinfo.disc_information_length);
	if (cgc.buflen > sizeof(discinfo))
		cgc.buflen = sizeof(discinfo);
	cgc.cmd[8] = cgc.buflen;

	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret != 0) {
		if (!quiet)
			fprintf(stderr, "Error: READ_DISC_INFO with size %hhu on device '%s' failed: %s (0x%02X,0x%02X,0x%02X)\n", cgc.cmd[8], device, strerror(errno), sense.sense_key, sense.asc, sense.ascq);
		close(fd);
		return 1;
	}

	memset(&cgc, 0, sizeof(cgc));
	memset(&sense, 0, sizeof(sense));
	memset(&trackinfo, 0, sizeof(trackinfo));
	cgc.cmd[0] = GPCMD_READ_TRACK_RZONE_INFO;
	cgc.cmd[1] = 1; /* type: 1 */
	cgc.cmd[5] = 1; /* track: 1 */
	cgc.cmd[8] = sizeof(trackinfo.track_information_length);
	cgc.buffer = (unsigned char *)&trackinfo.track_information_length;
	cgc.buflen = sizeof(trackinfo.track_information_length);
	cgc.sense = &sense;
	cgc.data_direction = CGC_DATA_READ;
	cgc.quiet = 1;
	cgc.timeout = 500;

	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret != 0) {
		if (!quiet)
			fprintf(stderr, "Error: READ_TRACK_RZONE_INFO for type %hhu and track %hhu with size %hhu on device '%s' failed: %s (0x%02X,0x%02X,0x%02X)\n", cgc.cmd[1], cgc.cmd[5], cgc.cmd[8], device, strerror(errno), sense.sense_key, sense.asc, sense.ascq);
		close(fd);
		return 1;
	}

	cgc.buffer = (unsigned char *)&trackinfo;
	cgc.buflen = be16_to_cpu(trackinfo.track_information_length) + sizeof(trackinfo.track_information_length);
	if (cgc.buflen > sizeof(trackinfo))
		cgc.buflen = sizeof(trackinfo);
	cgc.cmd[8] = cgc.buflen;

	ret = ioctl(fd, CDROM_SEND_PACKET, &cgc);
	if (ret != 0) {
		if (!quiet)
			fprintf(stderr, "Error: READ_TRACK_RZONE_INFO for type %hhu and track %hhu with size %hhu on device '%s' failed: %s (0x%02X,0x%02X,0x%02X)\n", cgc.cmd[1], cgc.cmd[5], cgc.cmd[8], device, strerror(errno), sense.sense_key, sense.asc, sense.ascq);
		close(fd);
		return 1;
	}

	close(fd);

	/* Supported media types */
	if (mmc_profile != 0x0A && mmc_profile != 0x12 && mmc_profile != 0x13 && mmc_profile != 0x1A && mmc_profile != -1) {
		if (!quiet)
			printf("Optical device '%s' does not contain CD-RW nor DVD-RAM nor DVD-RW nor DVD+RW medium\n", device);
		return 1;
	}

	/* Requirements for CD-RW */
	if (mmc_profile == 0x0A || mmc_profile == -1) {
		if (!discinfo.erasable) {
			if (!quiet)
				printf("%s device '%s' %sis not erasable or the drive is not capable of writing the media\n", (mmc_profile == -1) ? "Optical" : "CD-RW", device, (mmc_profile == -1) ? "is not CD-RW or " : "");
			return 1;
		}
		if (discinfo.disc_type != 0x20 && discinfo.disc_type != 0x00) {
			if (!quiet)
				printf("CD-RW device '%s' is not CD-ROM XA Disc nor CD-ROM Disc compatible\n", device);
			return 1;
		}
		if (discinfo.border_status == 2) {
			if (!quiet)
				printf("CD-RW device '%s' has reserved session\n", device);
			return 1;
		}
	}

	/* Requirements for CD-RW and DVD-RW */
	if (mmc_profile == 0x0A || mmc_profile == -1 || mmc_profile == 0x13) {
		if (!trackinfo.packet) {
			if (!quiet)
				printf("CD-RW/DVD-RW device '%s' is not formatted to packet writing mode\n", device);
			return 1;
		}
		if (!trackinfo.fp) {
			if (!quiet)
				printf("CD-RW/DVD-RW device '%s' is not formatted to overwritable mode\n", device);
			return 1;
		}
		if (trackinfo.rt && trackinfo.blank) {
			if (!quiet)
				printf("CD-RW/DVD-RW device '%s' is blank or unformatted\n", device);
			return 1;
		}
	}

	if (be32_to_cpu(trackinfo.fixed_packet_size) == 0 || be32_to_cpu(trackinfo.fixed_packet_size) >= 128) {
		if (!quiet)
			printf("Optical device '%s' has invalid packet size\n", device);
		return 1;
	}

	if (trackinfo.data_mode != 1 && trackinfo.data_mode != 2) {
		if (!quiet)
			printf("Optical device '%s' is not Mode 1 nor Mode 2\n", device);
		return 1;
	}

	if (!quiet)
		printf("Optical device '%s' is compatible with pktcdvd\n", device);
	return 0;
}
