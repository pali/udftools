#ifndef __NSRPART4H__
#define __NSRPART4H__

#include "nsr_part1.h"
#include "nsr_part3.h"

/* [4/7.1] -------------------------------------------------------------------
 *                             WARNING
 *                          struct lb_addr
 *                             WARNING
 *
 * struct lb_addr is NOT recommended for use. The size of this structure
 * will be rounded up to 8 bytes on some machines (e.g. HP-PA)!
 *
 */

struct lb_addr {
    UINT32          LBN;         /* partition relative block # */
    UINT16          PartNo;
};

/* ==========================================================================
 *           Allocation Descriptors - See ECMA-167 P4-14
 *
 * struct short_ad: This is used for a single partition
 * struct long_ad: This is used for multiple partitions
 * struct ext_ad: This is used for compression
 */

/* [4/14.14.1.1] ------------------------------------------------------------
 *                           **** WARNING ****
 *
 * The ELENGTH type corresponds to the ExtentLength field. Note that the
 * two most-significant bits correspond to the type of extent. This is
 * compiler-dependent, so the order of the Type and Length fields may
 * need to be reversed for some compilers.
 *
 *                           **** WARNING ****
 */
typedef union {
    UINT32          Length32;
    struct {
        UINT32          Length:30;    /* length in bytes */
        UINT32          Type:2;       /* See Extent Type Constants */
    } bf;                             /* "bitfield" */
} ELENGTH;

/*
 * Extent Type Constants
 */

#define E_RECORDED    0 /* extent allocated and recorded */
#define E_ALLOCATED   1 /* extent allocated but unrecorded */
#define E_UNALLOCATED 2 /* extent unallocated and unrecorded */
#define E_ALLOCEXTENT 3 /* extent is next extent of ADs */

/* [4/14.14.1] short allocation descriptor (8 bytes) -----------------------*/
struct short_ad {
    ELENGTH         ExtentLength;
    UINT32          Location;         /* logical block address */
};

/* [4/14.14.2] long allocation descriptor (16 bytes) -----------------------*/
struct long_ad {
    ELENGTH         ExtentLength;
    UINT32          Location_LBN;     /* partition relative block # */
    UINT16          Location_PartNo;  /* partition number */
    UINT16          uUdfFlags;        /* See UDF1.01 2.3.10.1 */
    UINT8           aImpUse[4];
};

/* udf flag definitions */
#define EXT_ERASED ((UINT16)BITZERO) /* Only valid for ALLOCATED ADs */

/* [4/14.14.3] extended allocation descriptor (20 bytes) -------------------*/
struct ext_ad {
    ELENGTH         ExtentLength;
    UINT32          RecordedLength;   /* in bytes too... */
    UINT32          InfoLength;       /* bytes */
    UINT32          Location_LBN;     /* partition relative block # */
    UINT16          Location_PartNo;  /* partition number */
    UINT8           aImpUse[2];
};

/* [4/14.1] File Set Descriptor --------------------------------------------*/
struct FileSetDesc {
    struct tag            sTag;          /* uTagID = 256 */
    struct timestamp      sRecordingTime;
    UINT16                uInterchangeLev;
    UINT16                uMaxInterchangeLev;
    UINT32                uCharSetList;
    UINT32                uMaxCharSetList;
    UINT32                uFileSetNum;
    UINT32                uFileSetDescNum;
    struct charspec       sLogVolIDCharSet;
    dstring               aLogVolID[128];
    struct charspec       sFileSetCharSet;
    dstring               aFileSetID[32];
    dstring               aCopyrightFileID[32];
    dstring               aAbstractFileID[32];
    struct long_ad        sRootDirICB;
    struct domainEntityId DomainID;
    struct long_ad        sNextExtent;
    UINT8                aReserved[48];
};

/* [4/14.3] Partition Header Descriptor ------------------------------------*/
/* This structure is recorded in the Partition Contents Use    */
/* field [3/10.5.6] of the Partition Descriptor [3/10.5].  The */
/* Partition Descriptor shall have "+NSR02" recorded in the    */
/* Partition Contents field.                                   */

