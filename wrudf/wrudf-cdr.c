/*	wrudf-cdr.c 
 *
 */

#include "config.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "wrudf.h"
#include "ide-pc.h"
#include "bswap.h"

static unsigned char *blockBuffer;
static uint32_t newVATindex;
static uint32_t sizeVAT;
static uint32_t prevVATlbn;
uint64_t  CDRuniqueID;			// from VAT FE


uint32_t newVATentry() {

    if( newVATindex + 10 > (sizeVAT >> 2) ) {		// ensure enough spave for regid and prevVATlbn
	sizeVAT += 2048;
	vat = realloc(vat, sizeVAT);
	if( vat == NULL )
	    printf("VAT reallocation failed\n");
    }
    /* could go through VAT and try to find unused 0xFFFFFFFF entry rather than alloc new one at the end */
    vat[newVATindex] = getNWA() - pd->partitionStartingLocation;
    return newVATindex++;
}

uint32_t	getNWA() {
    if( devicetype == DISK_IMAGE )
	return lseek(device, 0, SEEK_END) >> 11;

    if( read_trackinfo(device, &ti, lastTrack) )
	printf("Get Track Info failed\n");

    if( ti.nwa_v )
	return ti.nwa;
    else
	return INVALID;
}

uint32_t getMaxVarPktSize() {
    struct cdrom_buffercapacity bc;

    if( devicetype == DISK_IMAGE  )
	return 1024 * 2048;			// nonsense value; just for testing

    read_buffercapacity(device, &bc);

    return ((bc.freeBufferLength >> 11) - 20) << 11;	// - 20 :: keep some spare blocks ???
}

unsigned char*	readCDR(uint32_t lbn, uint16_t partition) {
    int		stat;
    uint32_t	pbn = getPhysical(lbn, partition);


    if( devicetype == DISK_IMAGE  ) {
	stat = lseek(device, 2048 * pbn, SEEK_SET);
	stat = read(device, blockBuffer, 2048);
	if( stat != 2048 )
	    fail("readCDR(hd) failed %s\n", strerror(errno));
	else
	    return blockBuffer;
    }

    stat = readCD(device, sectortype, pbn, 1, blockBuffer);

    if( stat ) {
	if( ! ignoreReadError )
	    printf("readCDR: %s\n", get_sense_string());
	return NULL;
    }
    return blockBuffer;
}    

void
writeHD(uint32_t physical, unsigned char* src) 
{
    int		stat;

    stat = lseek(device, 2048 * physical, SEEK_SET);
    stat = write(device, src, 2048);

    if( stat != 2048 ) {
	printf("writeHD failed %s\n", strerror(errno));
    }
}

void syncCDR() {
//    uint32_t nwa;

    if( device == DISK_IMAGE )
	return;

    synchronize_cache(device);
//    nwa = getNWA();
//    printf("Link from %d to %d\n", nwa-7, nwa-1);
}

void writeHDlink() {
    uint32_t blk = getNWA();
    int		i;

    for(i = 0; i < 7; i++ )
	writeHD(blk+i, blockBuffer);
}



/*	writeBlockCDR()
 *	Writes one 2048 byte block
 */
uint32_t
writeCDR(void* src) {
    uint32_t	physical;

    physical = getNWA();
//    printf("Write CDR sector %d\n", physical);

    if( devicetype == DISK_IMAGE )
	writeHD(physical, src);
    else {
	writeCD(device, physical, 1, src);
    }
    return physical;
}


/*	flagError
 *	split up extent in (1) good, (2) bad and (3) good subextents.
 *	In bad extent (2) set logicalBlockNum to zero
 *	If (1) is empty, maybe (2) can be merged with immediately preceding bad extent
 */
