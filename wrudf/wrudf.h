/*	wrudf.h
 *
 * PURPOSE
 *  	The header file for wrudf.c and related source files.
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 *	(C) 2001 Enno Fennema <e.fennema@dataweb.nl>
 *
 * HISTORY
 *  	16 Aug 01  ef  created.
 */


#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

#include "ecma_167.h"
#include "osta_udf.h"
#include "libudffs.h"

struct generic_desc
{
	tag		descTag;
	uint32_t	volDescSeqNum;
};

extern char*		hdWorkingDir;
extern int		ignoreReadError;
extern int		device;
extern int		devicetype;
#define DISK_IMAGE	0xAA

enum MEDIUM { CDR = 1, CDRW };

extern enum MEDIUM medium;

// #define CDR		'R'
// #define CDRW		'W'

extern uint32_t		trackSize;

#ifdef USE_READLINE
extern char	*line;
#else
extern char	line[256];
#endif

extern regid	entityWRUDF;
extern timestamp timeStamp;

enum RV { CMND_OK, CMND_FAILED, WRONG_NO_ARGS, CMND_ARG_INVALID, NOT_IN_RW_PARTITION,
	  DIR_INVALID, EXISTING_DIR, EXISTING_FILE, DELETED_DIR, DELETED_FILE, DOES_NOT_EXIST,
	  DIR_NOT_EMPTY, PERMISSION_DENIED, IS_DIRECTORY };

enum CMND { CMND_CP = 50, CMND_RM, CMND_MKDIR, CMND_RMDIR, CMND_LSC, CMND_LSH, CMND_CDC, CMND_CDH, CMND_QUIT };

extern	int	cmndc;
extern	char**	cmndv;

extern	uint32_t	options;
#define OPT_DUMMY	0x01
#define OPT_FORCE	0x02
#define OPT_RECURSIVE	0x04

extern int	spaceMapDirty, usdDirty, sparingTableDirty;

extern struct logicalVolDesc		*lvd;
extern struct partitionDesc		*pd;		/* The (re)writable partition descriptor */
extern uint16_t				virtualPartitionNum;
extern uint32_t				*vat;
extern struct unallocSpaceDesc		*usd;
extern struct spaceBitmapDesc		*spaceMap;
extern struct logicalVolIntegrityDesc	*lvid;
extern struct fileSetDesc		*fsd;
extern unsigned int			usedSparingEntries;
extern struct sparingTable		*st;

typedef struct _dir_ {
    struct _dir_	*parent, *child;
    uint32_t		dataSize;
    char		*data;
    long_ad		icb;				/* icb of this directory itself */
    char		*name;
    uint32_t		dirDirty;
    uint8_t		fe[2048];
}   Directory;

extern Directory		*rootDir, *curDir;


/*wrudf.c */
#ifdef USE_READLINE
char* readLine(char *prompt);
#endif

/* wrudf-cmnd.c */
int	updateDirectory(Directory* dir);
Directory *readDirectory(Directory *parentDir, long_ad *icb, char* name);

int	cpCommand(void);
int	rmCommand(void);
int	mkdirCommand(void);
int	rmdirCommand(void);
int	cdcCommand(void);
int	cdhCommand(void);
int	lscCommand(void);
int	lshCommand(void);

/* wrudf-desc.c */
struct fileIdentDesc*	makeFileIdentDesc(char* name);
struct fileIdentDesc*	findFileIdentDesc(Directory *dir, char* name);
int			deleteFID(Directory *dir, struct fileIdentDesc *fid);
int			removeFID(Directory *dir, struct fileIdentDesc *fid);
int			insertFileIdentDesc(Directory *dir, struct fileIdentDesc* fid);
struct fileEntry*	makeFileEntry();


/* wrudf-cdrw.c */
enum markAction { FREE, ALLOC };
void markBlock(enum markAction action, uint32_t blkno);

extern	int		lastTrack;
extern	int		sectortype;
extern  struct cdrom_trackinfo	ti;

int	getExtents(uint32_t requestedLength, short_ad *extents);
int	freeShortExtents(short_ad* extent);
int	freeLongExtents(long_ad* extent);

void	getUnallocSpaceExtent(uint32_t requestLength, uint32_t requestAfter, extent_ad *alloc);

void 	setChecksum(void *descriptor);
void	updateTimestamp(time_t t, uint32_t  ut);		/* current time if t = 0 */
void	setStrictRead(int yes);
uint32_t	getPhysical(uint32_t lbn, uint16_t part);
void	updateSparingTable();

#define ABSOLUTE	0xFFFF				/* ignore partition, process physical blocknumber */
/*	actually 0xFFFF is a perfectly valid partition number
 *	could make part a uint32_t and use 0xFFFFFFFF 
 *	or have write absolute block routine
*/

#define INVALID		0xFFFFFFFF

void*	readBlock(uint32_t lbn, uint16_t part);
void	freeBlock(uint32_t lbn, uint16_t part);
void	dirtyBlock(uint32_t lbn, uint16_t part);
void	writeBlock(uint32_t lbn, uint16_t part, void* src);
void* 	readSingleBlock(uint32_t pbn);
void* 	readTaggedBlock(uint32_t lbn, uint16_t part);
int	readExtents(char* dest, int usesShort, void* extents);
int	writeExtents(char* src, int usesShort, void* extents);

int	initIO(char *filename);
int	closeIO();

/* wrudf-cdr.c */
uint32_t	newVATentry();
uint32_t	getNWA();
uint32_t	getMaxVarPktSize();
uint32_t	writeCDR(void* src);
void	syncCDR();
void	writeHDlink();
unsigned char*	readCDR(uint32_t lbn, uint16_t partition);
int	verifyCDR(struct fileEntry *fe);
void	readVATtable();
void	writeVATtable();

/* ide-pc.h */
void	fail(char* fmt, ...);
