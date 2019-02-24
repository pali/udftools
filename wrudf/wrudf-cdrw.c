/*	wrufd-cdrw.c
 *
 * PURPOSE
 *	Lowlevel IO routines.
 *	To minimise reading and writing packets wrudf uses a few 64kb packetbuffers.
 *	Blocks to be read are preferentially taken from those buffers and updates written
 *	to the buffers. Buffers that are in-use will not be discarded.
 *	When a new buffer is required preferentially an unused not-dirty buffer gets overwritten.
 *	If not available, then a unused dirty buffer gets written and is then reused.
 *	If no such buffer can be found the system panics.
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *
 *  (C) 2001 Enno Fennema <e.fennema@dataweb.nl>
 *
 * HISTORY
 *  16 Aug 01  ef  Created.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>		/* for CDROM_DRIVE_STATUS  */

#include "wrudf.h"
#include "ide-pc.h"
#include "bswap.h"


#define MAXPKTBUFS	4
struct packetbuf {
    uint32_t		inuse;
    uint32_t		dirty;
    uint32_t		bufNum;
    uint32_t		start;
    unsigned char	*pkt;
};

int			lastTrack;
struct cdrom_trackinfo	ti;
struct read_error_recovery_params	*rerp;
u_char *rerp_buffer;
struct cdrom_cacheparams		*cp;
u_char *cp_buffer;

struct packetbuf pktbuf[MAXPKTBUFS+1];			/* last one special use */

static unsigned char *verifyBuffer;					/* for verify only */
static unsigned char *blockBuffer;

uint32_t	trackStart;
uint32_t	trackSize;
int	sectortype;					/* from track info for readCD() */

/* declarations */
struct packetbuf* findBuf(uint32_t blkno);
int	readPacket(struct packetbuf* pb);
int	writePacket(struct packetbuf* pb);


/*	markBlock()
 *	FREE or ALLOC block in spacemap
 */
void markBlock(enum markAction action, uint32_t blkno) {
    uint8_t	*bm;
    uint8_t	mask;
    uint32_t	value;

    spaceMapDirty = 1;
    bm = spaceMap->bitmap + (blkno >> 3);
    mask = 1 << (blkno & 7);
    memcpy(&value, &lvid->data[sizeof(uint32_t)*pd->partitionNumber], sizeof(value));

    if( action == FREE ) {
	*bm |= mask;
	value++;
    } else {
	*bm &= ~mask;
	value--;
    }

    memcpy(&lvid->data[sizeof(uint32_t)*pd->partitionNumber], &value, sizeof(value));
}

/*	GetExtents()
 *	Try to find unallocated blocks for 'requestedLength' bytes in the rewritable partition
 *	Return short_ad's of lbns in the physical partition together satisfying that request
 *	Last extent has length < n * 2048; if exact multiple of 2048 the a final length 0 short_ad
 *
 *	Return value: lenAllocDescs if extents found, 0 if not.
 */
int getExtents(uint32_t requestedLength, short_ad *extents) {
    uint32_t	blkno, lengthFound = 0;
    short_ad	*ext;
    uint32_t	mask, *bm;

    if( medium == CDR ) {
	/* check space availability */
	extents->extLength = requestedLength;
	extents->extPosition = getNWA() -  pd->partitionStartingLocation;
	if( (requestedLength & 2047) == 0 ) {
	    extents[1].extPosition = 0;
	    extents[1].extLength = 0;
	    return 16;
	} else
	    return 8;
    }

    // clear extents
    ext = extents;
    ext->extLength = 0;
    bm = ((uint32_t*)spaceMap->bitmap) - 1;
    mask = 0;
    blkno = 0xFFFFFFFF;

    while( lengthFound < requestedLength ) {
	blkno++;
	mask <<= 1;
	if( mask == 0 ) {
	    bm++;
	    mask = 1;
	    // check bm within limits
	}
	if( (*bm & mask) == 0 ) {			// if allocated
	    if( ext->extLength != 0 ) {
		ext++;
		if( ext - extents > 31 ) {
		    printf("GetExtents: Too many extents\n");
		    return 0;
		}
		ext->extLength = 0;
	    }
	    continue;
	}
	if( ext->extLength == 0 )
	    ext->extPosition = blkno;
	lengthFound += 2048;
	ext->extLength += 2048;
    }

    if( requestedLength != lengthFound  ) {
	ext->extLength = ((ext->extLength & ~2047) | (requestedLength & 2047)) - 2048;
    } else {
	ext++;
	ext->extLength = 0;
	ext->extPosition = 0;
    }
    return (uint8_t*)ext - (uint8_t*)extents + sizeof(short_ad);
}


