/*
 * cdrwtool - perform all sort of actions on a CD-R, CD-RW, and DVD-R drive.
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
 * History:
 * 
 * Ver 0.01 Alpha	99/11/09
 *	- Mostly untested ;)
 *
 * Ver 0.02 Alpha	00/02/02
 *	- addded reserve track option
 *	- progress indicator for format / blank (only when the device
 *	  "disconnects" through the IMMED bit -- see BLOCKING_CMD, and
 *	  if the drive supports the progress through sense stuff). Beware
 *	  that it is pretty crappy...
 *	- print OPC info in print_disc_track_info
 *	- do manual locking/unlocking to prevent lock door errors while
 *	  doing a format/blank with IMMED set.
 *	- added the -q quick setup option.
 *
 * Todo:
 *	- Add the -q quick option and integrate with mkudf.
 *	- Set sensible write speed, don't default to DEFAULT_SPEED.
 *
 * Drives tested and known to work:
 *	- HP 8210i
 *	- Sony CRX100E (without progress indicator)
 *	- Yamaha CRW6416SX (SCSI, no progress indicator)
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include <sys/ioctl.h>

#include <linux/cdrom.h>

#include "cdrwtool.h"
#include "mkudf.h"

#define CDROM_DEVICE	"/dev/scd0"

#define DEFAULT_SPEED	4

/* define this to 0 to make format and blank block until the entire
 * operation has succeeded. otherwise control is returned as soon as
 * the drive has verified the command -- this can be used for polling
 * the device for completion.
 */
#undef NONBLOCKING_OP

static char cdrom_device[NAME_MAX];
static int progress;
static int fd;

static char packet_data[32][2048];
static int last_packet = -1;

extern struct option longoptions[];

#if 1
void dump_buffer(const void *buffer, int size)
{
	unsigned char *ptr = (unsigned char *) buffer;
	int i;

	for (i = 0; i < size; i++)
		printf("%02x ", ptr[i]);
	printf("\n");
}
#endif

int wait_cmd(struct cdrom_generic_command *cgc, unsigned char *buf, int dir)
{
	cgc->buffer = buf;
	cgc->data_direction = dir;
	return ioctl(fd, CDROM_SEND_PACKET, cgc);
}

void sig_progress(int sig)
{
	struct cdrom_generic_command cgc;
	struct request_sense sense;
	int ret;

	memset(&cgc, 0, sizeof(cgc));
	memset(&sense, 0, sizeof(sense));

	cgc.sense = &sense;
	ret = wait_cmd(&cgc, NULL, CGC_DATA_NONE);

	if (!(sense.sks[0] & 0x80)) {
		printf("Progress indicator not implemented on this drive\n");
		printf("Don't access drive until operation has completed\n");
		progress = 101;
		return;
	}

	progress = ((sense.sks[1] << 8 | sense.sks[2]) * 100) / 0xffff;

	printf("%02d%% complete\n", progress);
	if (progress == 99) {
		progress = 101;
		return;
	}
	alarm(2);
}

void print_completion_info(void)
{
	/* we can only poll sense for non-blocking commands */
#ifndef NONBLOCKING_OP
	return;
#endif
	progress = 0;
	signal(SIGALRM, sig_progress);
	alarm(5);
	while (progress < 100)
		sleep(1);
}

/* buffer must already have been filled by mode_sense */
int mode_select(unsigned char *buffer, int len)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	memset(buffer, 0, 3);
	cgc.cmd[0] = GPCMD_MODE_SELECT_10;
	cgc.cmd[1] = 1 << 4;
	cgc.cmd[8] = cgc.buflen = len;

	return wait_cmd(&cgc, buffer, CGC_DATA_WRITE);
}

int mode_sense(unsigned char *buffer, int page, char pc, int size)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_MODE_SENSE_10;
//	cgc.cmd[1] = 1 << 4;
	cgc.cmd[2] = page | pc << 6;
	cgc.cmd[8] = cgc.buflen = size;

	return wait_cmd(&cgc, buffer, CGC_DATA_READ);
}

