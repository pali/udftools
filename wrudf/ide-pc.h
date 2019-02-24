/*	ide-pc.h
 *
 * PURPOSE
 *	Header file for programs using ide-pc.c defined routines   
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 *	(C) 2001 Enno Fennema
 *
 * HISTORY
 *  16 Aug 01  ef  Created.
 *
 */

#ifndef	_IDE_PC_H
#define	_IDE_PC_H

#include <sys/types.h>

struct cdrom_inquiry {
	u_char	type		: 5;	/*  0 */
#define INQ_WORM 	4
#define INQ_CDROM	5
	u_char	qualifier	: 3;	/*  0 */

	u_char	type_modifier	: 7;	/*  1 */
	u_char	removable	: 1;	/*  1 */

	u_char	ansi_version	: 3;	/*  2 */
	u_char	ecma_version	: 3;	/*  2 */
	u_char	iso_version	: 2;	/*  2 */

	u_char	data_format	: 4;	/*  3 */
	u_char	res3_54		: 2;	/*  3 */
	u_char	termiop		: 1;	/*  3 */
	u_char	aenc		: 1;	/*  3 */

	u_char	add_len		: 8;	/*  4 */
	u_char	sense_len	: 8;	/*  5 */ /* only Emulex ??? */
	u_char	res2		: 8;	/*  6 */

	u_char	softreset	: 1;	/*  7 */
	u_char	cmdque		: 1;
	u_char	res7_2		: 1;
	u_char	linked		: 1;
	u_char	sync		: 1;
	u_char	wbus16		: 1;
	u_char	wbus32		: 1;
	u_char	reladr		: 1;	/*  7 */

	char	vendor_info[8];		/*  8 */
	char	prod_ident[16];		/* 16 */
	char	prod_revision[4];	/* 32 */
	char	vendor_uniq[20];	/* 36 */
	char	reserved[40];		/* 56 to 96 */
};

struct cdrom_discinfo {
    u_short	data_length;
    u_char	disc_status	:2;
#define	DS_EMPTY	0	/* Empty disk				*/
#define	DS_APPENDABLE	1	/* Incomplete disk (appendable)		*/
#define	DS_COMPLETE	2	/* Complete disk (closed/no B0 pointer)	*/
    u_char	session_status	:2;
#define	SS_EMPTY	0	/* Empty session			*/
#define	SS_APPENDABLE	1	/* Incomplete session			*/
#define	SS_COMPLETE	3	/* Complete session (needs DS_COMPLETE)	*/
    u_char	erasable:1;
    u_char	_r1	:3;

    u_char	trk0;
    u_char	nsessions;
    u_char	trk0_lastsession;
    u_char	trk1_lastsession;
    u_char	_r4	:5;
    u_char	uru	:1;
    u_char	dbc_v	:1;
    u_char	did_v	:1;
    u_char	disc_type;
    u_char	_r9;
    u_char	_r10;
    u_char	_r11;
    u_int	disc_id;
    u_char	latest_leadin_msf[4];
    u_char	last_leadout_msf[4];
    u_char	barcode[8];
    u_char	_r32;
    u_char	n_opc;
};

struct cdrom_trackinfo {
    u_short	data_length;
    u_char	trk;
    u_char	session;
    u_char	_r4;
    u_char	trk_mode	:4;
    u_char	copy		:1;
    u_char	damage		:1;
    u_char	_r5		:2;
    u_char	data_mode	:4;
    u_char	fixpkt		:1;
    u_char	packet		:1;
    u_char	blank		:1;
    u_char	rsrvd_trk	:1;
    u_char	nwa_v	:1;
    u_char	_r7	:7;
    u_int	trk_start;
    u_int	nwa;
    u_int	free_blks;
    u_int	fixpkt_size;
    u_int	trk_size;
};

struct cdrom_buffercapacity {
    u_short	dataLength;
    u_short	rsrvd;
    u_int	totalBufferLength;
    u_int	freeBufferLength;
};

