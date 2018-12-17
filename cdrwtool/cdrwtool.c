/*
 * cdrwtool - perform all sort of actions on a CD-R, CD-RW, and DVD-R drive.
 *
 * Copyright (c) 1999,2000	Jens Axboe <axboe@suse.de>
 * Copyright (c) 2002           Ben Fennema
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <limits.h>

#include <sys/ioctl.h>
#include <asm/param.h>

#include <linux/cdrom.h>

#include "cdrwtool.h"
#include "../mkudffs/mkudffs.h"

static int progress;

int msf_to_lba(int m, int s, int f)
{
	return (((m * 60) + s) * 75) + f;
}

void hexdump(const void *buffer, int size)
{
	unsigned char *ptr = (unsigned char *) buffer;
	int i;

	for (i = 0; i < size; i++)
		printf("%02x ", ptr[i]);
	printf("\n");
}

void dump_sense(unsigned char *cdb, struct request_sense *sense)
{
	int i;

	printf("Command failed: ");

	for (i=0; i<12; i++)
		printf("%02x ", cdb[i]);

	if (sense) {
		printf("- sense %02x.%02x.%02x\n", sense->sense_key, sense->asc,
						sense->ascq);
	} else {
		printf(", no sense\n");
	}
}

int wait_cmd(int fd, struct cdrom_generic_command *cgc, unsigned char *buf,
			 int dir, int timeout)
{
	struct request_sense sense;
	int ret;

	if (cgc->timeout <= 0)
		cgc->timeout = 500;

	memset(&sense, 0, sizeof(sense));

	cgc->buffer = buf;
	cgc->data_direction = dir;
	cgc->sense = &sense;
	cgc->timeout = timeout;

	ret = ioctl(fd, CDROM_SEND_PACKET, cgc);
	if (ret)
	{
		perror("wait_cmd");
		dump_sense(cgc->cmd, cgc->sense);
	}
	return ret;
}

int sigfd;

void sig_progress(int sig)
{
	struct cdrom_generic_command cgc;
	struct request_sense sense;
	static int did = 0;
	int ret;

	if (sig != SIGALRM)
		return;

	memset(&cgc, 0, sizeof(cgc));
	memset(&sense, 0, sizeof(sense));

	cgc.sense = &sense;
	ret = wait_cmd(sigfd, &cgc, NULL, CGC_DATA_NONE, WAIT_PC);

	if ((ret || !(sense.sks[0] & 0x80)) && !did) {
		printf("Progress indicator not implemented on this drive\n");
		printf("Don't access drive until operation has completed\n");
		progress = 101;
		return;
	}

	progress = ((sense.sks[1] << 8 | sense.sks[2]) * 100) / 0xffff;
	did = 1;

	printf("%02d%% complete\n", progress);
	if (progress == 99) {
		progress = 101;
		return;
	}
	alarm(2);
}

void print_completion_info(int fd)
{
	sigfd = fd;
	/* we can only poll sense for non-blocking commands */
#if USE_IMMED == 0
	return;
#else
	progress = 0;
	signal(SIGALRM, sig_progress);
	alarm(5);
	while (progress < 100)
		sleep(1);
#endif
}

/* buffer must already have been filled by mode_sense */
int mode_select(int fd, unsigned char *buffer, int len)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	memset(buffer, 0, 3);
	cgc.cmd[0] = GPCMD_MODE_SELECT_10;
	cgc.cmd[1] = 1 << 4;
	cgc.cmd[8] = cgc.buflen = len;

	return wait_cmd(fd, &cgc, buffer, CGC_DATA_WRITE, WAIT_PC);
}

int mode_sense(int fd, unsigned char *buffer, int page, char pc, int size)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_MODE_SENSE_10;
//	cgc.cmd[1] = 1 << 4;
	cgc.cmd[2] = page | pc << 6;
	cgc.cmd[8] = cgc.buflen = size;

	return wait_cmd(fd, &cgc, buffer, CGC_DATA_READ, WAIT_PC);
}