int set_write_mode(write_params_t *w)
{
	unsigned char header[0x10];
	unsigned char *buffer;
	int ret, len, offset;

	memset(header, 0x00, sizeof(header));

	len = 0x10;

	if ((ret = mode_sense(header, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_DEFAULT, len)) < 0)
	{
		perror("mode_sense_write");
		return ret;
	}

	len = 2 + (((header[0] & 0xff) << 8) | (header[1] & 0xff));
	offset = 8 + (((header[6] & 0xff) << 8) | (header[7] & 0xff));
	buffer = calloc(len, sizeof(unsigned char));

	if ((ret = mode_sense(buffer, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_DEFAULT, len)) < 0)
	{
		perror("mode_sense_write");
		return ret;
	}


	buffer[offset+2] = w->ls_v << 5;
	buffer[offset+3] = w->track_mode | w->fpacket << 5 | w->border << 6;
	buffer[offset+4] = w->data_block & 0xf;
	buffer[offset+5] = w->link_size;
	buffer[offset+8] = w->session_format & 0xff;
	buffer[offset+10] = (w->packet_size >> 24) & 0xff;
	buffer[offset+11] = (w->packet_size >> 16) & 0xff;
	buffer[offset+12] = (w->packet_size >>  8) & 0xff;
	buffer[offset+13] = w->packet_size & 0xff;
	/* sub-header is only a mode-2 thing */
	if (w->data_block == 10)
	{
		buffer[offset+48] = 0x00;
		buffer[offset+49] = 0x00;
		buffer[offset+50] = 0x08;
		buffer[offset+51] = 0x00;
	}

	if ((ret = mode_select(buffer, len)) < 0) {
		dump_buffer(buffer, len);
		free(buffer);
		perror("mode_select");
		return ret;
	}

	free(buffer);
	return 0;
}
	
int get_write_mode(write_params_t *w)
{
	unsigned char header[0x10];
	unsigned char *buffer;
	int ret, len, offset;

	memset(header, 0x00, sizeof(header));

	len = 0x10;

	if ((ret = mode_sense(header, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_CURRENT, len)) < 0)
	{
		perror("mode_sense_write");
		return ret;
	}

	len = 2 + (((header[0] & 0xff) << 8) | (header[1] & 0xff));
	offset = 8 + (((header[6] & 0xff) << 8) | (header[7] & 0xff));
	buffer = calloc(len, sizeof(unsigned char));

	if ((ret = mode_sense(buffer, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_CURRENT, len)) < 0)
	{
		perror("mode_sense_write");
		return ret;
	}

	w->ls_v = (buffer[offset+2] >> 5) & 1;
	w->border = (buffer[offset+3] >> 6) & 3;
	w->fpacket = (buffer[offset+3] >> 5) & 1;
	w->track_mode = buffer[offset+3] & 0xf;
	w->data_block = buffer[offset+4] & 0xf;
	w->link_size = buffer[offset+5];
	w->session_format = buffer[offset+8];
	w->packet_size = buffer[offset+13];

	free(buffer);

	return 0;
}

int sync_cache(void)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = 0x35;
	cgc.cmd[1] = 2;
	return wait_cmd(&cgc, NULL, CGC_DATA_NONE);
}

int write_blocks(char *buffer, int lba, int blocks)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));

	cgc.cmd[0] = GPCMD_WRITE_10;
#if 0
	cgc.cmd[1] = 1 << 3;
#endif
	cgc.cmd[2] = (lba >> 24) & 0xff;
	cgc.cmd[3] = (lba >> 16) & 0xff;
	cgc.cmd[4] = (lba >>  8) & 0xff;
	cgc.cmd[5] = lba & 0xff;
	cgc.cmd[8] = blocks;
	cgc.buflen = blocks * 2048;

#if 0
	dump_buffer(cgc.cmd, 12);
#endif

	return wait_cmd(&cgc, buffer, CGC_DATA_WRITE);
}

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
		if ((err = write_blocks(*packet_data, last_packet * 32, 32)))
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

static void udf_flush_data(void)
{
	write_blocks(*packet_data, last_packet * 32, 32);
}

int write_file(options_t *o)
{
	int file, lba, size, blocks;
	char *buf = NULL;
	int ret = 0, go_on = 1;

	if ((file = open(o->filename, O_RDONLY)) < 0) {
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
		ret = read(file, buf, size);
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
		if ((ret = write_blocks(buf, lba, blocks)))
			break;

		/* sync to indicate that one packet has been sent */
		sync_cache();

		/* for fixed packets, the run-in/run-out blocks are
		 * contained within the packet size. variable packets
		 * don't count them as part of the written size.
		 */
		lba += blocks;
		lba += o->fpacket ? 0 : 7;
	}

	close(file);
	free(buf);
	return ret;
}

