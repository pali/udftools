/*	ide-pc.c
 *
 * PURPOSE
 *	Execute most MMC commands on a ATAPI-Packet command CD(re)writer through IOCTL CDROM_SEND_PACKET
 *	Where similar routines and structures are defined in linux/ cdrom.h this program
 *	maintains structures strictly as defined MMC to facilitate the interface with the CDwriter
 *	at the expense of a simple user interface.
 *
 * CONTACTS
 *	E-mail regarding this program should be addressed to
 *		e.fennema@dataweb.nl
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
 *
 *	16 Aug 01  ef  Created.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>		/* for u_char etc. */
#include <linux/cdrom.h>
#include <unistd.h>		/* sleep() */

#include "bswap.h"
#include "ide-pc.h"

typedef struct cdrom_generic_command CGC;

static int rv;			// -1 if ioctl failed, else equal stat
static struct request_sense sense_data;


void fail(char *fmt, ...) {
    va_list	args;

    va_start(args, fmt); 
    vfprintf(stderr, fmt, args);
    exit(1);
}

struct request_sense *
get_sense_data() 
{
    return &sense_data;
}

char*
get_sense_string() 
{
    static char str[128];

    sprintf(str, "Stat %d Err %02X  Key %02Xx; ASC %02Xx; ASCQ %02Xx; Info %02X%02X%02X%02Xx",
	rv, sense_data.error_code, sense_data.sense_key, sense_data.asc, sense_data.ascq, 
	sense_data.information[0], sense_data.information[1], sense_data.information[2], sense_data.information[3]);
    return str;
}

void initpc(CGC *pc) {
    memset(pc,0,sizeof(*pc));
    memset(&sense_data, 0 , sizeof(sense_data));
    pc->sense=&sense_data;
    pc->data_direction = CGC_DATA_NONE;
    pc->timeout = 2500;
}

int
blank(int fd, int type, int start)
{
    CGC pc;

    initpc(&pc);
    pc.cmd[0]=GPCMD_BLANK;
    pc.cmd[1]= type & 7;
    *(u_int*)&pc.cmd[2] = cpu_to_be32(start);		/* blank track tail : lba to start blanking
							 * blank track : track number to blank */
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
close_track_session(int fd, int close_track, int track)
{
    CGC pc;

    initpc(&pc);
    pc.cmd[0] = GPCMD_CLOSE_TRACK;
    pc.cmd[2] = close_track ? 0x01 : 0x02;
    pc.cmd[5] = track;
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
format(int fd, int session_grow, int size)
{
    struct {
	u_char	flh_rsrv;
	u_char	flh_imm;
	u_short	flh_fdlen;
	u_int	id;
	u_char	fd_sg;
	u_char	fd_rsrv[3];
	u_int	fd_size;
    }	f;			
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_WRITE;
    memset(&f, 0, sizeof(f));
    pc.buffer= (void*)&f;
    pc.buflen= sizeof(f);
    pc.cmd[0] = GPCMD_FORMAT_UNIT;
    pc.cmd[1] = 0x17;				/* Format Parameter List present; format 0x7 */
    f.flh_fdlen = cpu_to_be16(8);
    f.fd_sg = session_grow;
    f.fd_size = cpu_to_be32(size);
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}


int
set_cdspeed(int fd, int readspeed, int writespeed)
{
    CGC pc;

    initpc(&pc);
    pc.cmd[0]=GPCMD_SET_SPEED;
    *(u_short*) &pc.cmd[2] = cpu_to_be16(readspeed);
    *(u_short*) &pc.cmd[4] = cpu_to_be16(writespeed);

    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
synchronize_cache(int fd)
{
    CGC pc;

    initpc(&pc);
    pc.cmd[0] = GPCMD_FLUSH_CACHE;
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
test_unit_ready(int fd)
{
    CGC pc;

    initpc(&pc);		 		/* Test Unit Ready command = 0x00 */
    /*	kernel returns -1 when CHECK_CONDITION status returned
     *	sense data is valid and gives further info
     */
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int getDriveState(int fd ) {
    int	rv;
 
    rv = test_unit_ready(fd);

    if( rv ) sleep(2);				/* wait a bit longer */

    rv = test_unit_ready(fd);

    if( rv < 0 )
	return DS_NOT_READY;

    if( rv == 0 ) 
	return DS_OPERATIONAL;

    if( sense_data.sense_key == 2 && sense_data.asc == 0x04 && sense_data.ascq == 0x04 )
	return DS_OPERATIONAL;

    if( sense_data.sense_key == 2 && sense_data.asc == 0x3A &&  sense_data.ascq == 2 )
	return DS_TRAY_OPEN;
    
    return DS_NO_DISC;
}

int
inquiry(int fd, struct cdrom_inquiry *inq)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0]=GPCMD_INQUIRY;
    pc.cmd[4]=sizeof(struct cdrom_inquiry);
    pc.buffer= (void*)inq;
    pc.buflen= sizeof(struct cdrom_inquiry);
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}    

int
mediumRemoval(int fd, int allow)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_NONE;
    pc.cmd[0] = GPCMD_PREVENT_ALLOW_MEDIUM_REMOVAL;
    pc.cmd[4] = allow;
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}    

int
read_reccapacity(int fd, struct cdrom_reccapacity *reccap)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0]=GPCMD_READ_CDVD_CAPACITY;
    pc.buffer=(void*)reccap;
    pc.buflen=sizeof(struct cdrom_reccapacity);
    rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
    reccap->lastblock  = be32_to_cpu(reccap->lastblock);
    reccap->blocksize = be32_to_cpu(reccap->blocksize);
    return rv;
}

