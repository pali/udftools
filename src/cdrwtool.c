/*
 * cdrwtool - perform all sort of actions on a CD-R, CD-RW, and DVD-R drive.
 *
 * Copyright (c) 1999	Jens Axboe <axboe@image.dk>
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
 * History:
 * 
 * Ver 0.01 Alpha	99/11/09
 *	- Mostly untested ;)
 *
 * Ver 0.02 Alpha	00/02/02
 *	- addded reserve track option
 *	- progress indicator for format / blank (only the device
 *	  "disconnects" through the IMMED bit -- see BLOCKING_CMD)
 *	- print OPC info in print_disc_track_info
 *
 * Todo:
 *	- Some of the source is not endian safe right now. Fix that.
 *	- Add the -q quick option and integrate with mkudf.
 *	- Add progress indicator for blank/format.
 *	- Set sensible write speed, don't default to DEFAULT_SPEED.
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/ioctl.h>

#include <linux/cdrom.h>

#include "cdrwtool.h"
#include "mkudf.h"

#define CDROM_DEVICE	"/dev/hdc"

#define DEFAULT_SPEED	4

/* define this to 0 to make format and blank block until the entire
 * operation has succeeded. otherwise control is returned as soon as
 * the drive has verified the command -- this can be used for polling
 * the device for completion.
 */
#undef NONBLOCKING_OP

static char cdrom_device[NAME_MAX];

extern struct option longoptions[];

#if 1
void dump_buffer(const unsigned char *buffer, int size)
{
	int i;

	for (i = 0; i < size; i++)
		printf("%02x ", buffer[i]);
	printf("\n");
}
#endif

int wait_cmd(int device, struct cdrom_generic_command *cgc,
	     unsigned char *buf)
{
	cgc->buffer = buf;
	return ioctl(device, CDROM_SEND_PACKET, cgc);
}

/* buffer must already have been filled by mode_sense */
int mode_select(int device, unsigned char *buffer)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_MODE_SELECT_10;
	cgc.cmd[1] = 1 << 4;
	cgc.cmd[8] = cgc.buflen = 0x3c;

	return wait_cmd(device, &cgc, buffer);
}

int mode_sense(int device, unsigned char *buffer, int page, char pc, int size)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_MODE_SENSE_10;
//	cgc.cmd[1] = 1 << 4;
	cgc.cmd[2] = page | pc << 6;
	cgc.cmd[8] = cgc.buflen = size;

	return wait_cmd(device, &cgc, buffer);
}

int set_write_mode(int device, write_params_t *w)
{
	unsigned char buffer[0x40];
	int ret;

	memset(buffer, 0, sizeof(buffer));

	if ((ret = mode_sense(device, buffer, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_DEFAULT, 0x3c)) < 0) {
		perror("mode_sense_write");
		return ret;
	}

	buffer[10] = w->ls_v << 5;
	buffer[11] = w->track_mode | w->fpacket << 5 | w->border << 6;
	buffer[12] = w->data_block & 0xf;
	buffer[13] = w->link_size;
	buffer[16] = w->session_format & 0xff;
	buffer[18] = (w->packet_size >> 24) & 0xff;
	buffer[19] = (w->packet_size >> 16) & 0xff;
	buffer[20] = (w->packet_size >>  8) & 0xff;
	buffer[21] = w->packet_size & 0xff;

#if 0
	dump_buffer(buffer, 0x40);
#endif

	if ((ret = mode_select(device, buffer)) < 0) {
		dump_buffer(buffer, 32);
		perror("mode_select");
		return ret;
	}

	return 0;
}
	
int get_write_mode(int device, write_params_t *w)
{
	unsigned char buffer[0x40];
	int ret;

	memset(buffer, 0, sizeof(buffer));

	if ((ret = mode_sense(device, buffer, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_CURRENT, 0x3c)) < 0) {
		perror("mode_sense_write");
		return ret;
	}

	w->ls_v = (buffer[10] >> 5) & 1;
	w->border = (buffer[11] >> 6) & 3;
	w->fpacket = (buffer[11] >> 5) & 1;
	w->track_mode = (buffer[11] & 0xf) | 0x3;
	w->data_block = buffer[12] & 0xf;
	w->link_size = buffer[13];
	w->session_format = buffer[16];
	w->session_format = 0x20;
	w->packet_size = buffer[21];

	return 0;
}

int sync_cache(int device)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = 0x35;
	cgc.cmd[1] = 2;
	cgc.buflen = 12;
	return wait_cmd(device, &cgc, NULL);
}