int
freeShortExtents(short_ad* extents) 
{
    uint32_t blkno, lengthFreed;
    short_ad*	ext;
    
    for( ext = extents; extents->extLength != 0; ext++ ) {
	blkno = ext->extPosition;
	for( lengthFreed = 0 ; lengthFreed < ext->extLength; lengthFreed += 2048 ) {
	    markBlock(FREE, blkno);
	    blkno++;
	}
	if( ext->extLength & 2047 || (ext+1)->extLength == 0 )
	    break;
    }
    return CMND_OK;
}
	
int freeLongExtents(long_ad* extents) {
    uint32_t blkno, lengthFreed;
    long_ad*	ext;

    for( ext = extents; extents->extLength != 0; ext++ ) {
	if( ext->extLocation.partitionReferenceNum != pd->partitionNumber )
	    fail("freeLongExtents: Not RW partition\n");
	blkno = ext->extLocation.logicalBlockNum;
	for( lengthFreed = 0 ; lengthFreed < ext->extLength; lengthFreed += 2048 ) {
	    markBlock(FREE, blkno);
	    blkno++;
	}
	if( ext->extLength & 2047 || (ext+1)->extLength == 0 )
	    break;
    }
    return CMND_OK;
}

/*	getUnallocSpaceExtent()
 *	Only used to get new extent for LVID sequence
 *	(Could use to extend sparing blocks as well)
 */
void getUnallocSpaceExtent(uint32_t requestLength, uint32_t requestAfter, extent_ad *alloc) {
    uint32_t i, n;

    alloc->extLength = alloc->extLocation = 0;
    n = (requestLength + 2047) & ~2047;

    for( i = 0; i < usd->numAllocDescs; i++ ) {
	if( usd->allocDescs[i].extLocation > requestAfter ) {
	    if( usd->allocDescs[i].extLength >= requestLength ) {
		alloc->extLocation = usd->allocDescs[i].extLocation;
		alloc->extLength = n;
		usd->allocDescs[i].extLocation += n >> 11;
		usd->allocDescs[i].extLength -= n;
		usdDirty = 1;
		return;
	    } else {
		alloc->extLocation = usd->allocDescs[i].extLocation;
		alloc->extLength = usd->allocDescs[i].extLength;
		usd->numAllocDescs--;
		for (  ; i < usd->numAllocDescs; i++ )
		    usd->allocDescs[i] = usd->allocDescs[i+1];
		usdDirty = 1;
		return;
	    }
	}
    }
    if( requestAfter > 0 )
	getUnallocSpaceExtent( requestLength, 0, alloc);
}
    
void 
setChecksum(void *descriptor) 
{
    uint32_t	sum;
    int		i;
    tag		*descTag = (tag*) descriptor;

    descTag->descCRC = udf_crc((uint8_t*)descTag + sizeof(tag), descTag->descCRCLength, 0);
    descTag->tagChecksum = sum = 0;
    for( i = 0; i < 16; i++ )
	sum += *((uint8_t*)descTag + i);
    descTag->tagChecksum = (uint8_t) sum;
}

void 
updateTimestamp(time_t t, uint32_t ut) 
{
    struct timeval	timev;
    struct tm		time;
    int			offset;

    if( t == 0 ) {
	gettimeofday(&timev, NULL);
    } else {
	timev.tv_sec = t;
	timev.tv_usec = ut;
    }

    localtime_r( &timev.tv_sec, &time);
    offset = time.tm_gmtoff / 60;
    timeStamp.typeAndTimezone = 0x1000 + (offset & 0x0FFF);		/* 1/3.7.1 */
    timeStamp.year = time.tm_year + 1900;
    timeStamp.month = time.tm_mon + 1;;
    timeStamp.day = time.tm_mday;
    timeStamp.hour = time.tm_hour;
    timeStamp.minute = time.tm_min;
    timeStamp.second = time.tm_sec;
    timeStamp.centiseconds = timev.tv_usec / 10000;
    timeStamp.hundredsOfMicroseconds = (timev.tv_usec / 100) % 100;
    timeStamp.microseconds = timev.tv_usec % 100 ;
}