int
read_buffercapacity(int fd, struct cdrom_buffercapacity *bufcap)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0] = 0x5C;				// READ BUFFER CAPACITY
    *(u_short*)&pc.cmd[7] = cpu_to_be16(sizeof(struct cdrom_buffercapacity));
    pc.buffer=(void*)bufcap;
    pc.buflen=sizeof(struct cdrom_buffercapacity);
    rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
    bufcap->totalBufferLength  = be32_to_cpu(bufcap->totalBufferLength);
    bufcap->freeBufferLength = be32_to_cpu(bufcap->freeBufferLength);
    return rv;
}

int
reserve_track(int fd, int size)
{
    CGC pc;

    initpc(&pc);
    pc.cmd[0]=GPCMD_RESERVE_RZONE_TRACK;		/* 0x53 */
    *(u_int*)&pc.cmd[5] = cpu_to_be32(size);
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
startStopUnit(int fd, int action)
{
    CGC pc;

    initpc(&pc);
    pc.cmd[0] = GPCMD_START_STOP_UNIT;
    pc.cmd[4] = action;
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
read_discinfo(int fd, struct cdrom_discinfo *di)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;

    pc.cmd[0]=GPCMD_READ_DISC_INFO;
    *(u_short*) &pc.cmd[7] = cpu_to_be16( sizeof(struct cdrom_discinfo) );
    pc.buffer=(void*)di;
    pc.buflen=sizeof(struct cdrom_discinfo);

    rv = ioctl(fd, CDROM_SEND_PACKET, &pc);

    di->data_length = be16_to_cpu(di->data_length);
    di->disc_id = be32_to_cpu(di->disc_id);
    return rv;
}

int
read_header(int fd, struct cdrom_header *hd, u_int block)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0]=GPCMD_READ_HEADER;
    *(u_int*)&pc.cmd[2] = cpu_to_be32( block);
    *(u_short*)&pc.cmd[7] = cpu_to_be16( sizeof(struct cdrom_header) );
    pc.buffer=(void*)hd;
    pc.buflen=sizeof(struct cdrom_header);
    rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
    hd->lba = be32_to_cpu(hd->lba);
    return rv;
}


