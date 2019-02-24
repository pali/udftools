/*	wrudf.c
 *
 *	Maintains a UDF filing system
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <sys/resource.h>

#ifdef USE_READLINE
#include <readline/readline.h>
#endif

#include "wrudf.h"

char	*devicename;			/* "/dev/cdrom" or disk image filename */
int	device;				/* the file descriptor */
int	devicetype;
enum MEDIUM medium;
int	ignoreReadError;		/* used while reading VRS which may be absent on open CDR */

#ifdef USE_READLINE
char	*line;
#define	GETLINE(prompt) readLine(prompt);
#else
char	line[256];
#define GETLINE(prompt) do { printf("%s", prompt); if (fgets(line, 256, stdin)) *strchr(line, '\n') = 0; else line[0] = 0; } while (0)
#endif


int	cmndc;
int	cmndvSize;
char**	cmndv;

#define FOUND_BEA01	1
#define FOUND_NSR02	1<<1
#define FOUND_TEA01	1<<2
#define FOUND_PVD	1<<8
#define FOUND_LVD	1<<9
#define FOUND_PD	1<<10
#define FOUND_USD	1<<11 
uint32_t	found;

uint32_t	options;


Directory	*rootDir, *curDir;
timestamp	timeStamp;
regid		entityWRUDF = { 0, "-wrudf (" PACKAGE_VERSION ")", "\x04\x05"};

extent_ad	extentMainVolDescSeq;
extent_ad	extentRsrvVolDescSeq;
extent_ad	extentNextVolDescSeq;
extent_ad	extentLogVolIntegritySeq;
uint32_t	integrityDescBlocknumber;		/* where to write LVID when finished */
extent_ad	extentLogVolIntegrityDesc;		/* (continuation) extent for current LVIDs */

extern	uint64_t  CDRuniqueID;				/* in wrudf-cdr.c ex VAT FE */


struct partitionDesc		*pd;			/* for the writable partition */
uint16_t			virtualPartitionNum  = 0xFFFF;
uint32_t			*vat;
struct logicalVolDesc		*lvd;
struct unallocSpaceDesc		*usd;
struct spaceBitmapDesc		*spaceMap;
struct logicalVolIntegrityDesc	*lvid;
struct fileSetDesc		*fsd;
unsigned int			usedSparingEntries;
struct sparingTable		*st;

int	spaceMapDirty, usdDirty, sparingTableDirty;


#ifdef USE_READLINE
char* readLine(char* prompt) {
    if( line ) {
	free(line);
    }
    return line = readline(prompt);
}
#endif