int write_blocks(int device, char *buffer, int lba, int blocks)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));

	cgc.cmd[0] = GPCMD_WRITE_10;
	cgc.cmd[2] = (lba >> 24) & 0xff;
	cgc.cmd[3] = (lba >> 16) & 0xff;
	cgc.cmd[4] = (lba >>  8) & 0xff;
	cgc.cmd[5] = lba & 0xff;
	cgc.cmd[8] = blocks;
	cgc.buflen = blocks * 2048;

#if 0
	dump_buffer(cgc.cmd, 12);
#endif

	return wait_cmd(device, &cgc, buffer);
}

int write_file(int device, options_t *o)
{
	int fd, lba, size, blocks;
	char *buf = NULL;
	int ret = 0, go_on = 1;

	if ((fd = open(o->filename, O_RDONLY)) < 0) {
		fprintf(stderr, "can't open %s\n", o->filename);
		return 1;
	}

	/* for fixed packets, the write size is set. variable packets,
	 * we write a little less than the buffer capacity. the drive
	 * probably uses some of this for internal housekeeping, and
	 * we want to completely eliminate buffer underruns.
	 */
	size = o->fpacket ? o->packet_size * CDROM_BLOCK : o->buffer - 100 * 1024;
	lba = o->offset;

	buf = (char *) malloc(size+1);
	if (buf == NULL)
		return 1;

	while (!ret && go_on) {
		blocks = o->fpacket ? o->packet_size : size / CDROM_BLOCK;
		ret = read(fd, buf, size);
		if (ret == -1) {
			perror("read from file");
			break;
		} else if (ret < size) {
			/* not enough data to complete the packet. fill
			 * the rest of the data block with zeros. we must
			 * write out complete packets every time with
			 * fixed packets. for variable packets we just
			 * write what we have left.
			 */
			if (o->fpacket) {
				memset(&buf[ret], 0, size - ret - 1);
			} else {
				blocks = (size - ret) / CDROM_BLOCK;
			}
			/* regardless of type, this is the last write */
			go_on = 0;
		}

		fprintf(stdout, "writing at lba = %d, blocks = %d\n", lba, blocks);
		if ((ret = write_blocks(device, buf, lba, blocks)))
			break;

		/* sync to indicate that one packet has been sent */
		sync_cache(device);

		/* for fixed packets, the run-in/run-out blocks are
		 * contained within the packet size. variable packets
		 * don't count them as part of the written size.
		 */
		lba += blocks;
		lba += o->fpacket ? 0 : 7;
	}

	close(fd);
	free(buf);
	return ret;
}

int blank_disc(int device, int type)
{
        struct cdrom_generic_command cgc;
	int ret;

        memset(&cgc, 0, sizeof(cgc));
        cgc.cmd[0] = GPCMD_BLANK;
        cgc.cmd[1] = (type == BLANK_FULL ? 0 : 1);
#ifdef NONBLOCKING_OP
	cgc.cmd[1] |= 1 << 4;
#endif

        if ((ret = wait_cmd(device, &cgc, NULL)) < 0) {
                perror("blank disc");
                return ret;
        }

        return 0;
}

int format_disc(int device, options_t *o)
{
	struct cdrom_generic_command cgc;
	unsigned char buffer[16];
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	memset(buffer, 0, sizeof(buffer));

	cgc.cmd[0] = GPCMD_FORMAT_UNIT;
	cgc.cmd[1] = 1 << 4 | 7;
	cgc.buflen = 16;

	/* format list header */
	buffer[0] = 0;
	buffer[1] = 0;		/* FOV: 0 (use defaults) */
#ifdef NONBLOCKING_OP
	buffer[1] |= 2;
#endif
	buffer[2] = 0;
	buffer[3] = 8;

	/* bytes 4 through 7 are the initialization pattern, which
	 * we just ignore. no use for CD-RW drives.
	 */

	/* format descriptor */
	buffer[8] = 0;		/* session and grow bits (7 and 6) */
	buffer[9] = 0;
	buffer[10] = 0;
	buffer[11] = 0;
	buffer[12] = (o->offset >> 24) & 0xff;
	buffer[13] = (o->offset >> 16) & 0xff;
	buffer[14] = (o->offset >>  8) & 0xff;
	buffer[15] = o->offset & 0xff;

	if ((ret = wait_cmd(device, &cgc, buffer)) < 0) {
		perror("format disc");
		return ret;
	}

	return 0;
}