int blank_disc(int type)
{
        struct cdrom_generic_command cgc;
	int ret;

        memset(&cgc, 0, sizeof(cgc));
        cgc.cmd[0] = GPCMD_BLANK;
        cgc.cmd[1] = (type == BLANK_FULL ? 0 : 1);
#ifdef NONBLOCKING_OP
	cgc.cmd[1] |= 1 << 4;
#endif

        if ((ret = wait_cmd(&cgc, NULL, CGC_DATA_NONE)) < 0) {
                perror("blank disc");
                return ret;
        }

	print_completion_info();
	return 0;
}

int format_disc(options_t *o)
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
	buffer[1] |= (1 << 1);
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

	if ((ret = wait_cmd(&cgc, buffer, CGC_DATA_WRITE)) < 0) {
		perror("format disc");
		return ret;
	}

	print_completion_info();
	return 0;
}

int read_disc_info(disc_info_t *di)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	memset(di, 0, sizeof(disc_info_t));
	cgc.cmd[0] = GPCMD_READ_DISC_INFO;
	cgc.cmd[8] = cgc.buflen = 2;
	
	if ((ret = wait_cmd(&cgc, (unsigned char *)di, CGC_DATA_READ)) < 0) {
		perror("read disc info");
		return ret;
	}
	cgc.buflen = be16_to_cpu(di->length) + sizeof(di->length);

	if (cgc.buflen > sizeof(disc_info_t))
		cgc.buflen = sizeof(disc_info_t);

	cgc.cmd[8] = cgc.buflen;
	if ((ret = wait_cmd(&cgc, (unsigned char *)di, CGC_DATA_READ)) < 0)
	{
		perror("read disc info");
		return ret;
	}

	return 0;
}

int read_track_info(track_info_t *ti, int trackno)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	memset(ti, 0, sizeof(track_info_t));
	cgc.cmd[0] = GPCMD_READ_TRACK_RZONE_INFO;
	cgc.cmd[1] = 1;
	cgc.cmd[5] = trackno;
	cgc.cmd[8] = cgc.buflen = 28;
	
	if ((ret = wait_cmd(&cgc, (unsigned char *) ti, CGC_DATA_READ)) < 0) {
		perror("read track info");
		return ret;
	}

	return 0;
}

int reserve_track(options_t *o)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_RESERVE_RZONE_TRACK;
	cgc.cmd[5] = (o->reserve_track >> 24) & 0xff;
	cgc.cmd[6] = (o->reserve_track >> 16) & 0xff;
	cgc.cmd[7] = (o->reserve_track >>  8) & 0xff;
	cgc.cmd[8] = o->reserve_track & 0xff;
	
	if ((ret = wait_cmd(&cgc, NULL, CGC_DATA_NONE)) < 0) {
		perror("reserve track");
		return ret;
	}

	return 0;
}

int close_track(unsigned track)
{
	struct cdrom_generic_command cgc;
	int ret;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_CLOSE_TRACK;
#ifndef NONBLOCKING_OP
	cgc.cmd[1] = 1;
#endif
	cgc.cmd[2] = 1; /* bit 2 is close session/border */
	cgc.cmd[4] = (track >> 8) & 0xff;
	cgc.cmd[5] = track & 0xff;

	if ((ret = wait_cmd(&cgc, NULL, CGC_DATA_NONE)) < 0) {
		perror("close track");
		return ret;
	}
	print_completion_info();
	return 0;
}

int read_buffer_cap(options_t *o)
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

	if ((ret = wait_cmd(&cgc, (unsigned char *)&buf, CGC_DATA_READ)))
		return ret;

	o->buffer = be32_to_cpu(buf.buffer_size);

	printf("%uKB internal buffer\n", o->buffer >> 10);
	return 0;
}

int set_cd_speed(int speed)
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

	return wait_cmd(&cgc, NULL, CGC_DATA_NONE);
}

void cdrom_close(void)
{
	/* enable locking */
	if (ioctl(fd, CDROM_LOCKDOOR, 0) < 0)
		printf("can't unlock door\n");

	ioctl(fd, CDROM_SET_OPTIONS, CDO_LOCK);
}