struct cdrom_reccapacity {
    u_int		lastblock;
    u_int		blocksize;
};

struct cdrom_header {
    u_char	datamode;
    u_char	_r234[3];
    int		lba;
};

#define PGCTL_CURRENT_VALUES	0
#define PGCTL_CHANGE_MASK	1
#define PGCTL_DEFAULT_VALUES	2
#define PGCTL_SAVED_VALUES	3	

typedef struct mode_header {
    u_short	data_length;			// length excl. this field
    u_char	medium_type;
    u_char	dev_specific;
    u_short	r1;
    u_short	block_desc_length;		// 0
}   mode_hdr;

#define GPMODE_READ_ERROR_RECOV_PAGE		0x01	// Read/Write Error Recov in Mt Fuji version

struct read_error_recovery_params {
    u_char	page_code	:6;		// 07h
    u_char	_r1		:1;
    u_char	ps		:1;
    u_char	length;
    u_char	error_recovery_param;
    u_char	retry_count;
    u_char	_r2[4];
};

#define GPMODE_CACHE_PAGE			0x08	// Cache control page

struct cdrom_cacheparams {
    u_char	page_code	:6;		// 08h
    u_char	_r1		:1;
    u_char	ps		:1;
    u_char	length;
    u_char	rcd		:1;
    u_char	mf		:1;
    u_char	wce		:1;
    u_char	_r2		:5;
    u_short	dpftl;
    u_short	minpf;
    u_short	maxpf;
    u_short	maxpfceiling;
};


struct cdrom_writeparams {
    u_char	page_code	:6;
    u_char	_r1		:1;
    u_char	ps		:1;
    u_char	page_length;

    u_char	write_type	:4;
    u_char	test_write	:1;
    u_char	_r2		:3;

    u_char	trk_mode	:4;
    u_char	copy		:1;
    u_char	fp		:1;
    u_char	multi_session	:2;
#define	MS_NONE		0	/* No B0 pointer. Next session not allowed*/
#define	MS_FINAL	1	/* B0 = FF:FF:FF. Next session not allowed*/
#define	MS_MULTI	3	/* B0 = Next PA.  Next session allowed	  */

    u_char	data_blk_type	:4;
#define	DB_RAW		0	/* 2352 bytes of raw data		  */
#define	DB_RAW_PQ	1	/* 2368 bytes (raw data + P/Q Subchannel) */
#define	DB_RAW_PW	2	/* 2448 bytes (raw data + P-W Subchannel) */
#define	DB_RAW_PW_R	3	/* 2448 bytes (raw data + P-W raw Subchannel)*/
#define	DB_RES_4	4	/* -	Reserved			  */
#define	DB_RES_5	5	/* -	Reserved			  */
#define	DB_RES_6	6	/* -	Reserved			  */
#define	DB_VU_7		7	/* -	Vendor specific			  */
#define	DB_ROM_MODE1	8	/* 2048 bytes Mode 1 (ISO/IEC 10149)	  */
#define	DB_ROM_MODE2	9	/* 2336 bytes Mode 2 (ISO/IEC 10149)	  */
#define	DB_XA_F1	10	/* 2048 bytes Mode 2? (CD-ROM XA form 1)  */
#define	DB_XA_F1_8H	11	/* 2056 bytes Mode 2 (CD-ROM XA form 1) + 8 header bytes	  */
#define	DB_XA_F2	12	/* 2324 bytes Mode 2 (CD-ROM XA form 2)	  */
#define	DB_XA_MODE2_MIX	13	/* 2332 bytes Mode 2 (CD-ROM XA 1/2+subhdr) */
#define	DB_RES_14	14	/* -	Reserved			  */
#define	DB_VU_15	15	/* -	Vendor specific			  */
    u_char	_r4		:4;

    u_char	_r5;
    u_char	_r6;
    u_char	host_appl_code	:6;
    u_char	_r7		:2;
    u_char	session_format;
    u_char	_r9;
    u_int	pkt_size __attribute__ ((packed));
    u_short	audio_pause;
    u_char	mcn[16];
    u_char	isrc[16];
    u_char	subhdr0;
    u_char	subhdr1;
    u_char	subhdr2;
    u_char	subhdr3;
};

