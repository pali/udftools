/*
 * Perform all sort of actions on a CD-R, CD-RW, and DVD-R drive.
 *
 * Copyright Jens Axboe, 1999, 2000
 *
 * Released under the GPL licences.
 *
 */

#ifndef _CDRWTOOL_H
#define _CDRWTOOL_H 1

#include <stdint.h>
#include <linux/cdrom.h>
#include "../include/libudffs.h"

/*
 * define this to be the default cdrom device
 */
#define CDROM_DEVICE	"/dev/scd1"

#ifndef HZ
#define HZ sysconf(_SC_CLK_TCK)
#endif

/*
 * adjust these values if commands are timing out before completion
 */
#define WAIT_PC			(5 * HZ)
#define WAIT_SYNC		(25 * HZ)
#define WAIT_BLANK		(60 * 60 * HZ)

/*
 * define this to 0 to make format and blank block until the entire
 * operation has succeeded. otherwise control is returned as soon as
 * the drive has verified the command -- this can be used for polling
 * the device for completion.
 */
#define USE_IMMED 0

#define PAGE_CURRENT	0
#define PAGE_CHANGE	1
#define PAGE_DEFAULT	2
#define PAGE_SAVE	3

#define BLANK_FULL	1
#define BLANK_FAST	2

#define WRITE_MODE1	1
#define WRITE_MODE2	2	/* mode2, form 1 */

#define CDROM_BLOCK	2048
#define PACKET_BLOCK	32
#define PACKET_SIZE	CDROM_BLOCK*PACKET_BLOCK

#define DEFAULT_SPEED	12

typedef struct
{
	unsigned char	ls_v;			/* link_size valid */
	unsigned char	border;			/* or session */
	unsigned char	fpacket;		/* fixed, variable */
	unsigned char	track_mode;		/* control nibbler, sub q */
	unsigned char	data_block;		/* write type */
	unsigned char	link_size;		/* links loss size */
	unsigned char	session_format;		/* session closure settings */
	unsigned long	packet_size;		/* fixed packet size */
} write_params_t;

struct cdrw_disc
{
	const char *	filename;		/* file to write */
	unsigned long	offset;			/* write file / format */
	unsigned char	get_settings;		/* just print settings */
	unsigned char	set_settings;		/* save settings */
	unsigned char	blank;			/* blank cdrw disc */
	unsigned char	fpacket;		/* fixed/variable packets */
	unsigned char	packet_size;		/* fixed packet size */
	unsigned char	link_size;		/* link loss size */
	unsigned char	write_type;		/* mode1 or mode2 */
	unsigned char	disc_track_info;	/* list disc/track info */
	unsigned char	format;			/* format disc */
	unsigned char	border;			/* border/session */
	unsigned char	speed;			/* writing speed */
	unsigned int	buffer;			/* buffer size */
	unsigned int	reserve_track;		/* reserve a track */
	unsigned char	quick_setup;
	unsigned char	mkudf;
	unsigned int	close_track;
	unsigned int	close_session;
	struct udf_disc	udf_disc;
};

typedef struct disc_info {
	uint16_t length;
#ifdef WORDS_BIGENDIAN
	unsigned char reserved1	: 3;
	unsigned char erasable	: 1;
	unsigned char border	: 2;
	unsigned char status	: 2;
#else
	unsigned char status	: 2;
	unsigned char border	: 2;
	unsigned char erasable	: 1;
	unsigned char reserved1	: 3;
#endif
	uint8_t n_first_track;
	uint8_t n_sessions_l;
	uint8_t first_track_l;
	uint8_t last_track_l;
#ifdef WORDS_BIGENDIAN
	unsigned char did_v	: 1;
	unsigned char dbc_v	: 1;
	unsigned char uru	: 1;
	unsigned char reserved2	: 5;
#else
	unsigned char reserved2	: 5;
	unsigned char uru	: 1;
	unsigned char dbc_v	: 1;
	unsigned char did_v	: 1;
#endif
	uint8_t disc_type;
	uint8_t n_sessions_m;
	uint8_t first_track_m;
	uint8_t last_track_m;
	uint32_t disc_id;
	uint8_t lead_in_r;
	uint8_t lead_in_m;
	uint8_t lead_in_s;
	uint8_t lead_in_f;
	uint8_t lead_out_r;
	uint8_t lead_out_m;
	uint8_t lead_out_s;
	uint8_t lead_out_f;
	uint8_t disc_bar_code[8];
	uint8_t reserved3;
	uint8_t opc_entries;
} disc_info_t;

typedef struct track_info {
	uint16_t info_length;
	uint8_t track_number_l;
	uint8_t session_number_l;
	uint8_t reserved1;
#ifdef WORDS_BIGENDIAN
	uint8_t reserved2		: 2;
	uint8_t damage		: 1;
	uint8_t copy		: 1;
	uint8_t track_mode		: 4;
	uint8_t rt			: 1;
	uint8_t blank		: 1;
	uint8_t packet		: 1;
	uint8_t fp			: 1;
	uint8_t data_mode		: 4;
	uint8_t reserved3		: 6;
	uint8_t lra_v		: 1;
	uint8_t nwa_v		: 1;
#else
	uint8_t track_mode		: 4;
	uint8_t copy		: 1;
	uint8_t damage		: 1;
	uint8_t reserved2		: 2;
	uint8_t data_mode		: 4;
	uint8_t fp			: 1;
	uint8_t packet		: 1;
	uint8_t blank		: 1;
	uint8_t rt			: 1;
	uint8_t nwa_v		: 1;
	uint8_t lra_v		: 1;
	uint8_t reserved3		: 6;
#endif
	uint32_t track_start;
	uint32_t next_writable;
	uint32_t free_blocks;
	uint32_t packet_size;
	uint32_t track_size;
	uint32_t last_recorded;
	uint8_t track_number_m;
	uint8_t session_number_m;
	uint8_t reserved4;
	uint8_t reserved5;
} track_info_t;

typedef struct opc_table {
	uint16_t speed;
	uint8_t opc_value[6];
} opc_table_t;

typedef struct disc_capacity {
	uint32_t lba;
	uint32_t block_length;
} disc_capacity_t;

int msf_to_lba(int, int, int);
void hexdump(const void *, int);
void dump_sense(unsigned char *, struct request_sense *);
int wait_cmd(int, struct cdrom_generic_command *, unsigned char *, int, int);
int mode_select(int, unsigned char *, int);
int mode_sense(int, unsigned char *, int, char, int);
int set_write_mode(int, write_params_t *);
int get_write_mode(int, write_params_t *);
int sync_cache(int);
int write_blocks(int, unsigned char *, int, int);
int write_file(int, struct cdrw_disc *);
int blank_disc(int, int);
int format_disc(int, struct cdrw_disc *);
int read_disc_info(int, disc_info_t *);
int read_track_info(int, track_info_t *, int);
int reserve_track(int, struct cdrw_disc *);
int close_track(int, unsigned int);
int close_session(int, unsigned int);
int read_buffer_cap(int, struct cdrw_disc *);
int set_cd_speed(int, int);
void cdrom_close(int);
int cdrom_open_check(int);
void print_disc_info(disc_info_t *);
void print_track_info(track_info_t *);
int print_disc_track_info(int);
void make_write_page(write_params_t *, struct cdrw_disc *);
void print_params(write_params_t *);
void cdrw_init_disc(struct cdrw_disc *);

#endif /* _CDRWTOOL_H */
