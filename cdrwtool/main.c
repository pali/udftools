/*
 * main.c
 *
 * Copyright (c) 2002       Ben Fennema
 * All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include "cdrwtool.h"
#include "options.h"

#include "../mkudffs/defaults.h"
#include "../mkudffs/mkudffs.h"

int write_func(struct udf_disc *disc, struct udf_extent *ext)
{
	static unsigned char *buffer = NULL;
	static size_t bufferlen = 0;
	static int lastpacket = -1;
	int fd = *(int *)disc->write_data;
	size_t offset;
	int packet;
	struct udf_desc *desc;
	struct udf_data *data;

	if (buffer == NULL)
	{
		bufferlen = disc->blocksize * 32;
		buffer = calloc(bufferlen, 1);
	}

	if (ext == NULL)
	{
		if (lastpacket != -1)
		{
			if (write_blocks(fd, buffer, lastpacket * 32, 32) < 0)
				return -1;
		}
	}
	else if (!(ext->space_type & (USPACE|RESERVED)))
	{
		desc = ext->head;
		while (desc != NULL)
		{
			packet = (uint64_t)(ext->start + desc->offset) / 32;
			if (lastpacket != -1 && packet != lastpacket)
			{
				if (write_blocks(fd, buffer, lastpacket * 32, 32) < 0)
					return -1;
				memset(buffer, 0x00, bufferlen);
			}
			data = desc->data;
			offset = ((uint64_t)(ext->start + desc->offset) - (packet * 32)) * disc->blocksize;
			while (data != NULL)
			{
				if (data->length + offset > bufferlen)
				{
					memcpy(buffer + offset, data->buffer, bufferlen - offset);
					if (write_blocks(fd, buffer, packet * 32, 32) < 0)
						return -1;
					memset(buffer, 0x00, bufferlen);
					lastpacket = ++ packet;

					memcpy(buffer, (uint8_t *)data->buffer + (bufferlen - offset), data->length - (bufferlen - offset));
					offset = data->length - (bufferlen - offset);
				}
				else
				{
					lastpacket = packet;
					memcpy(buffer + offset, data->buffer, data->length);
					offset += data->length;
				}

				if (offset == disc->blocksize * 32)
				{
					if (write_blocks(fd, buffer, packet * 32, 32) < 0)
						return -1;
					memset(buffer, 0x00, bufferlen);
					lastpacket = -1;
					offset = 0;
					packet ++;
				}
				data = data->next;
			}
			desc = desc->next;
		}
	}
	return 0;
}

int quick_setup(int fd, struct cdrw_disc *disc, const char *device)
{
	disc_info_t di;
	track_info_t ti;
	int ret, i;
	unsigned blocks;

	printf("Settings for %s:\n", device);
	printf("\t%s packets, size %u\n", disc->fpacket ? "Fixed" : "Variable",
					  disc->packet_size);
	printf("\tMode-%d disc\n", disc->write_type);

	printf( "\nI'm going to do a quick setup of %s. The disc is going to "
		"be blanked and formatted with one big track. All data on "
		"the device will be lost!! Press CTRL-C to cancel now.\n"
		"ENTER to continue.\n",
		device);

	getchar();

	printf("Initiating quick disc blank\n");

	if ((ret = blank_disc(fd, BLANK_FAST)))
		return ret;

	if ((ret = read_disc_info(fd, &di)) < 0)
		return ret;

	if ((ret = read_track_info(fd, &ti, 1)) < 0)
		return ret;

	blocks = msf_to_lba(di.lead_out_m, di.lead_out_s, di.lead_out_f) - 152;
	if (disc->fpacket)
	{
		/* fixed packets format usable blocks */
		blocks = ((blocks + 7) / (disc->packet_size + 7)) * disc->packet_size;
	}
	printf("Disc capacity is %u blocks (%uKB/%uMB)\n",
		blocks, blocks * 2, blocks / 512);

	if (!disc->offset)
		disc->udf_disc.head->blocks = blocks;
	else
		disc->udf_disc.head->blocks = disc->offset;

	disc->udf_disc.blocks = disc->udf_disc.head->blocks;

	if (disc->fpacket)
	{
		if (!disc->offset)
			disc->offset = blocks;
		printf("Formatting track\n");
		if ((ret = format_disc(fd, disc)))
			return ret;
		add_type2_sparable_partition(&disc->udf_disc, 0, 2, 0);
		disc->udf_disc.udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);
	}
	else
	{
		disc->reserve_track = blocks;
		printf("Reserving track\n");
		if ((ret = reserve_track(fd, disc)))
			return ret;
	}

	disc->udf_disc.flags |= FLAG_UNALLOC_BITMAP;
	for (i=0; i<UDF_ALLOC_TYPE_SIZE; i++)
	{
		if (disc->udf_disc.sizing[i].denomSize == 0)
			memcpy(&disc->udf_disc.sizing[i], &default_sizing[DEFAULT_CDRW][i], sizeof(default_sizing[DEFAULT_CDRW][i]));
	}

	split_space(&disc->udf_disc);

	setup_vrs(&disc->udf_disc);
	setup_anchor(&disc->udf_disc);
	setup_partition(&disc->udf_disc);
	setup_vds(&disc->udf_disc);

	dump_space(&disc->udf_disc);

	printf("Writing UDF structures to disc\n");
	write_disc(&disc->udf_disc);
	disc->udf_disc.write(&disc->udf_disc, NULL);
	sync_cache(fd);

	printf("Quick setup complete!\n");
	return 0;
}

