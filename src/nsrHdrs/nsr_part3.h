#ifndef __NSRPART3H__
#define __NSRPART3H__

#include "nsr_part1.h"

/* [3/7.1] Extent Descriptor -----------------------------------*/
struct extent_ad {
    UINT32 Length;
    UINT32 Location;
};

/* [3/7.2] [4/7.2] Descriptor Tag --------------------------------------*/
struct tag {
    UINT16 uTagID;
    UINT16 uDescriptorVersion;
    UINT8  uTagChecksum;
    UINT8  uReserved;
    UINT16 uTagSerialNum;
    UINT16 uDescriptorCRC;
    UINT16 uCRCLen;
    UINT32 uTagLoc;
};

/* [3/10.1] Primary Volume Descriptor --------------------------*/
struct PrimaryVolDes {
    struct tag sTag;       /* uTagID = 1 */
    UINT32 uVolDescSeqNum;
    UINT32 uPrimVolDesNum;
    dstring aVolID[32];
    UINT16 uVSN;
    UINT16 uMaxVSN;
    UINT16 uInterchangeLev;
    UINT16 uMaxInterchangeLev;
    UINT32 uCharSetList;
    UINT32 uMaxCharSetList;
    dstring aVolSetID[128];
    struct charspec sDesCharSet;
    struct charspec sExplanatoryCharSet;
    struct extent_ad sVolAbstract;
    struct extent_ad sVolCopyrightNotice;
    struct regid sApplicationID;
    struct timestamp sRecordingTime;
    struct implEntityId sImplementationID;
    UINT8 aImplementationUse[64];
    UINT32 uPredecessorVDSLoc;
    UINT16 uFlags;
    UINT8 aReserved[22];
};

/* PrimaryVolDesc Flags Constants */

/* Bit 0 : Only one defined so far. */
#define COMMON_VOLSET_IDENTIFICATION ((UINT16)BITZERO)

/* [3/10.2] Anchor Volume Descriptor Pointer -------------------*/
struct AnchorVolDesPtr {
    struct tag sTag;       /* uTagID = 2 */
    struct extent_ad sMainVDSAdr;
    struct extent_ad sReserveVDSAdr;
    UINT8 Reserved[480];
};

/* [3/10.3] Volume Descriptor Pointer --------------------------*/
struct VolDescPtr {
    struct tag sTag;      /* uTagID = 3 */
    UINT32 uVolDescSeqNum;
    struct extent_ad sNextVDS;
    UINT8 aReserved[484];
};

/* [3/10.4] Implementation Use Descriptor ----------------------*/
struct ImpUseDesc {
    struct tag sTag;      /* uTagID = 4 */
    UINT32 uVolDescSeqNum;
    struct udfEntityId sImplementationIdentifier;
    UINT8 aReserved[460];
};

/* OSTA Volume Descriptor (fits inside of implUseDesc.aReserved). */
struct LVInformation {
    struct charspec sLVICharset;
    dstring aLogicalVolumeIdentifier[128];
    dstring aLVInfo1[36];
    dstring aLVInfo2[36];
    dstring aLVInfo3[36];
    struct implEntityId sImplementationID;
    UINT8 aImplementationUse[128];
};

/* [3/10.5] Partition Descriptor ----------------------------------------*/
struct PartDesc {
    struct tag sTag;        /* uTagID = 5 */
    UINT32 uVolDescSeqNum;
    UINT16 uPartFlags;
    UINT16 uPartNumber;
    struct regid sPartContents;
    UINT8  aPartContentsUse[128];
    UINT32 uAccessType;
    UINT32 uPartStartingLoc;
    UINT32 uPartLength;
    struct implEntityId sImplementationID;
    UINT8  aImplementationUse[128];
    UINT8  aReserved[156];
};

/* Partition Flags definition */
#define PARTITION_ALLOCATED ((UINT16) BITZERO)

/* Access Type */
#define ACCESS_UNSPECIFIED      0
#define ACCESS_READ_ONLY        1
#define ACCESS_WORM             2
#define ACCESS_REWRITABLE       3
#define ACCESS_OVERWRITABLE     4