void 
initialise(char *devicename) 
{
    uint32_t			i, len, blkno, lastblk, size;
    long_ad			*fsdAd;
    short_ad			*adSpaceMap;
    struct sparablePartitionMap *spm;
    char			zeroes[5];
    char                        fsdOut[91];
    struct generic_desc 	*p;
    struct volStructDesc	*vsd;
    struct partitionHeaderDesc	*phd;
    struct logicalVolHeaderDesc	*lvhd;

    initIO(devicename);
    memset(zeroes, 0, 5);

    /* read Volume Recognition Sequence */
    ignoreReadError = 1;
    for( blkno = 16; blkno < 256; blkno++ ) {
	vsd = readSingleBlock(blkno);

	if( vsd == NULL || memcmp(vsd->stdIdent, zeroes, 5) == 0 )
	    break;

	if( strncmp((char *)vsd->stdIdent, "BEA01", VSD_STD_ID_LEN) == 0 ) {
	    found |= FOUND_BEA01;
	    continue;
	}
	if( strncmp((char *)vsd->stdIdent, "NSR02", VSD_STD_ID_LEN) == 0 ) {
	    found |= FOUND_NSR02;
	    continue;
	}
	if( strncmp((char *)vsd->stdIdent, "TEA01", 5) == 0 ) {
	    found |= FOUND_TEA01;
	    continue;
	}
    }
    ignoreReadError = 0;

    if( !(found & FOUND_BEA01) || !(found & FOUND_NSR02) || !(found & FOUND_TEA01) )
	printf("No UDF VRS\n");

    p = NULL;

    if( medium == CDR ) {
	p = readSingleBlock(512);
	if( p == NULL || p->descTag.tagIdent != TAG_IDENT_AVDP )
	    fail("No AVDP at block 512 on CDR disc\n");
    }

    if( !p ) { 
	p = readTaggedBlock(256, ABSOLUTE);
	if( p->descTag.tagIdent != TAG_IDENT_AVDP ) {
	    p = readTaggedBlock(trackSize - 1, ABSOLUTE);
	    if( p->descTag.tagIdent != TAG_IDENT_AVDP ) {
		p = readTaggedBlock(trackSize - 256, ABSOLUTE);
		if( p->descTag.tagIdent != TAG_IDENT_AVDP )
		    fail("No AVDP at block 256, N-256 or N-1 \n");
	    }	
	}
    }

    extentMainVolDescSeq = ((struct anchorVolDescPtr*)p)->mainVolDescSeqExt;
    extentRsrvVolDescSeq = ((struct anchorVolDescPtr*)p)->reserveVolDescSeqExt;

    /* read Volume Descriptor Sequence */
    blkno = extentMainVolDescSeq.extLocation;
    len = extentMainVolDescSeq.extLength;

    for( i = 0; i < len; blkno++, i += 2048 ) {
	int	inMainSeq = 1;

	if( (p = readTaggedBlock(blkno, ABSOLUTE)) == NULL ) {
	    if( !inMainSeq ) 
		fail("Volume Descriptor Sequences read failure\n");
	    blkno = extentRsrvVolDescSeq.extLocation;
	    len = extentRsrvVolDescSeq.extLength;
	    inMainSeq = 0;
	    i = 0;
	}
	switch( p->descTag.tagIdent ) {
	case TAG_IDENT_PVD:
	    found |= FOUND_PVD;
	    break;
	case TAG_IDENT_VDP:
	    blkno = ((struct volDescPtr*)p)->nextVolDescSeqExt.extLocation - 1;
	    len   = ((struct volDescPtr*)p)->nextVolDescSeqExt.extLength;
	    i = (uint32_t) -2048;
	    break;
	case TAG_IDENT_IUVD:
	    break;
	case TAG_IDENT_PD:
	    /* must have one (re)writable partition */
	    /* may have at most one RDONLY partition */
	    switch( ((struct partitionDesc *)p)->accessType ) {
	    case PD_ACCESS_TYPE_READ_ONLY:
		break;
	    case PD_ACCESS_TYPE_REWRITABLE:
	    case PD_ACCESS_TYPE_WRITE_ONCE:
		found |= FOUND_PD;
		if( !pd )
		    pd  = (struct partitionDesc*) calloc(512, 1);
		if( p->volDescSeqNum > pd->volDescSeqNum )
		    memcpy(pd, p, 512);
		break;
	    case PD_ACCESS_TYPE_OVERWRITABLE:
		printf("Partition with overwritable accesstype is not supported\n");
		break;
	    default:
		printf("What to do with an accesstype %d partition?\n", 
		    ((struct partitionDesc*)p)->accessType);
		break;
	    }
	    break;
	case TAG_IDENT_LVD:
	    found |= FOUND_LVD;
	    if( !lvd )
		lvd  = (struct logicalVolDesc*) calloc(512, 1);
	    if( p->volDescSeqNum > lvd->volDescSeqNum )
		memcpy(lvd, p, 512);
	    break;
	case TAG_IDENT_USD:
	    found |= FOUND_USD;
	    if( !usd )
		usd = (struct unallocSpaceDesc*) calloc(512, 1);
	    if( p->volDescSeqNum > usd->volDescSeqNum )
		memcpy(usd, p, 512);
	    break;
	case TAG_IDENT_TD:
	    i = len;
	    break;
	default:
	    printf("Unexpected tag ID %04X\n in Volume Descriptor Sequence block %d", p->descTag.tagIdent, blkno);
	}
    }

    if( (found & FOUND_LVD) == 0 )
	fail("No LVD found \n");

    if( (found & FOUND_PD) == 0 )
	fail("No PD found\n");

    if( lvd->logicalBlockSize != 2048 )
	fail("Blocksize not 2048\n");

    spm  = (struct sparablePartitionMap*)lvd->partitionMaps;
    for( i = 0; i < lvd->numPartitionMaps; i++ ) {
	if( spm->partitionMapType == 2 ) {

	    if( strncmp((char *)spm->partIdent.ident, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE)) == 0 ) {
		unsigned int	j;

		if( spm->sizeSparingTable > 2048 )
		    fail("Cannot handle SparingTable > 2048 bytes");
		st = (struct sparingTable*)malloc(2048);
		p = readTaggedBlock(spm->locSparingTable[0], ABSOLUTE);
		memcpy(st, p, spm->sizeSparingTable);

		/* if #1 fails, try #2; should pick copy with highest sequenceNum */

		for( j = usedSparingEntries = 0; j < (spm->sizeSparingTable >> 3); j++ ) {
		     if( st->mapEntry[j].origLocation < 0xFFFFFFF0 )
			 usedSparingEntries++;
		}
	    } else if( strncmp((char *)spm->partIdent.ident, UDF_ID_VIRTUAL, strlen(UDF_ID_VIRTUAL)) == 0 )
		virtualPartitionNum = i;
	}
	spm = (struct sparablePartitionMap*)((char*)spm + spm->partitionMapLength);
    }

    if( medium == CDR ) {
	if( virtualPartitionNum != 0xFFFF )
	    readVATtable();
	else 
	    fail("No Virtual Partition Map on CDR\n");
    }

    if( medium != CDR && (found & FOUND_USD) == 0 )
	fail("Did not find Unallocated Space Descriptor\n");

    /* Read Fileset Descriptor Sequence */
    fsdAd = (long_ad*) lvd->logicalVolContentsUse;
    for( i = 0; i < (fsdAd->extLength >> 11); i++ ) {
	blkno = pd->partitionStartingLocation + fsdAd->extLocation.logicalBlockNum + i;
	if( !(p = readTaggedBlock(blkno, ABSOLUTE)) ) 
	    exit(1);
	switch( p->descTag.tagIdent ) {
	case TAG_IDENT_TD:
	    i = fsdAd->extLength;
	    break;
	case TAG_IDENT_FSD:
	    if( fsd && (lvd->volDescSeqNum > p->volDescSeqNum) )
		break;
	    fsd = (struct fileSetDesc*)malloc(512);
	    memcpy(fsd, p, 512);
	    break;
	default:
	    printf("Unxpected tag id %d, where File Set Desc(256) expected\n", p->descTag.tagIdent);
	}
    }

    if( !fsd )
	fail("No File Set Descriptor\n");

    /* load Spacemap extent */
    phd = (struct partitionHeaderDesc*)pd->partitionContentsUse;
    adSpaceMap = &phd->unallocSpaceBitmap;

    if( adSpaceMap->extLength != 0 ) {
	blkno = adSpaceMap->extPosition;
	len = adSpaceMap->extLength;
	spaceMap = (struct spaceBitmapDesc*) malloc((len + 2047) & ~2047);

	for( i = 0; i < len; i += 2048 ) {
	    p = readBlock(blkno, 0);
	    memcpy( (uint8_t*)spaceMap + i, (uint8_t*) p, 2048);
	    freeBlock(blkno++, 0);
	}

	if( spaceMap->descTag.tagIdent != TAG_IDENT_SBD )
	    fail("SpaceBitmap not found\n");
    }

    if (decode_string(NULL, fsd->fileSetIdent, fsdOut, sizeof(fsd->fileSetIdent), sizeof(fsdOut)) == (size_t)-1)
        fsdOut[0] = 0;

    printf("You are going to update fileset '%s'\nProceed (y/N) : ", fsdOut);
    GETLINE("");