struct PartHeaderDesc {
    struct short_ad UST;            /* unallocated space table */
    struct short_ad USB;            /* unallocated space bitmap */
    struct short_ad PIT;            /* partition integrity table */
    struct short_ad FST;            /* freed space table */
    struct short_ad FSB;            /* freed space bitmap */
    UINT8 aReserved[88];
};

/* [4/14.4] File Identifier Descriptor ---------------------------------------
 * constant length part of a ECMA 167 File Identifier Desc
 *
 * define a constant here to represent the constant size of the
 * file identifier descriptor since sizeof(FileIDDesc) may be
 * erroneous on risc machines that pad to a multiple of 4 bytes.
 * To obtain length of entire FID, add L_FI, L_IU, and the following
 * constant and then round up to a multiple of four (for padding).
 */
#define FILE_ID_DESC_CONSTANT_LEN       38

/* WARNING - Do not use sizeof(FileIDDesc) !!!
 *  This is placing a struct at a non-multiple-of-four location!!!
 *  There is not much we can do about this, since UDF1.01 mandates it.
 *  There are no apparent problems on g++/hp-ux, since the structure
 *  itself does not contain any word-long pieces.
 */
struct FileIDDesc {
    struct tag          sTag;   /* uTagID = 257 */
    UINT16              VersionNum;
    UINT8               Characteristics;
    UINT8               L_FI;
    struct long_ad      ICB;
    UINT16              L_IU;
    struct implEntityId sImplementationID;
    /* Implementation Use Falls here */
    /* File ID falls here */
    /* padding falls here */
};

/* File Characteristics [4/14.4.4] */

#define FILE_ATTR           (UINT8) BITZERO   /* attribute for a file     */
#define HIDDEN_ATTR         (UINT8) BITZERO   /* ... existence bit        */
#define DIR_ATTR            (UINT8) BITONE    /* ... for a directory      */
#define DELETE_ATTR         (UINT8) BITTWO    /* ... for a deleted file   */
#define PARENT_ATTR         (UINT8) BITTHREE  /* ... for a parent         */

/* [4/14.5] Allocation Extent Descriptor -----------------------------------*/
struct AllocationExtentDesc {
    struct tag sTag;         /* uTagID = 258 */
    UINT32 prevAllocExtLoc;
    UINT32 L_AD;
    /* allocation descriptors go here: ADMacros at end of file */
};

/* [4/14.6] Information Control Block Details ------------------------------*/
struct ICBTag {
    UINT32 PriorDirects;
    UINT16 StrategyType;
    UINT8  StrategyParm[2];
    UINT16 NumberEntries;
    UINT8  Reserved;
    UINT8  FileType;
    /* lb_addr   sParentICB; */
    UINT32 sParentICB_LBN;       /* partition relative block # */
    UINT16 sParentICB_PartNo;    /* partition number */
    UINT16 Flags;
};

/* ICB file types */
#define FILE_TYPE_UNSPECIFIED          0
#define FILE_TYPE_UNALLOC_SP_ENTRY     1
#define FILE_TYPE_PARTITION_INTEGRITY  2
#define FILE_TYPE_INDIRECT_ENTRY       3
#define FILE_TYPE_DIRECTORY            4
#define FILE_TYPE_RAW                  5
#define FILE_TYPE_BDEV                 6
#define FILE_TYPE_CDEV                 7
#define FILE_TYPE_EXT_ATTR             8
#define FILE_TYPE_FIFO                 9
#define FILE_TYPE_SOCKET              10
#define FILE_TYPE_TERMINAL_ENTRY      11
#define FILE_TYPE_SYMLINK             12

/* ICB Flags Constants */

/* Bits 0,1,2 - ADType */
#define ADTYPEMASK     (UINT16) 0x0007    /* allocation descriptor type */
                                          /* mask for ICBTag flags field */
#define ADSHORT        (UINT16) 0x00      /* Short ADs */
#define ADLONG         (UINT16) 0x01      /* Long ADs */
#define ADEXTENDED     (UINT16) 0x02      /* Extended ADs */
#define ADNONE         (UINT16) 0x03      /* Data replaces ADs */