int read_disc_info(int device, disc_info_t *di)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	memset(di, 0, sizeof(disc_info_t));
	cgc.cmd[0] = GPCMD_READ_DISC_INFO;
	cgc.cmd[8] = cgc.buflen = 2;
	
	if ((ret = wait_cmd(device, &cgc, (unsigned char *)di)) < 0) {
		perror("read disc info");
		return ret;
	}
	cgc.buflen = be16_to_cpu(di->length) + sizeof(di->length);

	if (cgc.buflen > sizeof(disc_info_t))
		cgc.buflen = sizeof(disc_info_t);

	cgc.cmd[8] = cgc.buflen;
	if ((ret = wait_cmd(device, &cgc, (unsigned char *)di)) < 0)
	{
		perror("read disc info");
		return ret;
	}

	return 0;
}

int read_track_info(int device, track_info_t *ti, int trackno)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	memset(ti, 0, sizeof(track_info_t));
	cgc.cmd[0] = GPCMD_READ_TRACK_RZONE_INFO;
	cgc.cmd[1] = 1;
	cgc.cmd[5] = trackno;
	cgc.cmd[8] = cgc.buflen = 28;
	
	if ((ret = wait_cmd(device, &cgc, (unsigned char *) ti)) < 0) {
		perror("read track info");
		return ret;
	}

	return 0;
}

int reserve_track(int device, options_t *o)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_RESERVE_RZONE_TRACK;
	cgc.cmd[5] = (o->reserve_track >> 24) & 0xff;
	cgc.cmd[6] = (o->reserve_track >> 16) & 0xff;
	cgc.cmd[7] = (o->reserve_track >>  8) & 0xff;
	cgc.cmd[8] = o->reserve_track & 0xff;
	
	if ((ret = wait_cmd(device, &cgc, NULL)) < 0) {
		perror("reserve track");
		return ret;
	}

	return 0;
}

int read_buffer_cap(int device, options_t *o)
{
	struct cdrom_generic_command cgc;
	struct {
		unsigned int pad;
		unsigned int buffer_size;
		unsigned int buffer_free;
	} __attribute((packed)) buf;
	int ret;

	memset(&cgc, 0, sizeof(cgc));
	memset(&buf, 0, sizeof(buf));
	cgc.cmd[0] = 0x5c;
	cgc.cmd[8] = cgc.buflen = 12;

	if ((ret = wait_cmd(device, &cgc, (unsigned char *)&buf)))
		return ret;

	o->buffer = be32_to_cpu(buf.buffer_size);

	printf("%uKB internal buffer\n", o->buffer / 1024);
	return 0;
}

int set_cd_speed(int device, int speed)
{
	struct cdrom_generic_command cgc;

	printf("setting write speed to %ux\n", speed);

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = 0xbb;

	/* read speed */
	cgc.cmd[2] = 0xff;
	cgc.cmd[3] = 0xff;

	/* write speed */
	cgc.cmd[4] = ((0xb0 * speed) >> 8) & 0xff;
	cgc.cmd[5] = (0xb0 * speed) & 0xff;

	return wait_cmd(device, &cgc, NULL);
}

void print_completion_info(int device)
{
	/* we can only poll sense for non-blocking commands */
#ifndef NONBLOCKING_OP
	return;
#endif
}

void print_disc_info(disc_info_t *di)
{
	printf("\terasable : %s\n", di->erasable ? "Yes" : "No");
	printf("\tborder = %d\n", di->border);
	printf("\tDisc status = %d\n", di->status);
	printf("\tnumber of first track = %d\n", di->n_first_track);
	printf("\tnumber of sessions = %d\n", (di->n_sessions_m << 8) | di->n_sessions_l);
	printf("\tnumber of tracks = %d\n", (di->first_track_m << 8) | di->first_track_l);
	printf("\tlast track = %d\n", (di->last_track_m << 8) | di->last_track_l);
	printf("\turu = %d\n", di->uru);
	printf("\tdid_v = %d\n", di->did_v);
	printf("\tdbc_v = %d\n", di->dbc_v);
	printf("\tdisc type = %d\n", di->disc_type);
	printf("\tdisc_id = %u\n", be32_to_cpu(di->disc_id));
	printf("\tOPC entries = %u\n", di->opc_entries);
	printf("\n");
}