int cdrom_open_check(void)
{
	int ret, attempts = 3;

	while (--attempts) {
		ret = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
		if (ret < 0)
			return ret;
		if (ret == CDS_DISC_OK)
			break;
	}

	if (attempts == 0)
		return 1;

	/* drive should now be ready. check media */
	ret = ioctl(fd, CDROM_DISC_STATUS);
	if (ret == CDS_AUDIO || ret < 0)
		return 1;

	/* disable locking */
	if ((ret = ioctl(fd, CDROM_CLEAR_OPTIONS, CDO_LOCK)) < 0)
		return ret;

	if ((ret == ioctl(fd, CDROM_LOCKDOOR, 1)) < 0) {
		fprintf(stderr, "CD-ROM appears to already be opened\n");
		return 1;
	}

	return 0;
}

int quick_setup(options_t *o)
{
	track_info_t ti;
	int ret;
	unsigned blocks;
	mkudf_options opt;

	printf("Settings for %s:\n", cdrom_device);
	printf("\t%s packets, size %u\n", o->fpacket ? "Fixed" : "Variable",
					  o->packet_size);
	printf("\tMode-%d disc\n", o->write_type);

	printf("\nI'm going to do a quick setup of %s. The disc is going to " \
		"be blanked and formatted with one big track. All data on " \
		"the device will be lost!! Press CTRL-C to cancel now.\n",
		 cdrom_device);

	getchar();

	printf("Initiating quick disc blank\n");

	if ((ret = blank_disc(BLANK_FAST)))
		return ret;

	if ((ret = read_track_info(&ti, 1)) < 0)
		return ret;

	blocks = be32_to_cpu(ti.track_size);
	if (o->fpacket && ti.packet && !ti.fp)
	{
			/* fixed packets format usable blocks */
		blocks = ((blocks + 7) / (o->packet_size + 7)) * o->packet_size;
	}
	printf("Disc capacity is %u blocks (%uKB/%uMB)\n",
		blocks, blocks * 2, blocks / 512);

	memset(&opt, 0x00, sizeof(mkudf_options));
	if (o->fpacket)
	{
		o->offset = blocks;
		printf("Formatting track\n");
		if ((ret = format_disc(o)))
			return ret;
		opt.partition = PT_SPARING;
	}
	else /* Does not work!!! */
	{
		o->reserve_track = blocks;
		printf("Reserving track\n");
		if ((ret = reserve_track(o)))
			return ret;
		opt.partition = PT_VAT;
	}

	opt.device = fd;
	opt.blocksize = 2048;
	opt.blocksize_bits = 11;
	opt.blocks = blocks;
	printf("Writing UDF structures to disc\n");
	mkudf(udf_write_data, &opt);
	udf_flush_data();

	printf("Quick setup complete!\n");
	return 0;
}

int mkudf_session(options_t *o)
{
	mkudf_options opt;
	memset(&opt, 0x00, sizeof(mkudf_options));
	opt.partition = PT_SPARING;
	opt.device = fd;
	opt.blocksize = 2048;
	opt.blocksize_bits = 11;
	opt.blocks = o->offset;
	printf("Writing UDF structures to disc\n");
	mkudf(udf_write_data, &opt);
	udf_flush_data();
	return 0;
}

void print_disc_info(disc_info_t *di)
{
	printf("\terasable : %s\n", di->erasable ? "Yes" : "No");
	printf("\tborder = %d\n", di->border);
	printf("\tDisc status = %d\n", di->status);
	printf("\tnumber of first track = %d\n", di->n_first_track);
	printf("\tnumber of sessions = %d\n", (di->n_sessions_m << 8) | di->n_sessions_l);
	printf("\tnumber of tracks = %d\n", (di->first_track_m << 8) | di->first_track_l);
	printf("\tstatus of last track = %d\n", (di->last_track_m << 8) | di->last_track_l);
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
	printf("\tlast_recorded = %u\n", be32_to_cpu(ti->last_recorded));
	printf("\tfree_blocks = %u\n", be32_to_cpu(ti->free_blocks));
	printf("\tpacket_size = %u\n", be32_to_cpu(ti->packet_size));
	printf("\ttrack_size = %u (%uKB)\n", be32_to_cpu(ti->track_size),
					     be32_to_cpu(ti->track_size) * 2);
}