struct cdrom_capabilities {		/* CD Cap / mech status */
    u_char	p_code		: 6;
    u_char	p_res		: 1;
    u_char	parsave		: 1;

    u_char	p_len;			/* 0x14 = 20 Bytes */

    u_char	cd_r_read	: 1;	/* Reads CD-R  media		     */
    u_char	cd_rw_read	: 1;	/* Reads CD-RW media		     */
    u_char	method2		: 1;	/* Reads fixed packet method2 media  */
    u_char	dvd_rom_read	: 1;	/* Reads DVD ROM media		     */
    u_char	dvd_r_read	: 1;	/* Reads DVD-R media		     */
    u_char	dvd_ram_read	: 1;	/* Reads DVD-RAM media		     */
    u_char	res_2_67	: 2;	/* Reserved			     */

    u_char	cd_r_write	: 1;	/* Supports writing CD-R  media	     */
    u_char	cd_rw_write	: 1;	/* Supports writing CD-RW media	     */
    u_char	test_write	: 1;	/* Supports emulation write	     */
    u_char	res_3_3		: 1;	/* Reserved			     */
    u_char	dvd_r_write	: 1;	/* Supports writing DVD-R media	     */
    u_char	dvd_ram_write	: 1;	/* Supports writing DVD-RAM media    */
    u_char	res_3_67	: 2;	/* Reserved			     */

    u_char	audio_play	: 1;	/* Supports Audio play operation     */
    u_char	composite	: 1;	/* Deliveres composite A/V stream    */
    u_char	digital_port_2	: 1;	/* Supports digital output on port 2 */
    u_char	digital_port_1	: 1;	/* Supports digital output on port 1 */
    u_char	mode_2_form_1	: 1;	/* Reads Mode-2 form 1 media (XA)    */
    u_char	mode_2_form_2	: 1;	/* Reads Mode-2 form 2 media	     */
    u_char	multi_session	: 1;	/* Reads multi-session media	     */
    u_char	res_4		: 1;	/* Reserved			     */

    u_char	cd_da_supported	: 1;	/* Reads audio data with READ CD cmd */
    u_char	cd_da_accurate	: 1;	/* READ CD data stream is accurate   */
    u_char	rw_supported	: 1;	/* Reads R-W sub channel information */
    u_char	rw_deint_corr	: 1;	/* Reads de-interleved R-W sub chan  */
    u_char	c2_pointers	: 1;	/* Supports C2 error pointers	     */
    u_char	ISRC		: 1;	/* Reads ISRC information	     */
    u_char	UPC		: 1;	/* Reads media catalog number (UPC)  */
    u_char	read_bar_code	: 1;	/* Supports reading bar codes	     */

    u_char	lock		: 1;	/* PREVENT/ALLOW may lock media	     */
    u_char	lock_state	: 1;	/* Lock state 0=unlocked 1=locked    */
    u_char	prevent_jumper	: 1;	/* State of prev/allow jumper 0=pres */
    u_char	eject		: 1;	/* Ejects disc/cartr with STOP LoEj  */
    u_char	res_6_4		: 1;	/* Reserved			     */
    u_char	loading_type	: 3;	/* Loading mechanism type	     */
#define CAPS_LT_CADDY	0
#define CAPS_LT_TRAY	1

    u_char	sep_chan_vol	: 1;	/* Vol controls each channel separat */
    u_char	sep_chan_mute	: 1;	/* Mute controls each channel separat*/
    u_char	disk_present_rep: 1;	/* Changer supports disk present rep */
    u_char	sw_slot_sel	: 1;	/* Load empty slot in changer	     */
    u_char	res_7		: 4;	/* Reserved			     */

    u_short	max_read_speed;		/* Max. read speed in KB/s	     */
    u_short	num_vol_levels;		/* # of supported volume levels	     */
    u_short	buffer_size;		/* Buffer size for the data in KB    */
    u_short	cur_read_speed;		/* Current read speed in KB/s	     */