void print_track_info(track_info_t *ti)
{
	printf("\ttrack_number = %d\n", (ti->track_number_m << 8) | ti->track_number_l);
	printf("\tsession_number = %d\n", (ti->session_number_m << 8) | ti->session_number_l);
	printf("\tdamage = %d\n", ti->damage);
	printf("\tcopy = %d\n", ti->copy);
	printf("\ttrack_mode = %d\n", ti->track_mode);
	printf("\tRt = %d\n", ti->rt);
	printf("\tblank = %d\n", ti->blank);
	printf("\tpacket = %d\n", ti->packet);
	printf("\tfp = %d\n", ti->fp);
	printf("\tdata_mode = %d\n", ti->data_mode);
	printf("\tlra_v = %d\n", ti->lra_v);
	printf("\tnwa_v = %d\n", ti->nwa_v);
	printf("\ttrack_start = %u\n", be32_to_cpu(ti->track_start));
	printf("\tnext_writable = %u\n", be32_to_cpu(ti->next_writable));
	printf("\tfree_blocks = %u\n", be32_to_cpu(ti->free_blocks));
	printf("\tpacket_size = %u\n", be32_to_cpu(ti->packet_size));
	printf("\ttrack_size = %u (%uKB)\n", be32_to_cpu(ti->track_size),
					     be32_to_cpu(ti->track_size) * 2);
}

int print_disc_track_info(int device)
{
	int ret, i;
	track_info_t ti;
	disc_info_t di;

	memset(&di, 0, sizeof(disc_info_t));
	memset(&ti, 0, sizeof(track_info_t));

	if ((ret = read_disc_info(device, &di)) < 0)
		return ret;

	printf("\nDISC INFO:\n");
	print_disc_info(&di);


	/* Assumes no more than 256 tracks ;) */
	printf("TRACK INFO:\n");
	for (i = di.first_track_l; i <= di.last_track_l; i++) {
		if ((ret = read_track_info(device, &ti, i)) < 0)
			return ret;
		printf("\nTrack %d\n", i);
		print_track_info(&ti);
	}
	
	return 0;
}

void make_write_page(write_params_t *w, options_t *o)
{
	switch (o->write_type) {
		case WRITE_MODE1: w->data_block = 8; break;
		case WRITE_MODE2: w->data_block = 10; break;
	}

	w->packet_size = o->packet_size;
	w->fpacket = o->fpacket;
	w->link_size = o->link_size;
	w->border = o->border;
}

void print_params(write_params_t *wp)
{
	fprintf(stdout, "writing %s packets\n", wp->fpacket ? "fixed" : "variable");
	fprintf(stdout, "border type\t: %d\n", wp->border);
	fprintf(stdout, "track mode\t: %d\n", wp->track_mode);
	fprintf(stdout, "data block type\t: %d\n", wp->data_block);
	fprintf(stdout, "session format\t: %d\n", wp->session_format);
	fprintf(stdout, "packet size\t: %lu\n", wp->packet_size);
}

void print_help(void)
{
	int i;
	
	for (i = 0; longoptions[i].name != NULL; i++)
		printf("\t%c\t%s\n", longoptions[i].val, longoptions[i].name);
}

int get_options(int argc, char *argv[], options_t *o)
{
	int c, res;

	/* default options */
	o->get_settings		= 0;
	o->set_settings		= 0;
	o->fpacket 		= 1;
	o->packet_size 		= 32;
	o->link_size 		= 0;
	o->write_type	 	= 1;
	o->blank	 	= 0;
	o->offset	 	= 0;
	o->filename[0]		= 0;
	o->disc_track_info	= 0;
	o->format		= 0;
	o->border		= 0;
	o->speed		= DEFAULT_SPEED;
	o->buffer		= 0;
	strcpy(cdrom_device, CDROM_DEVICE);

	while (1){
		c = getopt_long(argc, argv, "r:t:im:d:sgb:p:z:l:w:f:o:qh", longoptions, &res);
		if (c == -1)
			break;

		switch (c) {
			case 'r': {
				o->reserve_track = strtol(optarg, NULL, 10);
				printf("reserving track %u\n", o->reserve_track);
				break;
			}
			case 't': {
				o->speed = strtol(optarg, NULL, 10);
				printf("setting speed to %d\n", o->speed);
				break;
			}
			case 'm': {
				o->format = 1;
				o->offset = strtol(optarg, NULL, 10);
				printf("formatting %lu blocks\n", o->offset);
				break;
			}
			case 'i': {
				o->disc_track_info = 1;
				break;
			}
			case 'd': {
				strcpy(cdrom_device, optarg);
				printf("using device %s\n", cdrom_device);
				break;
			}
			case 'h': {
				print_help();
				return 1;
			}
			case 'g': {
				printf("ok, want to get\n");
				o->get_settings = 1;
				break;
			}
			case 's': {
				printf("ok, want to set\n");
				o->set_settings = 1;
				break;
			}
			case 'b': {
				if (!strcmp("full", optarg)) {
					printf("full blank\n");
					o->blank = BLANK_FULL;
				} else if (!strcmp("fast", optarg)) {
					printf("fast blank\n");
					o->blank = BLANK_FAST;
				} else {
					printf("full or fast blanking only\n");
					return 1;
				}
				break;
			}
			case 'p': {
				o->fpacket = !!strtol(optarg, NULL, 10);
				printf("%s packets\n", o->fpacket?"fixed":"variable");
				break;
			}
			case 'z': {
				o->packet_size = strtol(optarg, NULL, 10);
				printf("packet size: %d\n", o->packet_size);
				break;
			}
			case 'l': {
				o->border = strtol(optarg, NULL, 10);
				printf("border type: %d\n", o->border);
				break;
			}
			case 'w': {
				if (!strcmp("mode1", optarg)) {
					printf("mode1\n");
					o->write_type = 1;
				} else if (!strcmp("mode2", optarg)) {
					printf("mode2\n");
					o->write_type = 2;
				} else {
					fprintf(stderr, "mode1 or mode2 writing only\n");
					return 1;
				}
				break;
			}
			case 'f': {
				strcpy(o->filename, optarg);
				printf("write file %s\n", o->filename);
				break;
			}
			case 'o': {
				o->offset = strtoul(optarg, NULL, 10);
				printf("write offset %lu\n", o->offset);
				break;
			}
			case 'q': {
				o->quick = 1;
				break;
			}
			case '?': break;
		}
	}

	return 0;
};