int
read_trackinfo(int fd, struct cdrom_trackinfo *ti, int trackno)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0]=GPCMD_READ_TRACK_RZONE_INFO;
    pc.cmd[1]=1;
    *(u_int*)&pc.cmd[2] = cpu_to_be32( trackno );
    *(u_short*)&pc.cmd[7] = cpu_to_be16( sizeof(struct cdrom_trackinfo) );	/* 28; if higher data underrun error */
    pc.buffer=(void*)ti;
    pc.buflen=sizeof(struct cdrom_trackinfo);
    rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
    ti->data_length = be16_to_cpu(ti->data_length);
    ti->trk_start = be32_to_cpu(ti->trk_start);
    ti->nwa = be32_to_cpu(ti->nwa);
    ti->free_blks = be32_to_cpu(ti->free_blks);
    ti->fixpkt_size = be32_to_cpu(ti->fixpkt_size);
    ti->trk_size = be32_to_cpu(ti->trk_size);
    return rv;
}


int
readCD(int fd, int sectortype, int lba, int n, char* buf)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0]=GPCMD_READ_CD;
    pc.cmd[1]=sectortype << 2;			/* Sector type per MCC table 31 */
    *(int*) &pc.cmd[2] = cpu_to_be32(lba);
    pc.cmd[8]=n;				/* read n blocks */
    pc.cmd[9]=0x10;				/* user data only */
    pc.buffer=buf;
    pc.buflen=n * 2048;
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
writeCD(int fd, int lba, int nblks, char* buf)
{
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_WRITE;
    pc.cmd[0] = GPCMD_WRITE_10;
    pc.cmd[1] = 0;				/* DPO 0x10; FUA 0x08 */
    *(u_int*)  &pc.cmd[2] = cpu_to_be32(lba);
    *(u_short*)&pc.cmd[7] = cpu_to_be16(nblks);
    pc.buffer = buf;
    pc.buflen = nblks * 2048;
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}


#ifdef MMC2	// VERIFY and WRITE_AND_VERIFY are new in MMC2 and not supported by my HP8100
int
verify(int fd, int lba, int nblocks)
{
    CGC pc;

    initpc(&pc);
    pc.cmd[0] = GPCMD_VERIFY_10;
    pc.cmd[1] = 0;
    *(u_int*) &pc.cmd[2] = cpu_to_be32(lba);
    pc.cmd[8] = (u_char) nblocks;
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}
#endif

/*
 *	mode_sense
 *	Returns 8 byte header followed by actual mode page. The HP 8100 recognizes:
 *	 1 - Read Error Recovery page
 *		In MtFuji this is called Read/Write Error recovery mode page
 *		The separate Verify Error Recovery Mode page of SCSI-2 draft has gone.
 *		The HP8100 recovers a change mask of 01 06 36 FF 00 00 00 00 
 *		This would mean retry count 0 to 255 acceptable
 *		and TB, RC, PER and DTE bits settable. Not the DCR bit.
 *		Could these also affect a verify operation. MtFuji VERIFY says no.
 *	 5 - Write parameters page
 *	 8 - Media supported mode page
 *	 D - CD-ROM page: Inactivity time, seconds/minute, frames/second
 *	 E - CD-ROM Audio Control page
 *	1D - Timeout and protect (MtFuji 12.11.3.5)
 *	2A - CD Capabilities mode page
 *	2F - ASCII message ? : '(c) 1997-1998 HP, license available'
 */

int
mode_select(int fd, u_char *buffer)
{
    CGC pc;
    int len = 2 + be16_to_cpu(((mode_hdr*)buffer)->data_length);

    initpc(&pc);
    pc.data_direction = CGC_DATA_WRITE;
    pc.cmd[0] = GPCMD_MODE_SELECT_10;	
    pc.cmd[1] = 0x10;					// PF bit
    pc.buffer = buffer;
    pc.buflen = len;
    *(u_short*) &pc.cmd[7] = cpu_to_be16(len);
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}