void setStrictRead(int yes) {

    if( device == DISK_IMAGE )
	return;

    synchronize_cache(device);

    if( yes ) {
	rerp->error_recovery_param = 0x04;
	rerp->retry_count = 2;
	cp->rcd = 1;
    } else {
	rerp->error_recovery_param = 0x00;		// back to default
	rerp->retry_count = 5;
	cp->rcd = 0;
    }
    mode_select(device, rerp_buffer);
    mode_select(device, cp_buffer);
}

uint32_t	getPhysical(uint32_t lbn, uint16_t part) {
    if( part == 0xFFFF ) 
	return lbn;
    if( part == virtualPartitionNum )
	lbn = vat[lbn];
    return lbn + pd->partitionStartingLocation;
}

int cmpBlk(const void* a, const void* b) {		/* compare routine for sparing table lookup */
    if( *(uint32_t*)a < ((struct sparingEntry*)b)->origLocation )
	return -1;
    if( *(uint32_t*)a > ((struct sparingEntry*)b)->origLocation )
	return 1;
    return 0;
}

/* 	If the original packet occurs in the Sparing Table return the mappedLocation
 *	else return the original packet address unchanged.
 */
uint32_t lookupSparingTable(uint32_t original) {
    struct sparingEntry* se;

    if( !st )					// no sparing table found
	return original;

    se = bsearch(&original, st->mapEntry, usedSparingEntries, sizeof(struct sparingEntry), cmpBlk);
    
    if( se )
	return se->mappedLocation;
    else
	return original;
}

/*	Make an entry in the Sparing Table for packet 'original' and
 *	return the mappedLocation
 */
uint32_t newSparingTableEntry(uint32_t original) {
    struct sparingEntry *se, newEntry, swapEntry;
    uint32_t	mapped;
    unsigned int	i;

    if( !st ) {
	printf("No sparing table provided\n");
	return INVALID;
    }

    if( usedSparingEntries == st->reallocationTableLen ) {
	fail("SparingTable full\n");
    }
	
    se = bsearch(&original, &st->mapEntry[0], usedSparingEntries, sizeof(struct sparingEntry), cmpBlk);

    if( se ) {					/* sparing a sparing packet */
	se->origLocation = 0xFFFFFFF0;
    }
    newEntry.origLocation = original;
    newEntry.mappedLocation = mapped = st->mapEntry[usedSparingEntries].mappedLocation;

    for( i = 0; i < usedSparingEntries; i++ ) {
	if( st->mapEntry[i].origLocation < original )
	    continue;
    }
    for(   ; i <= usedSparingEntries; i++ ) {
	swapEntry = st->mapEntry[i];
	st->mapEntry[i] = newEntry;
	newEntry = swapEntry;
    }
    usedSparingEntries++;
    st->sequenceNum++;
    sparingTableDirty = 1;

#ifdef DEBUG
    printf("Sparing %d to %d\n", original, mapped);
#endif
    return mapped;
}

/*	updateSparingTable()
 *	Only done when quitting.
 *	Do not verify writing as that would change the table again.
 */
void updateSparingTable() {
    size_t		i;
    int			pbn, ret;
    off_t		off;
    ssize_t		len;
    struct generic_desc	*p;
    struct packetbuf	*pb;
    struct sparablePartitionMap *spm = (struct sparablePartitionMap*)lvd->partitionMaps;

    for( i = 0; i < sizeof(spm->locSparingTable)/sizeof(spm->locSparingTable[0]); i++ ) {
	pbn = spm->locSparingTable[i];
	if( pbn == 0 )
	    return;

	p = readBlock(pbn, ABSOLUTE);
	pb = findBuf(pbn);
	memcpy(p, st, sizeof(struct sparingTable) + st->reallocationTableLen * sizeof(struct sparingEntry));
	p->descTag.tagLocation = pbn;
	p->descTag.descCRCLength = 
	    sizeof(struct sparingTable) + st->reallocationTableLen * sizeof(struct sparingEntry) - sizeof(tag);
	setChecksum(p);
	if( devicetype != DISK_IMAGE ) {
	    ret = writeCD(device, pbn, 32, pb->pkt);
	    if( ret )
		printf("Write SparingTable at %d: %s\n", pbn, get_sense_string());
	} else { // DISK_IMAGE
	    off = lseek(device, 2048 * pbn, SEEK_SET);
	    if( off == (off_t)-1 )
		fail("writeSparingTable at %d: %s\n", pbn, strerror(errno));
	    len = write(device, pb->pkt, 32 * 2048);
	    if( len < 0 )
		fail("writeSparingTable at %d: %s\n", pbn, strerror(errno));
	    if( len != 32 * 2048 )
		fail("writeSparingTable at %d: %s\n", pbn, strerror(EIO));
	}
    }
}