#ifdef USE_READLINE
    if( !line )
	fail("wrudf terminated\n");
#endif

    if( line[0] != 'y' )
	fail("wrudf terminated\n");

    /* Read Logical Volume Integrity sequence */
    blkno = lvd->integritySeqExt.extLocation;
    lastblk = blkno + (lvd->integritySeqExt.extLength >> 11);
    extentLogVolIntegrityDesc = lvd->integritySeqExt;

    for(   ; blkno < lastblk; blkno++ ) {
	if( !(p = readTaggedBlock(blkno, ABSOLUTE))  ) 
	    fail("Read failure in Integrity Sequence, blk %d\n", blkno);

	switch( p->descTag.tagIdent ) {
	case TAG_IDENT_TD:
	    blkno = lastblk;
	    break;
	case TAG_IDENT_LVID:
	    size = sizeof(struct logicalVolIntegrityDesc) + sizeof(struct logicalVolIntegrityDescImpUse)
		+ 2 * sizeof(uint32_t) * ((struct logicalVolIntegrityDesc*)p)->numOfPartitions;
	    if( !lvid )
		lvid = (struct logicalVolIntegrityDesc*) malloc(size);
	    integrityDescBlocknumber = blkno;
	    memcpy(lvid, p, size);

	    if( lvid->nextIntegrityExt.extLocation ) {
		extentLogVolIntegrityDesc = lvid->nextIntegrityExt;
		blkno = lvid->nextIntegrityExt.extLocation;
		lastblk = blkno + (lvid->nextIntegrityExt.extLength >> 11);
		blkno--;					/* incremented again in for statement */
	    }
	    break;
	default:
	    printf("Unxpected tag %X in Integrity Sequence; blk %d\n", p->descTag.tagIdent, blkno);
	    blkno = lastblk;
	    break;
	}
    }

    if( !lvid || lvid->descTag.tagIdent != TAG_IDENT_LVID  )
	fail("No Logical Volume Integrity Descriptor\n");

    if( medium == CDR && lvid->integrityType == LVID_INTEGRITY_TYPE_CLOSE )
	fail("CDR volume has been closed\n");

    if( medium == CDR )
    {
	// take from VAT FileEntry
	lvhd = (struct logicalVolHeaderDesc*)lvid->logicalVolContentsUse;
	lvhd->uniqueID = CDRuniqueID;
    }
    
    curDir = rootDir = (Directory*)malloc(sizeof(Directory));
    memset(rootDir, 0, sizeof(Directory));
    rootDir->dataSize = 4096;
    rootDir->data = malloc(4096);
    rootDir->name = "";
    readDirectory( NULL, &fsd->rootDirectoryICB, "");

    if( medium == CDR )
	return;

    if( lvid->integrityType == LVID_INTEGRITY_TYPE_OPEN ) {
	GETLINE("** Volume was not closed; do you wish to proceed (y/N) : ");
	if( (line[0] | 0x20) != 'y' )
	    exit(0);
    }

    /*  If prevailing LVID is in one before last block of extent
     *	then next LVID will be in last and must contain continuation extent */

    if( integrityDescBlocknumber + 2 ==    
	extentLogVolIntegrityDesc.extLocation + (extentLogVolIntegrityDesc.extLength >> 11) ) 
    {
	getUnallocSpaceExtent( 32 * 2048, extentLogVolIntegrityDesc.extLocation, &lvid->nextIntegrityExt);
	if( lvid->nextIntegrityExt.extLength == 0 ) {
	    printf("No more unallocated space for Integrity Sequence\n");
	}
    } else {
	lvid->nextIntegrityExt.extLength = 0;
	lvid->nextIntegrityExt.extLocation = 0;
    }

    integrityDescBlocknumber++;

    updateTimestamp(0,0);
    memcpy(lvid->data + 2 * sizeof(uint32_t) * lvid->numOfPartitions, &entityWRUDF, sizeof(regid));
    lvid->recordingDateAndTime = timeStamp;
    lvid->integrityType = LVID_INTEGRITY_TYPE_OPEN;
    lvid->descTag.tagLocation = integrityDescBlocknumber;

    size = sizeof(struct logicalVolIntegrityDesc) + sizeof(struct logicalVolIntegrityDescImpUse)
	+ 2 * sizeof(uint32_t) * lvid->numOfPartitions;
    
    lvid->descTag.descCRCLength = size - sizeof(tag);
    setChecksum(lvid);

    p = readTaggedBlock(integrityDescBlocknumber, ABSOLUTE);
    memcpy( p, lvid, size);
    dirtyBlock(integrityDescBlocknumber, ABSOLUTE);	/* not written because of buffering : Force unit access? */

}