int mkudf_session(int fd, struct cdrw_disc *disc)
{
	int i;

	disc->udf_disc.head->blocks = disc->offset;
	disc->udf_disc.blocks = disc->udf_disc.head->blocks;
	add_type2_sparable_partition(&disc->udf_disc, 0, 2, 0);
	disc->udf_disc.udf_pd[0]->accessType = cpu_to_le32(PD_ACCESS_TYPE_REWRITABLE);

	disc->udf_disc.flags |= FLAG_UNALLOC_BITMAP;
	for (i=0; i<UDF_ALLOC_TYPE_SIZE; i++)
	{
		if (disc->udf_disc.sizing[i].denomSize == 0)
			memcpy(&disc->udf_disc.sizing[i], &default_sizing[DEFAULT_CDRW][i], sizeof(default_sizing[DEFAULT_CDRW][i]));
	}

	split_space(&disc->udf_disc);

	setup_vrs(&disc->udf_disc);
	setup_anchor(&disc->udf_disc);
	setup_partition(&disc->udf_disc);
	setup_vds(&disc->udf_disc);

	dump_space(&disc->udf_disc);

	printf("Writing UDF structures to disc\n");
	write_disc(&disc->udf_disc);
	disc->udf_disc.write(&disc->udf_disc, NULL);
	sync_cache(fd);
	return 0;
}

int main(int argc, char *argv[])
{
	struct cdrw_disc disc;
	write_params_t w;
	const char *filename;
	int fd, ret;

	appname = "cdrwtool";

	memset(&disc, 0x00, sizeof(disc));
	cdrw_init_disc(&disc);
	udf_init_disc(&disc.udf_disc);
	udf_set_version(&disc.udf_disc, 0x150);
	disc.udf_disc.flags |= FLAG_BOOTAREA_PRESERVE;
	filename = CDROM_DEVICE;
	parse_args(argc, argv, &disc, &filename);
	if (((fd = open(filename, O_RDWR | O_NONBLOCK)) < 0) &&
		((errno != EROFS) ||
		((fd = open(filename, O_RDONLY | O_NONBLOCK)) < 0)))
	{
		perror("open cdrom device");
		return fd;
	}
	disc.udf_disc.write = write_func;
	disc.udf_disc.write_data = &fd;

	if ((ret = cdrom_open_check(fd)))
	{
		fprintf(stderr, "set_options\n");
		cdrom_close(fd);
		return ret;
	}

	if ((ret = read_buffer_cap(fd, &disc)))
	{
		cdrom_close(fd);
		return ret;
	}

	if ((ret = get_write_mode(fd, &w)))
	{
		fprintf(stderr, "get_write\n");
		cdrom_close(fd);
		return ret;
	}

	if ((ret = set_cd_speed(fd, disc.speed)))
	{
		fprintf(stderr, "set speed\n");
		cdrom_close(fd);
		return ret;
	}

	if (disc.get_settings || disc.disc_track_info)
	{
		if (disc.get_settings)
			print_params(&w);
		if (disc.disc_track_info)
			print_disc_track_info(fd);
		cdrom_close(fd);
		return ret;
	}

	/* define write parameters based on command line options */
	make_write_page(&w, &disc);

	if ((ret = set_write_mode(fd, &w)))
	{
		printf("set_write\n");
		cdrom_close(fd);
		return ret;
	}

	if (disc.close_track)
	{
		ret = close_track(fd, disc.close_track);
		cdrom_close(fd);
		return ret;
	}

	if (disc.close_session)
	{
		ret = close_session(fd, disc.close_session);
		cdrom_close(fd);
		return ret;
	}

	if (disc.quick_setup)
	{
		ret = quick_setup(fd, &disc, filename);
		cdrom_close(fd);
		return ret;
	}

	if (disc.blank)
	{
		ret = blank_disc(fd, disc.blank);
		cdrom_close(fd);
		return ret;
	}

	if (disc.format)
	{
		ret = format_disc(fd, &disc);
		cdrom_close(fd);
		return ret;
	}

	if (disc.mkudf)
	{
		ret = mkudf_session(fd, &disc);
		cdrom_close(fd);
		return ret;
	}

	if (disc.filename)
	{
		ret = write_file(fd, &disc);
		cdrom_close(fd);
		return ret;
	}

	if (disc.reserve_track)
	{
		ret = reserve_track(fd, &disc);
		cdrom_close(fd);
		return ret;
	}

	cdrom_close(fd);
	return 0;
}