int set_write_mode(int fd, write_params_t *w)
{
	unsigned char header[0x10];
	unsigned char *buffer;
	int ret, len, offset;

	memset(header, 0x00, sizeof(header));

	len = 0x10;

	if ((ret = mode_sense(fd, header, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_DEFAULT, len)) < 0)
	{
		perror("mode_sense_write");
		return ret;
	}

	len = 2 + (((header[0] & 0xff) << 8) | (header[1] & 0xff));
	offset = 8 + (((header[6] & 0xff) << 8) | (header[7] & 0xff));
	buffer = calloc(len, sizeof(unsigned char));

	if ((len <= offset+13) || (w->data_block == 10 && len <= offset+51))
	{
		perror("mode_sense_write");
		free(buffer);
		return ret;
	}

	if ((ret = mode_sense(fd, buffer, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_DEFAULT, len)) < 0)
	{
		perror("mode_sense_write");
		free(buffer);
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

	if ((ret = mode_select(fd, buffer, len)) < 0) {
		hexdump(buffer, len);
		free(buffer);
		perror("mode_select");
		return ret;
	}

	free(buffer);
	return 0;
}
	
int get_write_mode(int fd, write_params_t *w)
{
	unsigned char header[0x10];
	unsigned char *buffer;
	int ret, len, offset;

	memset(header, 0x00, sizeof(header));

	len = 0x10;

	if ((ret = mode_sense(fd, header, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_CURRENT, len)) < 0)
	{
		perror("mode_sense_write");
		return ret;
	}

	len = 2 + (((header[0] & 0xff) << 8) | (header[1] & 0xff));
	offset = 8 + (((header[6] & 0xff) << 8) | (header[7] & 0xff));
	buffer = calloc(len, sizeof(unsigned char));

	if (len <= offset+13)
	{
		perror("mode_sense_write");
		free(buffer);
		return ret;
	}

	if ((ret = mode_sense(fd, buffer, GPMODE_WRITE_PARMS_PAGE,
			      PAGE_CURRENT, len)) < 0)
	{
		perror("mode_sense_write");
		free(buffer);
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

int sync_cache(int fd)
{
	struct cdrom_generic_command cgc;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = 0x35;
	cgc.cmd[1] = 2;
	return wait_cmd(fd, &cgc, NULL, CGC_DATA_NONE, WAIT_SYNC);
}

int write_blocks(int fd, unsigned char *buffer, int lba, int blocks)
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
	cgc.cmd[7] = (blocks >> 8) & 0xff;
	cgc.cmd[8] = blocks & 0xff;
	cgc.buflen = blocks * CDROM_BLOCK;

	return wait_cmd(fd, &cgc, buffer, CGC_DATA_WRITE, WAIT_SYNC);
}

int write_file(int fd, struct cdrw_disc *disc)
{
	int file, lba, size, blocks;
	unsigned char *buf = NULL;
	int ret = 0, go_on = 1;

	if ((file = open(disc->filename, O_RDONLY)) < 0) {
		fprintf(stderr, "can't open %s\n", disc->filename);
		return 1;
	}

	/* for fixed packets, the write size is set. variable packets,
	 * we write a little less than the buffer capacity. the drive
	 * probably uses some of this for internal housekeeping, and
	 * we want to completely eliminate buffer underruns.
	 */
	size = disc->fpacket ? disc->packet_size * CDROM_BLOCK : 63 * CDROM_BLOCK;
	lba = disc->offset;

	buf = (unsigned char *) malloc(size+1);
	if (buf == NULL) {
		perror("malloc");
		close(file);
		return 1;
	}

	while (!ret && go_on) {
		blocks = disc->fpacket ? disc->packet_size : size / CDROM_BLOCK;
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
			if (disc->fpacket) {
				memset(&buf[ret], 0, size - ret - 1);
			} else {
				blocks = (ret + CDROM_BLOCK - 1) / CDROM_BLOCK;
			}
			/* regardless of type, this is the last write */
			go_on = 0;
		}

		fprintf(stdout, "writing at lba = %d, blocks = %d\n", lba, blocks);
		if ((ret = write_blocks(fd, buf, lba, blocks)))
			break;

		/* sync to indicate that one packet has been sent */
//		sync_cache(fd);

		/* for fixed packets, the run-in/run-out blocks are
		 * contained within the packet size. variable packets
		 * don't count them as part of the written size.
		 */
		lba += blocks;
//		lba += disc->fpacket ? 0 : 7;
	}
	sync_cache(fd);

	close(file);
	free(buf);
	return ret;
}

int blank_disc(int fd, int type)
{
	struct cdrom_generic_command cgc;
	int ret;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_BLANK;
	cgc.cmd[1] = (type == BLANK_FULL ? 0 : 1);
	cgc.cmd[1] |= (USE_IMMED << 4);

	if ((ret = wait_cmd(fd, &cgc, NULL, CGC_DATA_NONE, WAIT_BLANK)) < 0)
	{
		perror("blank disc");
		 return ret;
	}

	print_completion_info(fd);
	return 0;
}

int format_disc(int fd, struct cdrw_disc *disc)
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
	buffer[1] |= (USE_IMMED << 1);
	buffer[2] = 0;
	buffer[3] = 8;

	/* bytes 4 through 7 are the initialization pattern, which
	 * we just ignore. no use for CD-RW drives.
	 */

	/* format descriptor */
	buffer[8] = 0x00;		/* session and grow bits (7 and 6) */
	buffer[9] = 0;
	buffer[10] = 0;
	buffer[11] = 0;
	buffer[12] = (disc->offset >> 24) & 0xff;
	buffer[13] = (disc->offset >> 16) & 0xff;
	buffer[14] = (disc->offset >>  8) & 0xff;
	buffer[15] = disc->offset & 0xff;

	if ((ret = wait_cmd(fd, &cgc, buffer, CGC_DATA_WRITE, WAIT_BLANK)) < 0)
	{
		perror("format disc");
		return ret;
	}

	print_completion_info(fd);
	return 0;
}