int 
readPacket(struct packetbuf* pb) 
{
    int		ret;
    off_t	off;
    ssize_t	len;
    uint32_t	physical;

    physical = lookupSparingTable(pb->start);

    if( devicetype != DISK_IMAGE ) {
	ret = readCD(device, sectortype, physical, 32, pb->pkt);
	if( ret )
	    printf("readPacket: readCD %s\n", get_sense_string());
    } else {
	off = lseek(device, 2048 * physical, SEEK_SET);
	if( off == (off_t)-1 )
	    fail("readPacket: lseek failed %s\n", strerror(errno));
	len = read(device, pb->pkt, 32 * 2048);
	if( len < 0 )
	    fail("readPacket: read failed %s\n", strerror(errno));
	if( len != 32 * 2048 )
	    fail("readPacket: read failed %s\n", strerror(EIO));
	ret = 0;
    }
    return ret;
}
   

int 
writePacket(struct packetbuf* pb) 
{
    int		ret, retry;
    off_t	off;
    ssize_t	len;
    uint32_t	physical;

    pb->dirty = 0;
    physical = lookupSparingTable(pb->start);

    if( devicetype != DISK_IMAGE ) {
	for(retry = 0; retry < 2; retry++ ) {
	    if( retry != 0 )
		physical = newSparingTableEntry(pb->start);

	    ret = writeCD(device, physical, 32, pb->pkt);

	    if( ret )
		fail("writePacket: writeCD %s\n", get_sense_string());

	    // must now verify write operation and spare if necessary
	    // My HP8100 does not support Verify or WriteAndVerify
	    // Use ReadCD with strict Read Error Recovery Parameters
	    setStrictRead(1);
	    ret = readCD(device, sectortype, physical, 32, verifyBuffer);

	    if( ret == 0 ) {
		setStrictRead(0);
		return 0;
	    }
	    printf("writePacket: verify %s\n", get_sense_string());
	}
	setStrictRead(0);
    } else { // DISK_IMAGE
#ifdef DEBUG_SPARING						// force sparing on packets of disk image 
	if( st ) {
	    if( physical == 0x820 || physical == 0x880 ) 	// arbitrary 'bad' blocks
		physical = newSparingTableEntry(pb->start);
	}
#endif
	off = lseek(device, 2048 * physical, SEEK_SET);
	if( off == (off_t)-1 )
	    fail("writePacket: writeHD failed %s\n", strerror(errno));
	len = write(device, pb->pkt, 32 * 2048);
	if( len < 0 )
	    fail("writePacket: writeHD failed %s\n", strerror(errno));
	if( len != 32 * 2048 )
	    fail("writePacket: writeHD failed %s\n", strerror(EIO));
	ret = 0;
    }
    return ret;
}


struct packetbuf* 
findBuf(uint32_t blkno) 
{
    struct packetbuf *b;

    blkno &= ~31;
    for(b = &pktbuf[0]; b < &pktbuf[MAXPKTBUFS]; b++ ) {
	if( blkno == b->start )
	    return b;
    }
    return NULL;
}


struct packetbuf* 
getFreePacketBuffer(uint32_t lbn, uint16_t part)
{
    struct packetbuf	*b, *bFree, *bMustWrite;
    uint32_t		blkno;

    blkno = lbn + ( part == 0xFFFF ? 0 : pd->partitionStartingLocation ); 
    bFree = bMustWrite = NULL;

    for( b = &pktbuf[0]; b < &pktbuf[MAXPKTBUFS]; b++ ) {
	if( (b->inuse | b->dirty) == 0 ) {
	    bFree = b;
	    break;
	} else
	    if( b->inuse == 0 )
		bMustWrite = b;
    }

    if( !bFree && !bMustWrite ) {
	printf("readBlock: Permission to panic, Sir!!!\n");
	return NULL;
    }

    if( !bFree ) {
	writePacket(bMustWrite);
	bMustWrite->inuse = 0;
	bMustWrite->dirty = 0;
	bFree = bMustWrite;
    }

    bFree->start = blkno & ~31;
    return bFree;
}

