#include "chkudf.h"
/* 
 * Function prototypes for all files 
 */

/*****************************************************************************
 * build_scsi.c
 * 
 * This module contains routines to build SCSI CDBs.
 *
 * the scsi_modesense10 function builds a mode sense scsi command in a
 * buffer passed to the routine.  
 *
 * The scsi_read10 command builds a read command in the preallocated
 * buffer.  It returns a pointer to the buffer.
 ****************************************************************************/ 
char *scsi_modesense10(char *buffer, int DBD, int PC, int pagecode, 
                  int pagelength);

char *scsi_read10(char *buffer, int LBA, int length, int sectorsize,
                  int DPO, int FUA, int RelAdr);



/*****************************************************************************
 * checkTag.c
 * 
 * Verifies that the structure is a tag, is in uTagLoc, and has the
 * requested TagID.  The CRC length must be between min and max inclusive.
 * If the TagID is -1, everything but the TagID is checked.  The return
 * value is CHECKTAG_NOT_TAG, CHECKTAG_TAG_DAMAGED, or CHECKTAG_TAG_GOOD to
 * indicate that the routine thinks the data is unlikely to be a tag, 
 * likely to be a tag but has a small problem, or is a good tag.
 ****************************************************************************/

int CheckTag(struct tag *TagPtr, UINT32 uTagLoc, UINT16 TagID,
               int crc_min, int crc_max);

/*****************************************************************************
 * chkudf.c
 *
 * This module contains no exported functions (just main).
 ****************************************************************************/


/*****************************************************************************
 * cleanup.c
 *
 * The cleanup command frees all memory allocated in the course of execution.
 ****************************************************************************/

void cleanup(void);


/*****************************************************************************
 * display_dirs.c
 *
 * The display_dirs function shows a recursive listing of the directory
 *  structure.
 ****************************************************************************/

int GetRootDir(void);
int DisplayDirs(void);
int GetFID(struct FileIDDesc *FID, struct FileEntry *fe, UINT16 part, int offset);

/*****************************************************************************
 * do_scsi.c
 *
 * This function issues SCSI commands
 ****************************************************************************/

BOOL do_scsi(UINT8 *command, int cmd_len, UINT8 *buffer, UINT32 in_len,
             UINT32 out_len, UINT8 *sense, int sense_len);

/*****************************************************************************
 * errors.c
 *
 * This function dumps the coded error from the error structure to the
 * display in a human readable form.
 ****************************************************************************/

void DumpError(void);
void ClearError(void);


/*****************************************************************************
 * filespace.c
 *
 * This routine checks file space assignments
 ****************************************************************************/

int track_filespace(UINT16 ptn, UINT32 Location, UINT32 Length);
int check_filespace(void);
int check_uniqueid(void);

/*****************************************************************************
 * getMap.c
 *
 * This routine loads the sparing maps if appropriate.
 ****************************************************************************/

void GetMap(void);


/*****************************************************************************
 * getVAT.c
 *
 * This routine loads the VAT if a virtual partition exists.
 ****************************************************************************/

void GetVAT(void);


/*****************************************************************************
 * globals.c
 *
 * The following are global variables used by chkudf.
 ****************************************************************************/

extern UINT32      blocksize;
extern MUINT8      bdivshift;
extern UINT32      secsize;
extern MUINT8      sdivshift;
extern MUINT8      s_per_b;
extern UINT32      packet_size;
extern BOOL        scsi;
extern int         device;
extern UINT32      LastSector;
extern BOOL        LastSectorAccurate;
extern UINT32      SS;
extern BOOL        isType5;
extern BOOL        isCDRW;

extern UINT8       *scsibuf;
extern UINT32      scsibufsize;
extern UINT8       cdb[];
extern UINT8       sensedata[];
extern int         sensebufsize;

extern sCacheData  Cache[];
extern MUINT8      bufno;
extern sError      Error;
extern char       *Error_Msgs[];


extern UINT16      UDF_Version;
extern BOOL        Version_OK;
extern UINT16      Serial_No;
extern BOOL        Serial_OK;
extern BOOL        Fatal;

extern UINT32      VDS_Loc, VDS_Len, RVDS_Loc, RVDS_Len;
extern sPart_Info  Part_Info[];
extern UINT16      PTN_no;
extern dstring     LogVolID[];      //The logical volume ID
extern MUINT32     VolSpaceListLen;
extern struct extent_ad_name VolSpace[];
extern UINT32      *VAT;
extern UINT32       VATLength;

extern struct long_ad FSD;
extern struct long_ad RootDirICB;
extern sICB_trk      *ICBlist;
extern MUINT32        ICBlist_len;
extern MUINT32        ICBlist_alloc;
extern UINT32         ID_Dirs;
extern UINT32         ID_Files;
extern UINT32         ID_UID;
extern UINT32         Num_Dirs;
extern UINT32         Num_Files;
extern UINT32         Num_Type_Err;
extern UINT32         FID_Loc_Wrong;


/*****************************************************************************
 * icbspace.c
 *
 * This routine tracks ICBs, link counts, and file space
 ****************************************************************************/

int read_icb(struct FileEntry *FE, UINT16, UINT32 Location, UINT32 Length, 
             int FID);


/*****************************************************************************
 * init.c
 *
 * This routine initializes any global variables that need it.
 ****************************************************************************/