long_ad *
flagError(long_ad *extents, long_ad *ext, uint32_t pbn) 
{
    uint32_t lbn = pbn - pd->partitionStartingLocation;
    int	posInExtent = lbn - ext->extLocation.logicalBlockNum + 1;
    int	blksInExtent = (ext->extLength + 2047) >> 11;

    /* if there is a preceding extent which is bad and 
     * the current bad block is the first in this extent, 
     * then add this bad to the preceding extent
     */
    if( ext > extents && lbn == ext->extLocation.logicalBlockNum && !ext[-1].extLocation.logicalBlockNum ) {
	if( ext[0].extLength < 2048 ) {
	    ext[-1].extLength += ext[0].extLength;
	    ext[0].extLength = 0;
	} else {
	    ext[-1].extLength += 2048;
	    ext[0].extLength -= 2048;
	    ext[0].extLocation.logicalBlockNum++;
	}
	return ext - 1;
    }

    /* watch one block extents */
    if( blksInExtent == 1 ) {
	ext[0].extLocation.logicalBlockNum = 0;
	return ext;
    }

    /* 1st block of extent is bad */
    if( posInExtent == 1 ) {
	memmove(ext+1, ext, extents - ext + 62);
	ext[0].extLength = 2048;
	ext[0].extLocation.logicalBlockNum = 0;
	ext[1].extLength -= 2048;
	ext[1].extLocation.logicalBlockNum = lbn + 1;
	return ext;
    }

    /* last block of extent is bad - watch partial block */
    if( posInExtent == blksInExtent ) {
	memmove(ext+1, ext, extents - ext + 62);
	ext[1].extLength = ext->extLength - 2048;
	ext[1].extLocation.logicalBlockNum = 0;
	ext[0].extLength -= ext[1].extLength;
	return ext;
    }

    /* middle block in extent is bad */
    memmove(ext+2, ext, extents - ext + 61);
    memcpy(ext+1, ext, sizeof(long_ad));
    ext[0].extLength = 2048 * posInExtent;
    ext[1].extLength = 2048;
    ext[1].extLocation.logicalBlockNum = 0;
    ext[2].extLength -= ext[0].extLength + 2048;
    ext[2].extLocation.logicalBlockNum = lbn + 1;
    return ext+1;
}