int
mode_sense(int fd, u_char **buffer, u_char **mode, u_char pageno, int pgctl)
{
    CGC pc;
    mode_hdr h;
    int len, offset;

    memset(&h, 0x00, sizeof(mode_hdr));

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0] = GPCMD_MODE_SENSE_10;
    pc.cmd[2] = (pgctl<<6) + (pageno & 0x3F);
    *(u_short*) &pc.cmd[7] = cpu_to_be16(sizeof(mode_hdr));
    pc.buffer = (void *)&h;
    pc.buflen = sizeof(mode_hdr);
    rv = ioctl(fd, CDROM_SEND_PACKET, &pc);

    if( rv ) {
	printf("mode_sense(1): stat %d\n", rv);
	return rv;
    }

    len = 2 + be16_to_cpu(h.data_length);
    offset = 8 + be16_to_cpu(h.block_desc_length);

    *buffer = (u_char*) calloc(len, sizeof(u_char));
    *mode = &(*buffer)[offset];
    pc.buffer = *buffer;

    pc.buflen = len;
    *(u_short*) &pc.cmd[7] = cpu_to_be16(len);
    return ioctl(fd, CDROM_SEND_PACKET, &pc);
}


// MMC-2 feature not supported by my HP 8100 CD-Writer
#ifdef MMC2
int
get_configuration(int fd, u_char* buffer, int size, u_char feature)
{
    int i, realsize;
    CGC pc;

    initpc(&pc);
    pc.data_direction = CGC_DATA_READ;
    pc.cmd[0] = 0x46;				// GET CONFIGURATION
    *(u_short*) &pc.cmd[2] = cpu_to_be16(feature);
    *(u_short*) &pc.cmd[7] = cpu_to_be16(8);		// only to see how much will be returned
    pc.buffer=(void*)buffer;
    if( size < 8 ) return -EINVAL;
    pc.buflen = 8;
    rv = ioctl(fd, CDROM_SEND_PACKET, &pc);

    if( rv )
	return rv;

    realsize = be32_to_cpu((int)buffer[0]) + 4;
    if( realsize < size ) {
	pc.buflen = realsize;
	*(u_short*) &pc.cmd[7] = cpu_to_be16(realsize);
    } else {
	pc.buflen = size;
	*(u_short*) &pc.cmd[7] = cpu_to_be16(size);
    }
    return rv = ioctl(fd, CDROM_SEND_PACKET, &pc);
}
#endif		// MMC2


int
get_writeparams(int fd, u_char **buffer, struct cdrom_writeparams **wp, int pgctl)
{
    rv = mode_sense(fd, buffer, (u_char**) wp, 0x05, pgctl);
    (*wp)->pkt_size = be32_to_cpu((*wp)->pkt_size);
    (*wp)->audio_pause = be16_to_cpu((*wp)->audio_pause);
    return rv;
}

int
set_writeparams(int fd, u_char *buffer, struct cdrom_writeparams *wp)
{
    wp->pkt_size = cpu_to_be32(wp->pkt_size);
    wp->audio_pause = cpu_to_be16(wp->audio_pause);

    rv = mode_select(fd, buffer);

    wp->pkt_size = be32_to_cpu(wp->pkt_size);
    wp->audio_pause = be16_to_cpu(wp->audio_pause);
    return rv;
}


int
get_capabilities(int fd, u_char **buffer, struct cdrom_capabilities **cap, int pgctl)
{
    rv = mode_sense(fd, buffer, (u_char**) cap, GPMODE_CAPABILITIES_PAGE, pgctl);

    (*cap)->max_read_speed  = be16_to_cpu((*cap)->max_read_speed);
    (*cap)->num_vol_levels  = be16_to_cpu((*cap)->num_vol_levels);
    (*cap)->buffer_size     = be16_to_cpu((*cap)->buffer_size);
    (*cap)->cur_read_speed  = be16_to_cpu((*cap)->cur_read_speed);
    (*cap)->max_write_speed = be16_to_cpu((*cap)->max_write_speed);
    (*cap)->cur_write_speed = be16_to_cpu((*cap)->cur_write_speed);
    return rv;
}