void* 
readBlock(uint32_t lbn, uint16_t part) 
{
    struct packetbuf *b;
    uint32_t	physical = getPhysical(lbn, part);

    if( medium == CDR )
	return readCDR(physical, ABSOLUTE);

    b = findBuf(physical);

    if( !b ) {
	b = getFreePacketBuffer(lbn, part);
	readPacket(b);
    }

    b->inuse |= 0x80000000 >> (lbn & 31);
    return b->pkt + ((lbn & 31) << 11);
}


void* 
readSingleBlock(uint32_t pbn) 
{
    int ret;
    off_t off;
    ssize_t len;

    if( devicetype != DISK_IMAGE ) {
	ret = readCD(device, sectortype, pbn, 1, blockBuffer);
	if( ret ) {
	    if( ! ignoreReadError )
		printf("readSingleBlock: %s\n", get_sense_string());
    	    return NULL;
	} else 
	    return blockBuffer;
    } else {
	off = lseek(device, 2048 * pbn, SEEK_SET);
	if( off != (off_t)-1 )
	    return NULL;
	len = read(device, blockBuffer, 2048);
	if( len != 2048 )
	    return NULL;
	else
	    return blockBuffer;
    }
}


void 
freeBlock(uint32_t lbn, uint16_t part) 
{
    struct packetbuf	*b;
    uint32_t			blkno;

    if( medium == CDR ) return;

    blkno = lbn + ( part == 0xFFFF ? 0 : pd->partitionStartingLocation );
    b = findBuf(blkno);

    if( !b )
	fail("freeBlock failed on block %d\n", blkno);

    b->inuse &=  ~(0x80000000 >> (blkno & 31));		/* turn off INUSE bit */
}


void 
dirtyBlock(uint32_t lbn, uint16_t part) 
{
    struct packetbuf	*pb;
    uint32_t			blkno;

    blkno = lbn + ( part == 0xFFFF ? 0 : pd->partitionStartingLocation );
    pb = findBuf(blkno);

    if( !pb )
	fail("dirtyBlock failed on block %d\n", blkno);

    pb->dirty |=  0x80000000 >> (blkno & 31);		/* turn on DIRTY bit */
}

void
writeBlock(uint32_t lbn, uint16_t part, void* src) 
{
    char *p;

    p = readBlock(lbn, part);
    memcpy(p, src, 2048);
    if( part != ABSOLUTE )
	markBlock(ALLOC, lbn);
    dirtyBlock(lbn, part);
    freeBlock(lbn, part);
}


void* 
readTaggedBlock(uint32_t lbn, uint16_t partition) 
{
    int		i;
    uint8_t	sum, *p;
    struct generic_desc *block;


    if( medium == CDR )
	block = (struct generic_desc*) readCDR(lbn, partition);
    else { 
	block = (struct generic_desc*) readBlock(lbn, partition);
	freeBlock(lbn,partition);

	if( !block )
	    return block;
    }

    if( block->descTag.tagIdent == 0x0000 ) {
	/* ignore not ISO 13346 defined descriptors with 0 tags */
	if( strncmp((char *)((struct sparingTable*)block)->sparingIdent.ident, UDF_ID_SPARING, strlen(UDF_ID_SPARING)) != 0 ) {
	    for( i = 0; i < 2048; i++ ) {
		if( ((uint8_t*)block)[i] != 0 ) {
		    printf("readTaggedBlock: Empty block %d not all zeroes\n", lbn);
		    break;
		}
	    }
	    return block;
	}
    }

    for( i = 0, sum = 0, p = (uint8_t*)block; i< 16; i++ )
	if( i != 4 )
	    sum += *(p + i);

    if( block->descTag.tagChecksum != sum )
	fail("readTagged: Checksum error in block %d\n", lbn);

    if( block->descTag.descCRC != udf_crc((uint8_t*)block + sizeof(tag), ((tag*)block)->descCRCLength, 0) )
	fail("readTagged: CRC error in block %d\n", lbn);

    return block;
}