int read_disc_info(int fd, disc_info_t *di)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	memset(di, 0, sizeof(disc_info_t));
	cgc.cmd[0] = GPCMD_READ_DISC_INFO;
	cgc.cmd[8] = cgc.buflen = 2;
	
	if ((ret = wait_cmd(fd, &cgc, (unsigned char *)di, CGC_DATA_READ, WAIT_PC)) < 0)
	{
		perror("read disc info");
		return ret;
	}
	cgc.buflen = be16_to_cpu(di->length) + sizeof(di->length);

	if (cgc.buflen > sizeof(disc_info_t))
		cgc.buflen = sizeof(disc_info_t);

	cgc.cmd[8] = cgc.buflen;
	if ((ret = wait_cmd(fd, &cgc, (unsigned char *)di, CGC_DATA_READ, WAIT_PC)) < 0)
	{
		perror("read disc info");
		return ret;
	}

	return 0;
}

int read_track_info(int fd, track_info_t *ti, int trackno)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	memset(ti, 0, sizeof(track_info_t));
	cgc.cmd[0] = GPCMD_READ_TRACK_RZONE_INFO;
	cgc.cmd[1] = 1;
	cgc.cmd[5] = trackno;
	cgc.cmd[8] = cgc.buflen = 28;
	
	if ((ret = wait_cmd(fd, &cgc, (unsigned char *) ti, CGC_DATA_READ, WAIT_PC)) < 0)
	{
		perror("read track info");
		return ret;
	}

	return 0;
}

int reserve_track(int fd, struct cdrw_disc *disc)
{
	struct cdrom_generic_command cgc;
	int ret;
	
	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_RESERVE_RZONE_TRACK;
	cgc.cmd[5] = (disc->reserve_track >> 24) & 0xff;
	cgc.cmd[6] = (disc->reserve_track >> 16) & 0xff;
	cgc.cmd[7] = (disc->reserve_track >>  8) & 0xff;
	cgc.cmd[8] = disc->reserve_track & 0xff;
	
	if ((ret = wait_cmd(fd, &cgc, NULL, CGC_DATA_NONE, WAIT_BLANK)) < 0)
	{
		perror("reserve track");
		return ret;
	}

	return 0;
}