    u_char	res_16;			/* Reserved			     */

    u_char	res_17_0	: 1;	/* Reserved			     */
    u_char	BCK		: 1;	/* Data valid on falling edge of BCK */
    u_char	RCK		: 1;	/* Set: HIGH high LRCK=left channel  */
    u_char	LSBF		: 1;	/* Set: LSB first Clear: MSB first   */
    u_char	length		: 2;	/* 0=32BCKs 1=16BCKs 2=24BCKs 3=24I2c*/
    u_char	res_17		: 2;	/* Reserved			     */

    u_short	max_write_speed;	/* Max. write speed n KB/s           */
    u_short	cur_write_speed;	/* Current write speed in KB/s	     */
};


#define BLANK_DISC      0x00    /* Erase the entire disc                  */
#define BLANK_MINIMAL   0x01    /* Erase the PMA, 1st session TOC, pregap */
#define BLANK_TRACK     0x02    /* Erase an incomplete track              */
#define BLANK_UNRESERVE 0x03    /* Unreserve a track                      */
#define BLANK_TAIL      0x04    /* Erase the tail of a track: input trackno required */
#define BLANK_UNCLOSE   0x05    /* Unclose the last session               */
#define BLANK_SESSION   0x06    /* Erase the last session                 */

// #define PGCTL_CURRENT_VALUES	0
// #define PGCTL_CHANGE_MASK	1
// #define PGCTL_DEFAULT_VALUES	2
// #define PGCTL_SAVED_VALUES	3	

void	fail(char *fmt, ...);

struct request_sense *get_sense_data();
char*	get_sense_string(void);

int	blank(int fd, int type, int track);				// see BLANK_xxx above
int	format(int fd, int session_grow, int size);
int	close_track_session(int fd, int close_track, int track);	// close_track == 0 --> close session
int	synchronize_cache(int fd);
int	test_unit_ready(int fd);

#define DS_OPERATIONAL		0
#define DS_NO_DISC		1
#define DS_TRAY_OPEN		2
#define DS_NOT_READY		3
int	getDriveState(int fd);					// returns DS_xxx

int	inquiry(int fd, struct cdrom_inquiry *inq);
int	mediumRemoval(int fd, int allow);
int	read_discinfo(int fd, struct cdrom_discinfo *di);
int	read_trackinfo(int fd, struct cdrom_trackinfo *ti, int track);
int	read_buffercapacity(int fd, struct cdrom_buffercapacity *bufcap);
int	read_reccapacity(int fd, struct cdrom_reccapacity *rc);	
int	read_header(int fd, struct cdrom_header *hd, u_int block);
int	reserve_track(int fd, int size);

#define SSU_STOP	0
#define SSU_STOP_EJECT	1
#define SSU_START	2
#define SSU_LOAD_START	3
int	startStopUnit(int fd, int ssu_action);

int	readCD(int fd, int sectortype, int lba, int nblocks, unsigned char* buf);
int	writeCD(int fd, int lba, int nblocks, unsigned char* buf);

int	mode_sense(int fd, u_char **buffer, u_char **mode, u_char pageno, int pgctl);
int	mode_select(int fd, u_char *buffer);

#ifdef MMC2
int	verify(int fd, int lba, int nblocks); 
#else
int	verify(int fd, int sectortype, int lba, int nblocks, char* buf);
#endif

int	set_cdspeed(int fd, int readspeed, int writespeed);

/* get_write_params sets the pointers to the mode header 'buffer' and the mode page 'wp' */
int	get_writeparams(int fd, u_char **buffer, struct cdrom_writeparams **wp, int pgctl);

/* to set write params leave buffer and wp as returned by get_write_params */
int	set_writeparams(int fd, u_char *buffer, struct cdrom_writeparams *wp);
int	get_capabilities(int fd, u_char **buffer, struct cdrom_capabilities **cap, int pgctl);


#endif