int print_disc_track_info(void)
{
	int ret, i;
	track_info_t ti;
	disc_info_t di;

	memset(&di, 0, sizeof(disc_info_t));
	memset(&ti, 0, sizeof(track_info_t));

	if ((ret = read_disc_info(&di)) < 0)
		return ret;

	printf("\nDISC INFO:\n");
	print_disc_info(&di);


	/* Assumes no more than 256 tracks ;) */
	printf("TRACK INFO:\n");
	for (i = di.n_first_track; i <= di.last_track_l; i++) {
		if ((ret = read_track_info(&ti, i)) < 0)
			return ret;
		printf("\nTrack %d\n", i);
		print_track_info(&ti);
	}
	
	return 0;
}

void make_write_page(write_params_t *w, options_t *o)
{
	switch (o->write_type)
	{
		case WRITE_MODE1:
		{
			w->data_block = 8;
			w->session_format = 0x00;
			break;
		}
		case WRITE_MODE2:
		{
			w->data_block = 10;
			w->session_format = 0x20;
			break;
		}
	}

	w->packet_size = o->packet_size;
	w->fpacket = o->fpacket;
	w->link_size = o->link_size;
	w->border = o->border;
	w->track_mode = w->track_mode | 0x3;
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
	o->link_size 		= 0; /* jens 7 */
	o->write_type	 	= 1;
	o->blank	 	= 0;
	o->offset	 	= 0;
	o->filename[0]		= 0;
	o->disc_track_info	= 0;
	o->format		= 0;
	o->border		= 0;
	o->speed		= DEFAULT_SPEED;
	o->buffer		= 0;
	o->quick_setup		= 0;
	o->close_track		= 0;
	o->reserve_track	= 0;
	o->mkudf		= 0;
	strcpy(cdrom_device, CDROM_DEVICE);

	while (1){
		c = getopt_long(argc, argv, "r:t:im:u:d:sgqcb:p:z:l:w:f:o:h", longoptions, &res);
		if (c == -1)
			break;

		switch (c) {
			case 'c': {
				o->close_track = 1;
				break;
			}
			case 'q': {
				o->quick_setup = 1;
				break;
			}
			case 'u': {
				o->mkudf = 1;
				o->offset = strtol(optarg, NULL, 10);
				printf("mkudfing %lu blocks\n", o->offset);
				break;
			}
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
			case '?': break;
		}
	}

	return 0;
};

int main(int argc, char **argv)
{
	int ret;
	write_params_t w;
	options_t o;

	memset(&w, 0, sizeof(w));
	memset(&o, 0, sizeof(o));

	/* get command line options */
	if ((ret = get_options(argc, argv, &o)))
		return ret;

	/* open device */
	if ((fd = open(cdrom_device, O_RDONLY | O_NONBLOCK)) < 0) {
		perror("open cdrom device");
		return fd;
	}

	atexit(cdrom_close);

	if ((ret = cdrom_open_check())) {
		fprintf(stderr, "set_options\n");
		return ret;
	}

	if ((ret = read_buffer_cap(&o)))
		return ret;

	if ((ret = get_write_mode(&w))) {
		fprintf(stderr, "get_write\n");
		return ret;
	}

	if ((ret = set_cd_speed(o.speed))) {
		fprintf(stderr, "set speed\n");
		return ret;
	}

	if (o.get_settings || o.disc_track_info) {
		if (o.get_settings)
			print_params(&w);
		if (o.disc_track_info)
			print_disc_track_info();
		return ret;
	}

	/* define write parameters based on command line options */
	make_write_page(&w, &o);

	if ((ret = set_write_mode(&w))) {
		printf("set_write\n");
		return ret;
	}

	if (o.close_track)
		return close_track(o.close_track);

	if (o.quick_setup)
		return quick_setup(&o);

	/* blank disc, if specified */
	if (o.blank)
		return blank_disc(o.blank);

	/* format disc, if specified */
	if (o.format)
		return format_disc(&o);

	if (o.mkudf)
		return mkudf_session(&o);

	/* write file, if specified */
	if (o.filename[0] != 0)
		return write_file(&o);

	if (o.reserve_track)
		return reserve_track(&o);

	close(fd);
	return 0;
}