int verifyCDR(struct fileEntry *fe) {
    long_ad	*ext, *extents;
    uint32_t	processed;
    int		stat, rv = 0;
    uint32_t	pbn, pbnFE, rewriteBlkno;

    setStrictRead(1);
    extents = ext = (long_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
    processed = 0;
    pbnFE = vat[fe->descTag.tagLocation] + pd->partitionStartingLocation;
    pbn = ext->extLocation.logicalBlockNum + pd->partitionStartingLocation;

    if( (fe->icbTag.flags & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB )
	goto FileEntryOnly;

    do {
	if( pbn > pbnFE ) {
	    if( devicetype == DISK_IMAGE ) {
		printf("Verify %d\n", pbn);
		stat = 0;
//		if( (pbn == 557) || pbn == 565 ) stat = 1;	/* for testing only */
	    } else 
		stat = readCD(device, sectortype, pbn, 1, blockBuffer);

	    if( stat ) {
		printf("readError %d : %s\n", pbn, get_sense_string());
		rv++;
		ext = flagError(extents, ext, pbn);
		processed = ext->extLength - 2048;
	    }
	}
	processed += 2048;
	pbn++;

	if( processed == ext->extLength ) {
	    ext++;
	    pbn = ext->extLocation.logicalBlockNum + pd->partitionStartingLocation;
	    processed = 0;
	}
    } while (processed < ext->extLength);

  FileEntryOnly:
    if( rv == 0 ) {				/* no errors in data, now verify FileEntry */
	int retries;

	for( retries = 0; retries < 3; retries++ ) {
	    if( devicetype == DISK_IMAGE ) {
		printf("Verify %d\n", pbnFE);
		stat = 0;
//		if( pbnFE  == 552 || pbnFE == 553 ) stat = 1;	/* testing only */
	    } else 
		stat = readCD(device, sectortype, pbnFE, 1, blockBuffer);

	    if( stat == 0 ) {
		setStrictRead(0);
		return 0;
	    }
	    printf("readError %d : %s\n", pbnFE, get_sense_string());
	    pbnFE = getNWA();
	    vat[fe->descTag.tagLocation] = pbnFE - pd->partitionStartingLocation;
	    writeCDR(fe);
	}
	printf("verifyCDR: verify FE failed 3 times\n"); 
    }

    setStrictRead(0);

    rewriteBlkno = getNWA() + 1 - pd->partitionStartingLocation;		// NWA itself for revised FileEntry

    fe->lengthAllocDescs = 0;
    for( ext = extents; ext->extLength; ext++ ) {
	fe->lengthAllocDescs += sizeof(long_ad);
	if( ext->extLocation.logicalBlockNum != 0 )
	    continue;
	ext->extLocation.logicalBlockNum = rewriteBlkno;
	rewriteBlkno += ext->extLength >> 11;
	if( ext->extLength & 2047 )
	    break;
    }
    fe->descTag.descCRCLength = sizeof(struct fileEntry) + fe->lengthExtendedAttr + fe->lengthAllocDescs - 16;
    return rv;
}

void readVATtable() {
    uint32_t 	blkno;
    struct fileEntry *fe;

    blkno = getNWA() - (devicetype == DISK_IMAGE ? 1 : 8);
    prevVATlbn = blkno;
    fe = (struct fileEntry*)readTaggedBlock(blkno, ABSOLUTE);

    if( fe->descTag.tagIdent != TAG_IDENT_FE ||  fe->icbTag.fileType != 0 )
	fail("VAT ICB not found\n");
	
    CDRuniqueID  = fe->uniqueID;
    sizeVAT = ((fe->informationLength + 4095) & ~2047);
    vat = malloc(sizeVAT);
    memset(vat, 0xFF, sizeVAT);				// later or not at all
    newVATindex = (fe->informationLength - 36) >> 2;

    if( sizeof(*fe) + fe->lengthExtendedAttr + fe->informationLength <= 2048 ) {
	memcpy(vat, fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr, fe->informationLength - 36);
    } else {
	readExtents((char*)vat, 1, fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
    }
}


void 
writeVATtable() 
{
    regid	*id;
    struct fileEntry *fe;
    uint64_t	i;
    int		stat, retries, size;
    uint32_t	startBlk;
    short_ad	*ext;
    uint16_t	udf_rev_le16;

    retries = 0;

    id = (regid*)(&vat[newVATindex]);
    memset(id, 0, sizeof(regid));
    strcpy((char *)id->ident, UDF_ID_ALLOC);
    udf_rev_le16 = cpu_to_le16(0x150);
    memcpy(id->identSuffix, &udf_rev_le16, sizeof(udf_rev_le16));
    id->identSuffix[2] = UDF_OS_CLASS_UNIX;
    id->identSuffix[3] = UDF_OS_ID_LINUX;
    *(uint32_t*)(id+1) = prevVATlbn;

    fe = makeFileEntry();
    size = (newVATindex + 9) << 2;
    fe->informationLength = cpu_to_le64(size);
    fe->descTag.tagSerialNum = lvid->descTag.tagSerialNum;
    fe->icbTag.fileType = ICBTAG_FILE_TYPE_UNDEF;
    fe->fileLinkCount = 0;

    for( retries = 0; retries < 8; retries++ ) {
	startBlk = getNWA();

	if( sizeof(*fe) + size < 2048 ) {
	    fe->icbTag.flags = ICBTAG_FLAG_AD_IN_ICB;
	    memcpy(fe->extendedAttrAndAllocDescs, vat, size);
	    fe->lengthAllocDescs = cpu_to_le32(size);
	} else {
	    fe->logicalBlocksRecorded = (size + 2047) >> 11;
	    fe->icbTag.flags = ICBTAG_FLAG_AD_SHORT;
	    ext = (short_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
	    ext->extLength = size;
	    ext->extPosition = startBlk  - pd->partitionStartingLocation;
	    fe->lengthAllocDescs = cpu_to_le32(16);
	}

	fe->descTag.tagLocation = startBlk  - pd->partitionStartingLocation + fe->logicalBlocksRecorded;
	fe->descTag.descCRCLength = sizeof(*fe) + fe->lengthExtendedAttr + fe->lengthAllocDescs - sizeof(tag);
	setChecksum(&fe->descTag);

	for( i = 0; i < fe->logicalBlocksRecorded; i++ )
	    writeCDR(vat + (i<<9));

	writeCDR(fe);

	setStrictRead(1);

	for( i = 0; i <= fe->logicalBlocksRecorded; i++ ) {	// "<=" so including FileEntry 
	    if( devicetype == DISK_IMAGE ) {
		printf("Verify %llu\n", (unsigned long long int)(startBlk+i));
		stat = 0;
	    } else
		stat = readCD(device, sectortype, startBlk + i, 1, blockBuffer);

	    if( stat != 0 ) {
		printf("writeVATtable verifyError %llu : %s\n", (unsigned long long int)(startBlk + i), get_sense_string());
		break;
	    }
	}
	if( stat == 0 )
	    break;
    }

    if( retries == 8 )
	printf("*** writeVATtable rewrite FAILED\nLast VAT was at LBN %d\n", prevVATlbn); 

    setStrictRead(0);
}