int close_track(int fd, unsigned int track)
{
	struct cdrom_generic_command cgc;
	int ret;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_CLOSE_TRACK;
	cgc.cmd[1] = USE_IMMED;
	cgc.cmd[2] = 1; /* bit 2 is close session/border */
	cgc.cmd[4] = (track >> 8) & 0xff;
	cgc.cmd[5] = track & 0xff;

	if ((ret = wait_cmd(fd, &cgc, NULL, CGC_DATA_NONE, WAIT_BLANK)) < 0)
	{
		perror("close track");
		return ret;
	}
	print_completion_info(fd);
	return 0;
}

int close_session(int fd, unsigned int track)
{
	struct cdrom_generic_command cgc;
	int ret;

	memset(&cgc, 0, sizeof(cgc));
	cgc.cmd[0] = GPCMD_CLOSE_TRACK;
	cgc.cmd[1] = USE_IMMED;
	cgc.cmd[2] = 2; /* bit 2 is close session/border */
	cgc.cmd[4] = (track >> 8) & 0xff;
	cgc.cmd[5] = track & 0xff;

	if ((ret = wait_cmd(fd, &cgc, NULL, CGC_DATA_NONE, WAIT_BLANK)) < 0)
	{
		perror("close session");
		return ret;
	}
	print_completion_info(fd);
	return 0;
}

int read_buffer_cap(int fd, struct cdrw_disc *disc)
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

	if ((ret = wait_cmd(fd, &cgc, (unsigned char *)&buf, CGC_DATA_READ, WAIT_PC)))
		return ret;

	disc->buffer = be32_to_cpu(buf.buffer_size);

	printf("%uKB internal buffer\n", disc->buffer >> 10);
	return 0;
}

int set_cd_speed(int fd, int speed)
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

	return wait_cmd(fd, &cgc, NULL, CGC_DATA_NONE, WAIT_PC);
}

void cdrom_close(int fd)
{
	/* enable locking */
	if (ioctl(fd, CDROM_LOCKDOOR, 0) < 0)
		printf("can't unlock door\n");

	ioctl(fd, CDROM_SET_OPTIONS, CDO_LOCK);
}

int cdrom_open_check(int fd)
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

	if ((ret = ioctl(fd, CDROM_LOCKDOOR, 1)) < 0) {
		fprintf(stderr, "CD-ROM appears to already be opened\n");
		return 1;
	}

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
	printf("\tlead_in = %02d:%02d:%02d (%u)\n",
		di->lead_in_m,
		di->lead_in_s,
		di->lead_in_f,
		msf_to_lba(di->lead_in_m, di->lead_in_s, di->lead_in_f));
	printf("\tlead_out = %02d:%02d:%02d (%u)\n",
		di->lead_out_m,
		di->lead_out_s,
		di->lead_out_f,
		msf_to_lba(di->lead_out_m, di->lead_out_s, di->lead_out_f));
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

int print_disc_track_info(int fd)
{
	int ret, i;
	track_info_t ti;
	disc_info_t di;

	memset(&di, 0, sizeof(disc_info_t));
	memset(&ti, 0, sizeof(track_info_t));

	if ((ret = read_disc_info(fd, &di)) < 0)
		return ret;

	printf("\nDISC INFO:\n");
	print_disc_info(&di);


	/* Assumes no more than 256 tracks ;) */
	printf("TRACK INFO:\n");
	for (i = di.n_first_track; i <= di.last_track_l; i++) {
		if ((ret = read_track_info(fd, &ti, i)) < 0)
			return ret;
		printf("\nTrack %d\n", i);
		print_track_info(&ti);
	}
	
	return 0;
}

void make_write_page(write_params_t *w, struct cdrw_disc *disc)
{
	switch (disc->write_type)
	{
		case WRITE_MODE1:
		{
			w->data_block = 0x8;
			w->session_format = 0x00;
			break;
		}
		case WRITE_MODE2:
		{
			w->data_block = 0xa;
			w->session_format = 0x20;
			break;
		}
	}

	w->packet_size = disc->packet_size;
	w->fpacket = disc->fpacket;
	w->link_size = disc->link_size;
	w->border = disc->border;
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

void cdrw_init_disc(struct cdrw_disc *disc)
{
	disc->fpacket		= 1;
	disc->packet_size	= PACKET_BLOCK;
	disc->write_type	= WRITE_MODE2;
	disc->speed		= DEFAULT_SPEED;
}