int
readExtents(char* dest, int usesShort, void* extents) 
{
    uint	len, blkno, partitionNumber;
    char	*p;
    long_ad	*lo;
    short_ad	*sh;

    if( usesShort ) {
	sh = (short_ad*) extents;
	len = sh->extLength;
	blkno = sh->extPosition;
	partitionNumber  = pd->partitionNumber;
    } else {
	lo = (long_ad*) extents;
	len = lo->extLength;
	blkno = lo->extLocation.logicalBlockNum;
	partitionNumber = lo->extLocation.partitionReferenceNum;
    }

    for(;;) {
	p = readBlock(blkno, partitionNumber);
	memcpy(dest, p, 2048);
	freeBlock(blkno, partitionNumber);
	dest += 2048;
	if( len < 2048 )
	    break;
	if( len == 0 ) {
	    if( usesShort ) {
		sh++;
		len = sh->extLength;
		blkno = sh->extPosition;
	    } else {
		lo++;
		len = lo->extLength;
		blkno = lo->extLocation.logicalBlockNum;
		partitionNumber = lo->extLocation.partitionReferenceNum;
	    }
	    if( len == 0 )
		break;
	    continue;
	}
	len -= 2048;
	blkno++;
    } 
    return CMND_OK;
}    


int
writeExtents(char* src, int usesShort, void* extents) 
{
    uint	len, blkno, partitionNumber;
    long_ad	*lo=NULL;
    short_ad	*sh=NULL;

    if( usesShort ) {
	sh = (short_ad*) extents;
	len = sh->extLength;
	blkno = sh->extPosition;
	partitionNumber  = pd->partitionNumber;
    } else {
	lo = (long_ad*) extents;
	len = lo->extLength;
	blkno = lo->extLocation.logicalBlockNum;
	partitionNumber = lo->extLocation.partitionReferenceNum;
    }

    for(;;) {
	writeBlock(blkno, partitionNumber, src);
	src += 2048;
	if( len < 2048)
	    break;
	if( len == 2048 ) {
	    if( usesShort ) {
		sh++;
		len = sh->extLength;
		blkno = sh->extPosition;
	    } else {
		lo++;
		len = lo->extLength;
		blkno = lo->extLocation.logicalBlockNum;
		partitionNumber = lo->extLocation.partitionReferenceNum;
	    }
	    if( len == 0 )
		break;
	    continue;
	}
	len -= 2048;
	blkno++;
    }
    return CMND_OK;
}    


