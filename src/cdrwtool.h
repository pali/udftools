/*
 * Perform all sort of actions on a CD-R, CD-RW, and DVD-R drive.
 *
 * Copyright Jens Axboe, 1999, 2000
 *
 * Released under the GPL licences.
 *
 */

#ifndef WT_H
#define WT_H

#include <getopt.h>
#include <unistd.h>
#include <endian.h>

#include <asm/param.h>

/*
 * define this to be the default cdrom device
 */
#define CDROM_DEVICE	"/dev/scd1"

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

#ifndef NAME_MAX
#define NAME_MAX	255
#endif

#define DEFAULT_SPEED	12

typedef struct {
	unsigned char	ls_v;			/* link_size valid */
	unsigned char	border;			/* or session */
	unsigned char	fpacket;		/* fixed, variable */
	unsigned char	track_mode;		/* control nibbler, sub q */
	unsigned char	data_block;		/* write type */
	unsigned char	link_size;		/* links loss size */
	unsigned char	session_format;		/* session closure settings */
	unsigned long	packet_size;		/* fixed packet size */
} write_params_t;

typedef struct {
	char		filename[NAME_MAX];	/* file to write */
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
} options_t;

struct option longoptions[] = {
	{ "device",				1, NULL, 'd' },
	{ "set write parameters",		0, NULL, 's' },
	{ "get write parameters",		0, NULL, 'g' },
	{ "blank cdrw disc",			1, NULL, 'b' },
	{ "format cdrw disc",			1, NULL, 'm' },
	{ "run mkudf on track",			1, NULL, 'u' },
	{ "set cd writing speed",		1, NULL, 't' },
	{ "write fixed packets",		1, NULL, 'p' },
	{ "perform quick setup",		0, NULL, 'q' },
	{ "reserve track",			1, NULL, 'r' },
	{ "close track",			1, NULL, 'c' },
	{ "fixed packet size",			1, NULL, 'z' },
	{ "border/session setting",		1, NULL, 'l' },
	{ "write type",				1, NULL, 'w' },
	{ "file to write",			2, NULL, 'f' },
	{ "start at this lba for file write",	1, NULL, 'o' },
	{ "print detailed disc info",		0, NULL, 'i' },
	{ "help",				0, NULL, 'h' },
	{ NULL,					0, NULL,  0  }
};

#ifndef be16_to_cpu
#define be16_to_cpu(x) \
        ((__u16)( \
                (((__u16)(x) & (__u16)0x00ffU) << 8) | \
                (((__u16)(x) & (__u16)0xff00U) >> 8) ))
#endif

#ifndef be32_to_cpu
#define be32_to_cpu(x) \
        ((__u32)( \
                (((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
                (((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
                (((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
                (((__u32)(x) & (__u32)0xff000000UL) >> 24) ))
#endif

typedef struct disc_info {
	__u16 length;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned char reserved1	: 3;
	unsigned char erasable	: 1;
	unsigned char border	: 2;
	unsigned char status	: 2;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned char status	: 2;
	unsigned char border	: 2;
	unsigned char erasable	: 1;
	unsigned char reserved1	: 3;
#else
#error "<bits/endian.h> is wack"
#endif
	__u8 n_first_track;
	__u8 n_sessions_l;
	__u8 first_track_l;
	__u8 last_track_l;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned char did_v	: 1;
	unsigned char dbc_v	: 1;
	unsigned char uru	: 1;
	unsigned char reserved2	: 5;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned char reserved2	: 5;
	unsigned char uru	: 1;
	unsigned char dbc_v	: 1;
	unsigned char did_v	: 1;
#endif
	__u8 disc_type;
	__u8 n_sessions_m;
	__u8 first_track_m;
	__u8 last_track_m;
	__u32 disc_id;
	__u8 lead_in_r;
	__u8 lead_in_m;
	__u8 lead_in_s;
	__u8 lead_in_f;
	__u8 lead_out_r;
	__u8 lead_out_m;
	__u8 lead_out_s;
	__u8 lead_out_f;
	__u8 disc_bar_code[8];
	__u8 reserved3;
	__u8 opc_entries;
} disc_info_t;

typedef struct track_info {
	__u16 info_length;
	__u8 track_number_l;
	__u8 session_number_l;
	__u8 reserved1;
#if __BYTE_ORDER == __BIG_ENDIAN
	__u8 reserved2		: 2;
	__u8 damage		: 1;
	__u8 copy		: 1;
	__u8 track_mode		: 4;
	__u8 rt			: 1;
	__u8 blank		: 1;
	__u8 packet		: 1;
	__u8 fp			: 1;
	__u8 data_mode		: 4;
	__u8 reserved3		: 6;
	__u8 lra_v		: 1;
	__u8 nwa_v		: 1;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	__u8 track_mode		: 4;
	__u8 copy		: 1;
	__u8 damage		: 1;
	__u8 reserved2		: 2;
	__u8 data_mode		: 4;
	__u8 fp			: 1;
	__u8 packet		: 1;
	__u8 blank		: 1;
	__u8 rt			: 1;
	__u8 nwa_v		: 1;
	__u8 lra_v		: 1;
	__u8 reserved3		: 6;
#endif
	__u32 track_start;
	__u32 next_writable;
	__u32 free_blocks;
	__u32 packet_size;
	__u32 track_size;
	__u32 last_recorded;
	__u8 track_number_m;
	__u8 session_number_m;
	__u8 reserved4;
	__u8 reserved5;
} track_info_t;

typedef struct opc_table {
	__u16 speed;
	__u8 opc_value[6];
} opc_table_t;

typedef struct disc_capacity {
	__u32 lba;
	__u32 block_length;
} disc_capacity_t;

#endif /* WT_H */