char packet_data[32][2048];
int last_packet = -1;

void udf_write_data(mkudf_options *opt, int block, void *buffer, int size, char *type)
{
	int packet = block / 32;
	int err;

	if (last_packet == -1)
	{
		last_packet = packet;
		memset(packet_data, 0x00, 2048*32);
	}

	if (packet != last_packet)
	{
		if ((err = write_blocks(opt->device, *packet_data, last_packet * 32, 32)))
		{
			printf("Error writing packet %d (%x)\n", last_packet, err);
			exit(1);
		}

		memset(packet_data, 0x00, 2048*32);
		memcpy(packet_data[block % 32], buffer, size);
		last_packet = packet;
	}
	else
	{
		memcpy(packet_data[block % 32], buffer, size);
	}
}

static void udf_flush_data(int device)
{
	write_blocks(device, *packet_data, last_packet * 32, 32);
}

int main(int argc, char **argv)
{
	int fd, ret;
	write_params_t w;
	options_t o;

	memset(&w, 0, sizeof(w));
	memset(&o, 0, sizeof(o));

	/* get command line options */
	if (get_options(argc, argv, &o))
		return 1;

	/* open device */
	if ((fd = open(cdrom_device, O_RDONLY | O_NONBLOCK)) == -1) {
		perror("open cdrom device");
		return 1;
	}

	if (read_buffer_cap(fd, &o))
		return 1;

	if ((ret = get_write_mode(fd, &w))) {
		fprintf(stderr, "get_write\n");
		return ret;
	}

	if ((ret = set_cd_speed(fd, o.speed))) {
		fprintf(stderr, "set speed\n");
		return ret;
	}

	if (o.get_settings || o.disc_track_info) {
		if (o.get_settings)
			print_params(&w);
		if (o.disc_track_info)
			print_disc_track_info(fd);
		return 0;
	}

	/* define write parameters based on command line options */
	make_write_page(&w, &o);

	if ((ret = set_write_mode(fd, &w))) {
		printf("set_write\n");
		return ret;
	}

	/* blank disc, if specified */
	if (o.blank)
		if ((ret = blank_disc(fd, o.blank)))
			return ret;

	/* format disc, if specified */
	if (o.format) {
		if (o.offset % (o.packet_size * 2)) {
			printf("format size not multiple of packet size\n");
			return 1;
		}
		if ((ret = format_disc(fd, &o)))
			return ret;
	}

	/* write file, if specified */
	if (o.filename[0] != 0)
	{
		if ((ret = write_file(fd, &o)))
			return ret;
	}
	else if (o.quick && o.format)
	{
		mkudf_options opt;
		memset(&opt, 0x00, sizeof(mkudf_options));
		opt.device = fd;
		opt.partition = PT_SPARING;
		opt.blocksize = 2048;
		opt.blocksize_bits = 11;
		opt.blocks = o.offset;

		mkudf(udf_write_data, &opt);
		udf_flush_data(fd);
	}

	if (o.reserve_track)
		if ((ret == reserve_track(fd, &o)))
			return ret;

	if (o.format || o.blank || o.reserve_track)
		print_completion_info(fd);

	close(fd);
	return 0;
}