int
initIO(char *filename) 
{
    struct packetbuf *pb;
    int		rv;
    off_t	off;
    ssize_t	len;
    struct stat filestat;
    struct cdrom_discinfo  di;
    u_char *buffer;
    struct cdrom_writeparams *wp;
    uint16_t	ident;

    if( (rv = stat(filename, &filestat)) < 0 )
	fail("initIO: stat on %s failed\n", filename);

    if( S_ISREG(filestat.st_mode) ) {		/* disk image of a UDF volume */
	devicetype = DISK_IMAGE;
	if( (device = open(filename, O_RDWR)) < 0 ) {
	    fail("initIO: open %s failed\n", filename);
	    return 0;
	}

	/* heuristically determine medium imitated on disk image based on VAT FileEntry in block 512 */
	off = lseek(device, 2048 * 512, SEEK_SET);
	if( off == (off_t)-1 )
	    fail("initIO: lseek %s failed: %s\n", filename, strerror(errno));
	len = read(device, &ident, 2);
	if( len < 0 )
	    fail("initIO: read %s failed: %s\n", filename, strerror(errno));
	if( len != 2 )
	    fail("initIO: read %s failed: %s\n", filename, strerror(EIO));
	medium = ident == TAG_IDENT_VDP ? CDR : CDRW;

	if( medium == CDRW ) {
	    for( pb = pktbuf, rv = 1; pb <= pktbuf + MAXPKTBUFS; pb++ ) {
		pb->start = 0xFFFFFFFF;
		pb->pkt = malloc(32*2048);
		pb->bufNum = rv++;
		if( pb->pkt == NULL )
		    fail("malloc packetBuffer failed\n");
	    }
	}
    }

    if( (blockBuffer = malloc(2048)) == NULL )
	fail("malloc blockBuffer failed\n");

    if( devicetype == DISK_IMAGE )
	return 0;

    if( (device = open(filename, O_RDONLY | O_NONBLOCK )) < 0 )
	fail("initIO: open %s failed\n", filename);

    rv = ioctl(device, CDROM_DRIVE_STATUS);

    if( (rv != CDS_DISC_OK) && (rv != CDS_NO_INFO) )
	fail("No disc or not ready\n");

    if( read_discinfo(device, &di) )
	fail("Read discinfo failed\n");

    if( di.erasable == 0 ) {
	medium = CDR;
	printf("CDR disc\n");
    } else {
	medium = CDRW;
	printf("CDRW disc\n");
    }

    lastTrack = di.trk1_lastsession;

    if( read_trackinfo(device, &ti, lastTrack) )	/* last track 1 in last session writable for UDF */
	fail("Read discinfo failed\n");

    if( medium == CDRW ) {
	if( !ti.fixpkt ) {
	    printf("Assume CDRW disc used as CDR\n");
	    medium = CDR;
	} else if ( ti.fixpkt_size != 32 )
	    fail("CDRW not fixed 32 sector packets\n");
    }

    if( medium == CDRW ) {
	for( pb = pktbuf, rv = 1; pb <= pktbuf + MAXPKTBUFS; pb++ ) {
	    pb->start = 0xFFFFFFFF;
	    pb->pkt = malloc(32*2048);
	    pb->bufNum = rv++;
	    if( pb->pkt == NULL )
		fail("malloc packetBuffer failed\n");
	}
	if( (verifyBuffer = malloc(32 * 2048)) == NULL )
	    fail("malloc verifyBuffer failed\n");
    }

    if( medium == CDR ) {
	if( ti.rsrvd_trk || ! ti.packet || ti.fixpkt )
	    fail("CDR disc last track reserved or not variable packet\n");

	if( !ti.nwa_v )
	    fail("Next writable address invalid in track %d\n", ti.trk);
    }

    trackStart = ti.trk_start;
    trackSize  = ti.trk_size;

    if( ti.data_mode == 1 )
	sectortype = 2;					/* for readCD() */
    else if( ti.data_mode == 2 )
	sectortype = 4;
    else
	fail("Neither Data Mode1 nor Mode2 on disc\n");

    /* get read error recovery parameters page */
    rv = mode_sense(device, &rerp_buffer, (void *)&rerp, GPMODE_R_W_ERROR_PAGE, PGCTL_CURRENT_VALUES);
	
    /* get cache parameters page */
    rv = mode_sense(device, &cp_buffer, (void *)&cp, GPMODE_CACHE_PAGE, PGCTL_CURRENT_VALUES);

    /* setup write params */
    get_writeparams(device, &buffer, &wp, PGCTL_CURRENT_VALUES);

    wp->write_type = 0;
//    wp->trk_mode = 7;
    wp->copy = 0;
    wp->fp = medium == CDRW ? 1 : 0;
    wp->test_write = 0;
    wp->data_blk_type = ( ti.data_mode == 2 ? DB_XA_F1 : DB_ROM_MODE1);
    wp->host_appl_code = 0;
    wp->session_format = (ti.data_mode == 2 ? 0x20 : 0x00);
    wp->pkt_size = medium == CDRW ? 32 : 0;
    wp->subhdr0 = 0x00;
    wp->subhdr1 = 0x00;
    wp->subhdr2 = 0x08;
    wp->subhdr3 = 0x00;

    if( (rv = set_writeparams(device, buffer, wp)) ) 
    {
        free(buffer);
	fail("Set write parameters failed\n");
    }
    else
        free(buffer);

    return 0;
}	


int 
closeIO(void) 
{
    struct packetbuf *pb;

    if( medium == CDRW ) {
	for( pb = pktbuf; pb < pktbuf + MAXPKTBUFS; pb++ ) {
	    if( pb->dirty && !pb->inuse )
		writePacket(pb);
	    if( pb->inuse || pb->dirty)
		printf("PacketBuffet[%d] at %d inuse %08X  dirty %08X\n", 
		    pb->bufNum, pb->start, pb->inuse, pb->dirty);
	    free(pb->pkt);
	}
    }

    if( blockBuffer ) free(blockBuffer);
    if( verifyBuffer ) 	free(verifyBuffer);

    if( devicetype != DISK_IMAGE )
	synchronize_cache(device);
	
    close(device);
    return 1;
}