int 
finalise(void) 
{
    int		i, lbn, len, size, blkno ;
    struct generic_desc 	*p;
    short_ad	*adSpaceMap;
    struct partitionHeaderDesc *phd;

    updateDirectory(rootDir);				/* and any dirty children */

    if( medium == CDR ) {
	writeVATtable();
    } else {
	/* rewrite Space Bitmap */
	if( spaceMapDirty) {
	    phd = (struct partitionHeaderDesc*)pd->partitionContentsUse;
	    adSpaceMap = &phd->unallocSpaceBitmap;
	    lbn = adSpaceMap->extPosition;
	    len = adSpaceMap->extLength;

	    for( i = 0; i < len; i += 2048 ) {
		p = readBlock(lbn, 0);
		memcpy( p, (uint8_t*)spaceMap + i, 2048);
		dirtyBlock(lbn, 0);
		freeBlock(lbn++, 0);
	    }
	}

	if( sparingTableDirty )
	    updateSparingTable();

	/* write closed Logical Volume Integrity Descriptor */
	lvid->integrityType = LVID_INTEGRITY_TYPE_CLOSE;
	updateTimestamp(0,0);
	lvid->recordingDateAndTime = timeStamp;
	lvid->descTag.tagLocation = integrityDescBlocknumber;
	size = sizeof(struct logicalVolIntegrityDesc) + sizeof(struct logicalVolIntegrityDescImpUse) 
	    + 2 * sizeof(uint32_t) * lvid->numOfPartitions;
	lvid->descTag.descCRCLength = size - sizeof(tag);
	setChecksum(lvid);
	p = readBlock(integrityDescBlocknumber, ABSOLUTE);
	memcpy(p, lvid, size);
	dirtyBlock(integrityDescBlocknumber, ABSOLUTE);
	freeBlock(integrityDescBlocknumber, ABSOLUTE);

	/* terminating descriptor */
	blkno = lvid->nextIntegrityExt.extLocation;
	if( !blkno )
	    blkno = integrityDescBlocknumber + 1;

	p = readBlock(blkno, ABSOLUTE);
	memset(p, 0, 2048);
	p->descTag.tagLocation = blkno;
	p->descTag.tagSerialNum = lvid->descTag.tagSerialNum;
	p->descTag.tagIdent = TAG_IDENT_TD;
	p->descTag.descCRCLength = 512 - sizeof(tag);
	setChecksum(p);
	dirtyBlock(blkno, ABSOLUTE);
	freeBlock(blkno, ABSOLUTE);

	if( usdDirty ) {
	    size = sizeof(struct unallocSpaceDesc) + usd->numAllocDescs * sizeof(extent_ad);
	    usd->descTag.descCRCLength = size - sizeof(tag);
	    setChecksum(usd);
	    p = readBlock(usd->descTag.tagLocation, ABSOLUTE);
	    memcpy(p, usd, size);
	    dirtyBlock(usd->descTag.tagLocation, ABSOLUTE);
	    freeBlock(usd->descTag.tagLocation, ABSOLUTE);

	    /* now rewrite in Reserve Sequence which must be exact copy of Main Sequence */
	    usd->descTag.tagLocation +=
		extentRsrvVolDescSeq.extLocation - extentMainVolDescSeq.extLocation;
	    setChecksum(usd);
	    p = readBlock(usd->descTag.tagLocation, ABSOLUTE);
	    memcpy(p, usd, size);
	    dirtyBlock(usd->descTag.tagLocation, ABSOLUTE);
	    freeBlock(usd->descTag.tagLocation, ABSOLUTE);
	}
    } // end not CDR				

    closeIO();						/* clears packet buffers; closes device */

    /* free allocated space */
    if(pd)  free(pd);
    if(lvd) free(lvd);
    if(fsd) free(fsd);
    if(usd) free (usd);
    if(spaceMap) free(spaceMap);
    if(lvid) free(lvid);
    if(st)  free(st);
    if(vat) free(vat);
    return 0;
}