void initialize(void);


/*****************************************************************************
 * linkcount.c
 *
 * This routine checks the link count of the file entries.
 ****************************************************************************/

int TestLinkCount(void);


/*****************************************************************************
 * readSpMap.c
 *
 * This routine reads and verifies a space allocation bitmap
 ****************************************************************************/

int ReadSpaceMap(void);

/*****************************************************************************
 * read_udf.c
 *
 * This routine is the start of the logical checks.
 ****************************************************************************/

void Check_UDF(void);


/*****************************************************************************
 * setSectorSize.c
 *
 * This routine attempts to determine the sector size, and sets the secsize
 * and sdivshift globals.
 ****************************************************************************/

void SetSectorSize(void);


/*****************************************************************************
 * setFirstSector.c
 *
 * SetFirstSector obtains the start address of the last session for CD/DVD
 * media.
 ****************************************************************************/

void SetFirstSector(void);

/*****************************************************************************
 * setLastSector.c
 *
 * SetLastSector tries a bunch of tricks to find the last sector.  It sets
 * the LastSector and LastSectorAccurate global variables.
 *
 * SetLastSectorAccurate adjusts LastSector after probing the media a bit.
 * It looks for AVDP in a variety of locations, and attempts to identify
 * CD-RW media formatted with fixed packets.  This routine also sets the
 * isCDRW flag based on its guesses.
 ****************************************************************************/

void SetLastSector(void);

void SetLastSectorAccurate(void);


/*****************************************************************************
 * utils.c
 *
 * Miscellaneous small routines.
 * endian32 swaps a 32 bit integer from big endian to little or vice versa.
 ****************************************************************************/

UINT32 endian32(UINT32 toswap);
UINT16 endian16(UINT16 toswap);
UINT16 doCRC(UINT8 *buffer, int n);
int Is_Charspec(struct charspec chars);
void printDstring( char *start, UINT8 fieldLen);
void printDchars( char *start, UINT8 length);
void printCharSpec(struct charspec chars);
void printTimestamp( struct timestamp x);
void printExtentAD(struct extent_ad extent);
void printLongAd(struct long_ad *longad);

/*****************************************************************************
 * utils_read.c
 *
 * The ReadSectors command performs reading from a logical device or
 * file.  It depends on several global variables, including secsize.
 *
 * The ReadLBlocks command reads blocks from a partition.  It relies on 
 * ReadSectors and several globals, including blocksize.
 *
 * The ReadFileData command reads data from a file.  It relies on 
 * ReadLBlocks.
 ****************************************************************************/

int ReadSectors(void *buffer, UINT32 address, UINT8 Count);

int ReadLBlocks(void *buffer, UINT32 address, UINT16 partition, UINT8 Count);

int ReadFileData(void *buffer, struct FileEntry *ICB, UINT16 part, 
                 int offset, int Count, UINT32 *data_start_loc);

/*****************************************************************************
 * verifyAVDP.c
 *
 * These routines read and verify the AVDP.
 ****************************************************************************/

void VerifyAVDP(void);


/*****************************************************************************
 * verifyICB.c
 *
 * These routines read and verify file ICBs.
 ****************************************************************************/

int checkICB(struct FileEntry *fe, struct long_ad FE, int dir);


/*****************************************************************************
 * verifyLVID.c
 *
 * This routine verifies an LVID sequence.
 ****************************************************************************/

int verifyLVID(UINT32 loc, UINT32 len);


/*****************************************************************************
 * verifyRegid.c
 *
 * These routines read and verify various registered identifiers.
 ****************************************************************************/
int CheckRegid(struct udfEntityId *reg, char *ID);
void DisplayImplID(struct implEntityId * ieip);
void DisplayUdfID(struct udfEntityId * ueip);
void DisplayRegIDID( struct regid *RegIDp);
//void printOSInfo( UINT8 osClass, UINT8 osIdentifier );


/*****************************************************************************
 * verifyVD.c
 *
 * These routines verify the various Volume Descriptors.
 ****************************************************************************/

int checkIUVD(struct ImpUseDesc *mIUVD, struct ImpUseDesc *rIUVD);
int checkLVD(struct LogVolDesc *mLVD, struct LogVolDesc *rLVD);
int checkPD(struct PartDesc *mPD, struct PartDesc *rPD);
int checkPVD(struct PrimaryVolDes *mPVD, struct PrimaryVolDes *rPVD);
int checkUSD(struct UnallocSpDesHead *mUSD, struct UnallocSpDesHead *rUSD);


/*****************************************************************************
 * verifyVDS.c
 *
 * This routine checks the VDS and its contents for validity.  It will store
 * usefule file system information along the way, i.e. partition info.
 * 
 * The name in CheckSequence is for printing to the display.
 ****************************************************************************/

int ReadVDS(UINT8 *VDS, char *name, UINT32 loc, UINT32 len);

int VerifyVDS(void);

/*****************************************************************************
 * verifyVRS.c
 *
 * This routine checks for ISO 9660 and ECMA 167 recognition structures.
 ****************************************************************************/

int VerifyVRS(void);


/*****************************************************************************
 * volspace.c
 *
 * This routine checks for overlapping volume space assignments
 ****************************************************************************/

int track_volspace(UINT32 Location, UINT32 Length, char *Name);