/* [3/10.7.2] Type 1 Partition Map */
struct PartMap1 {
    UINT8 uPartMapType;
    UINT8 uPartMapLen;
    UINT16 uVSN;
    UINT16 uPartNum;
};

/* [3/10.7.3] Type 2 Partition Map */
struct PartMap2 {
    UINT8 uPartMapType;
    UINT8 uPartMapLen;
    UINT8 uPartID[62];
};

struct PartMapVAT {
    UINT8  uPartMapType;
    UINT8  uPartMapLen;
    UINT8  uReserved[2];
    struct udfEntityId sVATIdentifier;
    UINT16 uVSN;
    UINT16 uPartNum;
    UINT8  uReserved2[24];
};

struct PartMapSP {
    UINT8  uPartMapType;
    UINT8  uPartMapLen;
    UINT8  uReserved[2];
    struct udfEntityId sSPIdentifier;
    UINT16 uVSN;
    UINT16 uPartNum;
    UINT16 uPacketLength;
    UINT8  N_ST;
    UINT8  Reserved2;
    UINT32 SpareSize;
    UINT32 SpareLoc[4];
};

/* [3/10.6] Logical Volume Descriptor --------------------------*/
struct LogVolDesc {
    struct tag sTag;       /* uTagID = 6 */
    UINT32 uVolDescSeqNum;
    struct charspec sDesCharSet;
    dstring uLogVolID[128];
    UINT32 uLogBlkSize;
    struct domainEntityId sDomainID;
    UINT8 uLogVolUse[16];
    UINT32 uMapTabLen;
    UINT32 uNumPartMaps;
    struct implEntityId sImplementationID;
    UINT8 aImplementationUse[128];
    struct extent_ad integritySeqExtent;
/* Variable-length stuff follows: (see Macros below) */
/*    struct PartMap1 sPartMaps[uNumPartMaps];       */
/* Or:                                               */
/*    struct PartMap2 sPartMaps[uNumPartMaps];       */
};

/* Macros for LVDs */
#define LVD_HdrLen      (sizeof(struct LogVolDesc))
#define LVD_PM1Start(x) (struct PartMap1 *)((char *)(x)+LVD_HdrLen)
#define LVD_PM2Start(x) (struct PartMap2 *)((char *)(x)+LVD_HdrLen)

/* [3/10.8] Unallocated Space (volume) Entry -------------------*/
struct UnallocSpDesHead {
    struct tag sTag;          /* uTagID = 7 */
    UINT32 uVolDescSeqNum;
    UINT32 uNumAllocationDes;
};

/* [3/10.9] Terminator Descriptor */
struct Terminator {
    struct tag sTag;          /* uTagID = 8 */
    UINT8 aReserved[496];
};

/* [3/10.10] Logical Volume Integrity Descriptor ---------------*/
struct LogicalVolumeIntegrityDesc {
    struct tag          sTag;          /* uTagID = 9 */
    struct timestamp    sRecordingTime;
    UINT32              integrityType;
    struct extent_ad    nextIntegrityExtent;
    UINT32              UniqueIdL;
    UINT32              UniqueIdH;
    UINT8              reserved[24];
    UINT32              N_P;    /* num Partitions */
    UINT32              L_IU;   /* Len implement use */
/* Variable-length stuff follows: (see Macros below) */
/*    UINT32              FreeSpaceTable[N_P];       */
/*    UINT32              SizeTable[N_P];            */
/*    struct LVIDImplUse  ImplementUse;              */
};


/* Logical Volume Integrity Descriptor Implementation Use area */
struct LVIDImplUse {
    struct implEntityId implementationID;
    UINT32              numFiles;
    UINT32              numDirectories;
    UINT16              MinUDFRead;
    UINT16              MinUDFWrite;
    UINT16              MaxUDFWrite;
};

/* Macros for LVIDImplUse */
/* NOTE that LVIDIU_IUStart() needs the LVID_IUStart() as its
   argument. Thus, the supported use model is for the two structs to
   be instantiated separately, and the LVIDIU to be cast to the
   implementation use pointer supplied by the LVID. */
#define LVIDIU_HdrLen     (sizeof(struct LVIDImplUse))
#define LVIDIU_IUStart(x) (UINT8 *)((char *)(x)+LVIDIU_HdrLen)

#endif