int
parseCmnd(char* line) 
{
    char	*p, *q, next, term;
    int		i, j;
    int		cmnd;

    if( !cmndvSize ) {
	cmndvSize = 64;
	cmndv = malloc(cmndvSize * sizeof(char*));
    }

    if( !line || line[0] == 0 )
	return CMND_FAILED;

    cmndc = 0;
    for( p = line; *p == ' '; p++ )   ;
    for( q = p; *q && (*q != ' '); q++ )   ;
    next = *q;
    *q = 0;

    if( !strcmp(p, "cd") || !strcmp(p, "ls") ) {
	printf("Specify cdh/lsh or cdc/lsc for Harddisk or CompactDisc\n");
	return CMND_FAILED;
    }

    if(      !strcmp(p, "cp") )    cmnd = CMND_CP;
    else if( !strcmp(p, "rm") )    cmnd = CMND_RM;
    else if( !strcmp(p, "mkdir") ) cmnd = CMND_MKDIR;
    else if( !strcmp(p, "rmdir") ) cmnd = CMND_RMDIR;
    else if( !strcmp(p, "lsc") )   cmnd = CMND_LSC;
    else if( !strcmp(p, "lsh") )   cmnd = CMND_LSH;
    else if( !strcmp(p, "cdc") )   cmnd = CMND_CDC;
    else if( !strcmp(p, "cdh") )   cmnd = CMND_CDH;
    else if( !strcmp(p, "quit") )  cmnd = CMND_QUIT;
    else if( !strcmp(p, "exit") )  cmnd = CMND_QUIT;
    else if( !strcmp(p, "help") ) {
	printf(
	"Available commands:\n"
	"\tcp\n"
	"\trm\n"
	"\tmkdir\n"
	"\trmdir\n"
	"\tlsc\n"
	"\tlsh\n"
	"\tcdc\n"
	"\tcdh\n"
	"Specify cdh/lsh or cdc/lsc to do cd or ls for Harddisk or CompactDisc.\n"
	"\tquit\n"
	"\texit\n"
	);
	return CMND_FAILED;
    } else {
	printf("Invalid command\n");
	return CMND_FAILED;
    }

    while( next != 0 ) {
	if( cmndc == cmndvSize ) {
	    cmndvSize += 32;
	    cmndv = realloc(cmndv, cmndvSize);
	    if( !cmndv )
		fail("cmndv reallocation failed\n");
	}
	p = q + 1;
	while( *p == ' ') p++;
	if( *p == 0 ) 
	    break;
	if( *p == '"' ) {
	    term = '"'; q = p+1;
	} else {
	    term = ' '; q = p;
	}
	while( *q && *q != term ) q++;

	next = *q;
	if( *p == '"' ) {
	    if( *q == 0 )
		return CMND_ARG_INVALID;
	    cmndv[cmndc++] = p+1;
	    *(q-1) = 0;
	} else {
	    cmndv[cmndc++] = p;
	    *q = 0;
	}
    }

    options = 0;
    for( i = 0; i < cmndc; i ++ ) {
	if( cmndv[i][0] == '-' ) {
	    for( j = 1; cmndv[i][j]; j++ ) {
		switch( cmndv[i][j] ) {
		case 'f':
		    options |= OPT_FORCE;
		    break;
		case 'r':
		    options |= OPT_RECURSIVE;
		    break;
		default:
		    return CMND_ARG_INVALID;
		}
	    }
	    cmndv[i] = NULL;
	}
    }

    for( i = j = 0; i < cmndc; i++ ) {
	if( i != j )
	    cmndv[j] = cmndv[i];
	if( cmndv[j] != NULL )
	    j++;
    }

    cmndc = j;

    /* could check whether options valid for command */

    return cmnd;
}