/* Bits 3-9 : Miscellaneous */
#define SORTED_DIRECTORY   0x0008 /* This should be cleared. */
#define NON_RELOCATABLE    0x0010 /* We don't set this. */
#define ARCHIVE            0x0020 /* Should always be set when written or modified. */
#define ICBF_S_ISUID       0x0040
#define ICBF_S_ISGID       0x0080
#define ICBF_C_ISVTX       0x0100
#define CONTIGUOUS         0x0200
#define FLAG_SYSTEM        0x0400
#define FLAG_TANSFORMED    0x0800
#define FLAG_MULTIVERS     0x1000


/* [4/14.7] Indirect Entry -------------------------------------------------*/
struct IndirectEntry {
    struct tag sTag;       /* uTagID = 259 */
    struct ICBTag sICBTag; /* FileType = 3 */
    struct long_ad sIndirectICB;
};

/* [4/14.8] Terminal Entry -------------------------------------------------*/
struct TerminalEntry {
    struct tag sTag;        /* uTagID = 260 */
    struct ICBTag sICBTag;  /* FileType = 11 */
};

/* [4/14.9] File Entry -----------------------------------------------------*/
struct FileEntry {
    struct tag sTag;        /* uTagID = 261 */
    struct ICBTag sICBTag;  /* FileType = [4-10] */
    UINT32 UID;
    UINT32 GID;
    UINT32 Permissions;
    UINT16 LinkCount;
    UINT8 RecFormat;
    UINT8 RecDisplayAttr;
    UINT32 RecLength;
    UINT32 InfoLengthL;
    UINT32 InfoLengthH;
    UINT32 LogBlocksL;
    UINT32 LogBlocksH;
    struct timestamp sAccessTime;   /* each timestamp is 12 bytes long */
    struct timestamp sModifyTime;
    struct timestamp sAttrTime;
    UINT32 Checkpoint;
    struct long_ad sExtAttrICB;     /* 16 bytes */
    struct implEntityId sImpID;
    UINT32 UniqueIdL;           /* for 13346 */
    UINT32 UniqueIdH;
    UINT32 L_EA;
    UINT32 L_AD;
    /* allocation descriptors go here: ADMacros at end of file */
};

/* [4/14.11] Unallocated Space Entry ---------------------------------------*/
struct UnallocSpEntry {
    struct tag sTag;   /* uTagID = 263 */
    struct ICBTag sICBTag;  /* FileType = 1 */
    UINT32 L_AD;
    /* allocation descriptors go here: ADMacros at end of file */
};

/* [4/14.12] Space Bitmap Entry --------------------------------------------*/
struct SpaceBitmapHdr {
    struct tag sTag;   /* uTagID = 264 */
    UINT32 N_Bits;
    UINT32 N_Bytes;
};
struct SpaceBitmapEntry {
    struct SpaceBitmapHdr hdr;
    UINT8  bitmap[0]; /* bitmap */
};

/* [4/14.13] Partition Integrity Entry -------------------------------------*/
struct PartIntegrityEntry {
    struct tag sTag;         /* uTagID = 265 */
    struct ICBTag sICBTag;   /* FileType = 2 */
    struct timestamp sRecordingTime;
    UINT8  IntegrityType;
    UINT8  reserved[175];
    struct regid sImpID;
    UINT8 aImpUse[256];
};

/* INTEGRITY CONSTANTS
   The first two are valid for LVIDs as well as PIEs.
   The final one, INTEGRITY_STABLE, is ONLY valid for PIE.
   */

#define INTEGRITY_OPEN     0
#define INTEGRITY_CLOSE    1
#define INTEGRITY_STABLE   2

/* Extended Attributes =====================================================*/

/* [4/14.10.1] Extended Attribute Header Descriptor ------------------------*/
struct ExtAttrHeaderDesc {
    struct tag sTag;   /* uTagID = 262 */
    UINT32 ImpAttrLoc;
    UINT32 AppAttrLoc;
};

struct ImplUseEA {
    UINT32 AttrType;
    UINT8  AttrSubType;
    UINT8  Reserved[3];
    UINT32 AttrLen;
    UINT32 AppUseLen;
    struct udfEntityId EA_ID;
};
#endif