int show_help()
{
	char *msg =
	"Interactive tool to maintain a UDF filesystem.\n"
	"Usage:\n"
	"\twrudf [device]\n"
	"Available commands:\n"
	"\tcp\n"
	"\trm\n"
	"\tmkdir\n"
	"\trmdir\n"
	"\tlsc\n"
	"\tlsh\n"
	"\tcdc\n"
	"\tcdh\n"
	"Specify cdh/lsh or cdc/lsc to do cd or ls for Harddisk or CompactDisc.\n"
	"\tquit\n"
	"\texit\n";
	printf("%s", msg);
	return 0;
}

int
main(int argc, char** argv) 
{ 
    int	 	rv=0;
    int		cmnd;
    char	prompt[256];
    char	*ptr;
    size_t	len;
    Directory	*d;

    setlocale(LC_CTYPE, "");

    printf("wrudf from " PACKAGE_NAME " " PACKAGE_VERSION "\n");
    devicename= "/dev/cdrom";

    if( argc > 2 || (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-help") || !strcmp(argv[1], "--help"))) )
	return show_help();
    else if( argc == 2 )
	devicename = argv[1];			/* can specify disk image filename */

    if( setpriority(PRIO_PROCESS, 0, -10) ) {
	printf("setpriority(): %s\n", strerror(errno));
    }

    hdWorkingDir = getcwd(NULL, 0);
    initialise(devicename);

    for(;;) {
	d = rootDir;
	prompt[0] = 0;
	ptr = prompt;
	while( curDir != d ) { 
	    len = strlen(d->name);
	    if( ptr + len + 1 >= prompt + sizeof(prompt) - 7 )
	        break;
	    memcpy(ptr, d->name, len);
	    ptr[len] = '/';
	    ptr += len + 1;
	    d = d->child;
	}
	len = strlen(d->name);
	if( ptr + len + 1 >= prompt + sizeof(prompt) - 7 ) {
	    memcpy(ptr, "...", 3);
	    ptr += 3;
	} else if( d->name[0] == 0 ) {
	    *(ptr++) = '/';
	} else {
	    memcpy(ptr, d->name, len);
	    ptr += len;
	}

	memcpy(ptr, " > ", 4);
	
	GETLINE(prompt);

	cmnd = parseCmnd(line);

	if( cmnd == CMND_FAILED )
	    continue;

	if( cmnd == CMND_QUIT )
	    break;

	switch( cmnd ) {
	case CMND_CP:
	    rv = cpCommand();
	    break;
	case CMND_RM:
	    rv = rmCommand();
	    break;
	case CMND_MKDIR:
	    rv = mkdirCommand();
	    break;
	case CMND_RMDIR:
	    rv = rmdirCommand();
	    break;
	case CMND_LSC:
	    rv = lscCommand();
	    break;
	case CMND_LSH:
	    rv = lshCommand();
	    break;
	case CMND_CDC:
	    rv = cdcCommand();
	    break;
	case CMND_CDH:
	    rv = cdhCommand();
	    break;
	}

	switch( rv ) {
	case CMND_OK:
	    break;
	case CMND_FAILED:
	    printf("Failed\n");
	    break;
	case WRONG_NO_ARGS:
	    printf("Incorrect number of arguments\n");
	    break;
	case CMND_ARG_INVALID:
	    printf("Invalid argument\n");
	    break;
	case NOT_IN_RW_PARTITION:
	    printf("Not in RW partition\n");
	    break;
	case DIR_INVALID:
	    printf("Invalid directory\n");
	    break;
	case DIR_NOT_EMPTY:
	    printf("Directory not empty\n");
	    break;
	case EXISTING_DIR:
	    printf("Dir already exists\n");
	    break;
	case DOES_NOT_EXIST:
	    printf("File/directory does not exist\n");
	    break;
	case PERMISSION_DENIED:
	    printf("Permission denied\n");
	    break;
	case IS_DIRECTORY:
	    printf("Is a directory\n");
	    break;
	default:
	    printf("Unknown return value %d\n", rv);
	}
    }
    free(hdWorkingDir);
    return finalise();
}
