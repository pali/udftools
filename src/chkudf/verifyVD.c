#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/*
 * This set of functions is used to verify individual volume structures.
 * In addition, appropriate information is pulled off and stored in global
 * variables.  The fields that are not checked or displayed should be, I
 * just haven't implemented it.  The code is pretty simple but tedious.
 * Each interesting (to me) field of the Main descriptor is displayed, 
 * and if a difference is found between the Main and Reserve sequences, 
 * the Reserve field is also displayed.
 *
 * If the reserve sequence doesn't exist, no errors are generated here
 * because it is flagged earlier and would make a messy display.
 */

/* 
 * The following routine checks the PVD.  It looks for legal values in most
 * fields and verifies that the main and reserve PVD are equivalent.  A few
 * fields are not checked or displayed:
 *   UINT8 aImplementationUse[64];
 *   UINT32 uPredecessorVDSLoc;
 *   UINT16 uFlags;
 *   UINT8 aReserved[22];
 */
int checkPVD(struct PrimaryVolDes *mPVD, struct PrimaryVolDes *rPVD)
{
  int error;

  error = CheckTag((struct tag *)mPVD, mPVD->sTag.uTagLoc, TAGID_PVD, 496, 496);
  DumpError();
  if (error < CHECKTAG_OK_LIMIT) {
    printf("  (M) Volume Identifier: ");
    printDstring( mPVD->aVolID, 32);
    if (RVDS_Len && memcmp(mPVD->aVolID, rPVD->aVolID, 32)) {
      printf("**(R) Volume Identifier: ");
      printDstring(rPVD->aVolID, 32);
    }
    printf("  (M) Volume Set ID:     ");
    printDstring( mPVD->aVolSetID, 128);
    if (RVDS_Len && memcmp(mPVD->aVolSetID, rPVD->aVolSetID, 128)) {
      printf("**(R) Volume Set ID:     ");
      printDstring( rPVD->aVolSetID, 128);
    }
    printf("  (M) Recording Time: ");
    printTimestamp( mPVD->sRecordingTime);
    if (RVDS_Len && memcmp(&mPVD->sRecordingTime, &rPVD->sRecordingTime, sizeof(struct timestamp))) {
      printf("  (R) Recording Time: ");
      printTimestamp( rPVD->sRecordingTime);
    }
    printf("  (M) Primary Volume Descriptor number is %u.\n", mPVD->uPrimVolDesNum);
    if (RVDS_Len && (mPVD->uPrimVolDesNum != rPVD->uPrimVolDesNum)) {
      printf("  (R) Primary Volume Descriptor number is %u.\n", rPVD->uPrimVolDesNum);
    }
    if ((mPVD->uVSN != 1) || (mPVD->uMaxVSN != 1) || 
        (RVDS_Len && ((rPVD->uVSN != 1) || (rPVD->uMaxVSN != 1)))) {
      printf("%s(M) Volume %u of %u.\n", mPVD->uVSN > mPVD->uMaxVSN ? "**" : "  ",
            mPVD->uVSN, mPVD->uMaxVSN);
      if ((mPVD->uVSN != rPVD->uVSN) || (mPVD->uMaxVSN != rPVD->uMaxVSN)) {
        printf("**(R) Volume %u of %u.\n", rPVD->uVSN, rPVD->uMaxVSN);
      }
    }
    if ((mPVD->uInterchangeLev != 2) || (RVDS_Len && (rPVD->uInterchangeLev != 2))) {
      printf("  (M) Interchange level is %u.\n", mPVD->uInterchangeLev);
      if (RVDS_Len && (mPVD->uInterchangeLev != rPVD->uInterchangeLev)) {
        printf("**(R) Interchange level is %u.\n", rPVD->uInterchangeLev);
      }
    }
    if ((mPVD->uMaxInterchangeLev != 3) || (RVDS_Len && (rPVD->uMaxInterchangeLev != 3))) {
      printf("**(M) Max. Interchange level is %u.\n", mPVD->uMaxInterchangeLev);
      if (RVDS_Len && (mPVD->uMaxInterchangeLev != rPVD->uMaxInterchangeLev)) {
        printf("**(R) Max. Interchange level is %u.\n", rPVD->uMaxInterchangeLev);
      }
    }
    if ((mPVD->uCharSetList != 1) || (RVDS_Len && (rPVD->uCharSetList != 1))) {
      printf("**(M) Character set list is 0x%08x.\n", mPVD->uCharSetList);
      if (RVDS_Len && (mPVD->uCharSetList != rPVD->uCharSetList)) {
        printf("**(R) Character set list is 0x%08x.\n", rPVD->uCharSetList);
      }
    }
    if ((mPVD->uMaxCharSetList != 1) || (RVDS_Len && (rPVD->uMaxCharSetList != 1))) {
      printf("**(M) Max. Character set list is 0x%08x.\n", mPVD->uMaxCharSetList);
      if (RVDS_Len && (mPVD->uMaxCharSetList != rPVD->uMaxCharSetList)) {
        printf("**(R) Max. Character set list is 0x%08x.\n", rPVD->uMaxCharSetList);
      }
    }
    if (!Is_Charspec(mPVD->sDesCharSet)) {
      printf("**(M) Description Character Set is: ");
      printCharSpec(mPVD->sDesCharSet);
    }
    if (RVDS_Len && !Is_Charspec(rPVD->sDesCharSet)) {
      printf("**(R) Description Character Set is: ");
      printCharSpec(rPVD->sDesCharSet);
    }
    if (!Is_Charspec(mPVD->sExplanatoryCharSet)) {
      printf("**(M) Description Character Set is: ");
      printCharSpec(mPVD->sExplanatoryCharSet);
    }
    if (RVDS_Len && !Is_Charspec(rPVD->sExplanatoryCharSet)) {
      printf("**(R) Description Character Set is: ");
      printCharSpec(rPVD->sExplanatoryCharSet);
    }
    if (RVDS_Len && memcmp(&mPVD->sVolAbstract, &rPVD->sVolAbstract, sizeof(struct extent_ad))) {
      printf("**(M) Volume Abstract location: ");
      printExtentAD(mPVD->sVolAbstract);
      printf("**(R) Volume Abstract location: ");
      printExtentAD(rPVD->sVolAbstract);
    }
    if (RVDS_Len && memcmp(&mPVD->sVolCopyrightNotice, &rPVD->sVolCopyrightNotice, sizeof(struct extent_ad))) {
      printf("**(M) Volume Abstract location: ");
      printExtentAD(mPVD->sVolCopyrightNotice);
      printf("**(R) Volume Abstract location: ");
      printExtentAD(rPVD->sVolCopyrightNotice);
    }
    printf("  (M) App. ID: ");
    DisplayImplID((struct implEntityId *)&mPVD->sApplicationID);
    if (RVDS_Len && memcmp(&mPVD->sApplicationID, &rPVD->sApplicationID, sizeof(struct udfEntityId))) {
      printf("**(R) App. ID: ");
      DisplayImplID((struct implEntityId *)&rPVD->sApplicationID);
    }
    printf("  (M) Impl. ID: ");
    DisplayImplID(&mPVD->sImplementationID);
    if (RVDS_Len && memcmp(&mPVD->sImplementationID, &rPVD->sImplementationID, sizeof(struct udfEntityId))) {
      printf("**(R) Impl. ID: ");
      DisplayImplID(&rPVD->sImplementationID);
    }
  }
  return Error.Code;
}


/* 
 * The following routine checks the IUVD.  It looks for legal values in most
 * fields and verifies that the main and reserve IUVD are equivalent.  A few
 * fields are not checked or displayed:
 *   UINT8 aImplementationUse[128];
 */
int checkIUVD(struct ImpUseDesc *mIUVD, struct ImpUseDesc *rIUVD)
{
  int error;
  struct LVInformation *mLVI, *rLVI;

  error = CheckTag((struct tag *)mIUVD, mIUVD->sTag.uTagLoc, TAGID_IUD, 0, secsize);
  DumpError();
  if (error < CHECKTAG_OK_LIMIT) {
    printf("%s(M) Impl. ID: ",
           CheckRegid(&mIUVD->sImplementationIdentifier, E_REGID_IUVD) ? "**" : "  ");
    DisplayUdfID(&mIUVD->sImplementationIdentifier);
    if (RVDS_Len && CheckRegid(&rIUVD->sImplementationIdentifier, E_REGID_IUVD)) {
      printf("**(R) Impl. ID: ");
      DisplayUdfID(&rIUVD->sImplementationIdentifier);
    }                                                                            

    mLVI = (struct LVInformation *)&(mIUVD->aReserved[0]);
    rLVI = (struct LVInformation *)&(rIUVD->aReserved[0]);

    if (!Is_Charspec(mLVI->sLVICharset)) {
      printf("**(M) Description Character Set is: ");
      printCharSpec(mLVI->sLVICharset);
    }
    if (RVDS_Len && !Is_Charspec(rLVI->sLVICharset)) {
      printf("**(R) Description Character Set is: ");
      printCharSpec(rLVI->sLVICharset);
    }                                                                            
 
    if (memcmp(LogVolID, mLVI->aLogicalVolumeIdentifier, 128)) {
      printf("**(M) Logical Volume Identifier doesn't match LVD\n");
    }
    printf("  (M) Logical Volume Identifier: ");
    printDstring(mLVI->aLogicalVolumeIdentifier, 128);
    if (RVDS_Len && memcmp(mLVI->aLogicalVolumeIdentifier, rLVI->aLogicalVolumeIdentifier, 128)) {
      printf("**(R) Logical Volume Identifier: ");
      printDstring(rLVI->aLogicalVolumeIdentifier, 128);
    }

    printf("  (M) Logical Volume Info 1: ");
    printDstring(mLVI->aLVInfo1, 36);
    if (RVDS_Len && memcmp(mLVI->aLVInfo1, rLVI->aLVInfo1, 36)) {
      printf("**(R) Logical Volume Info 1: ");
      printDstring(rLVI->aLVInfo1, 36);
    }

    printf("  (M) Logical Volume Info 2: ");
    printDstring(mLVI->aLVInfo2, 36);
    if (RVDS_Len && memcmp(mLVI->aLVInfo2, rLVI->aLVInfo2, 36)) {
      printf("**(R) Logical Volume Info 2: ");
      printDstring(rLVI->aLVInfo2, 36);
    }

    printf("  (M) Logical Volume Info 3: ");
    printDstring(mLVI->aLVInfo3, 36);
    if (RVDS_Len && memcmp(mLVI->aLVInfo3, rLVI->aLVInfo3, 36)) {
      printf("**(R) Logical Volume Info 3: ");
      printDstring(rLVI->aLVInfo3, 36);
    }

    printf("  (M) Impl. ID: ");
    DisplayImplID(&mLVI->sImplementationID);
    if (RVDS_Len && memcmp(&mLVI->sImplementationID, &rLVI->sImplementationID, sizeof(struct udfEntityId))) {
      printf("**(R) Impl. ID: ");
      DisplayImplID(&rLVI->sImplementationID);
    }
  }
  return 0;
}


/* 
 * The following routine checks the PD.  It looks for legal values in most
 * fields and verifies that the main and reserve PD are equivalent.  A few
 * fields are not checked or displayed:
 *   UINT8  aPartContentsUse[128];
 *   UINT8  aImplementationUse[128];
 *   UINT8  aReserved[156];
 */
int checkPD(struct PartDesc *mPD, struct PartDesc *rPD)
{
  int hit, i, error;
  struct PartHeaderDesc *PHD;

  error = CheckTag((struct tag *)mPD, mPD->sTag.uTagLoc, TAGID_PD, 496, 496);
  DumpError();
  if (error < CHECKTAG_OK_LIMIT) {
    printf("  (M) Partition number %d.\n", mPD->uPartNumber);
    if (RVDS_Len && (mPD->uPartNumber != rPD->uPartNumber)) {
      printf("**(R) Partition number %d.\n", rPD->uPartNumber);
    }

    printf("  (M) Partition flags: %04x (Space %sAllocated)\n", mPD->uPartFlags,
            mPD->uPartFlags & PARTITION_ALLOCATED ? "" : "NOT ");
    if (RVDS_Len && (mPD->uPartFlags != rPD->uPartFlags)) {
      printf("  (R) Partition flags: %04x (Space %sAllocated)\n", rPD->uPartFlags,
              rPD->uPartFlags & PARTITION_ALLOCATED ? "" : "NOT ");
    }

    if (memcmp((UINT8 *)&mPD->sPartContents +1, E_REGID_NSR, 5)) {
      printf("**(M) Illegal partition contents identifier\n      ");
      DisplayRegIDID(&mPD->sPartContents);
      printf("\n");
    }
    if (*((UINT8 *)(&mPD->sPartContents) + 6) - '0' != UDF_Version) {
      printf("**(M) NSR version is %d, partition claims %d.  Changing.\n", UDF_Version,
             *((UINT8 *)(&mPD->sPartContents) + 6) - '0');
      UDF_Version = *((UINT8 *)(&mPD->sPartContents) + 6) - '0';
      Version_OK = TRUE;
    }
    if (RVDS_Len && memcmp((UINT8 *)&mPD->sPartContents, (UINT8 *)&rPD->sPartContents, sizeof(struct regid))) {
      printf("**(R) Reserve sequence partition contents identifier\n      ");
      DisplayRegIDID(&rPD->sPartContents);
      printf("\n");
    }

    printf("  (M) Impl. ID: ");
    DisplayImplID(&mPD->sImplementationID);
    if (RVDS_Len && memcmp(&mPD->sImplementationID, &rPD->sImplementationID, sizeof(struct udfEntityId))) {
      printf("**(R) Impl. ID: ");
      DisplayImplID(&rPD->sImplementationID);
    }


    switch (mPD->uAccessType) {
      case ACCESS_UNSPECIFIED:  printf("  (M) Access Type Unspecified.\n");  break;
      case ACCESS_READ_ONLY:    printf("  (M) Access Type Read Only.\n");    break;
      case ACCESS_WORM:         printf("  (M) Access Type Write Once.\n");   break;
      case ACCESS_REWRITABLE:   printf("  (M) Access Type Rewritable.\n");   break;
      case ACCESS_OVERWRITABLE: printf("  (M) Access Type Overwritable.\n"); break;
      default:                  printf("**(M) Access Type Non-Standard.\n"); break;
    }
    if (RVDS_Len && (mPD->uAccessType != rPD->uAccessType)) {
      switch (rPD->uAccessType) {
        case ACCESS_UNSPECIFIED:  printf("**(R) Access Type Unspecified.\n");  break;
        case ACCESS_READ_ONLY:    printf("**(R) Access Type Read Only.\n");    break;
        case ACCESS_WORM:         printf("**(R) Access Type Write Once.\n");   break;
        case ACCESS_REWRITABLE:   printf("**(R) Access Type Rewritable.\n");   break;
        case ACCESS_OVERWRITABLE: printf("**(R) Access Type Overwritable.\n"); break;
        default:                  printf("**(R) Access Type Non-Standard.\n"); break;
      }
    }

    printf("  (M) Partition starts at sector %d.\n", mPD->uPartStartingLoc);
    if (RVDS_Len && (mPD->uPartStartingLoc != rPD->uPartStartingLoc)) {
      printf("**(R) Partition starts at sector %d.\n", rPD->uPartStartingLoc);
    }

    printf("  (M) Partition length is %d sectors.\n", mPD->uPartLength);
    if (RVDS_Len && (mPD->uPartLength != rPD->uPartLength)) {
      printf("**(R) Partition Length is %d sectors.\n", rPD->uPartLength);
    }

    track_volspace(mPD->uPartStartingLoc, mPD->uPartLength, "A partition");

    hit = 0;
    for (i = 0; i < PTN_no; i++) {
      if (Part_Info[i].Num == mPD->uPartNumber) {
        Part_Info[i].Offs = mPD->uPartStartingLoc;
        Part_Info[i].Len = mPD->uPartLength;
        hit++;
        PHD = (struct PartHeaderDesc *)(mPD->aPartContentsUse);
        if (PHD->USB.ExtentLength.Length32) {
          Part_Info[i].Space = PHD->USB.Location;
          Part_Info[i].SpLen = PHD->USB.ExtentLength.bf.Length;
          Part_Info[i].SpMap = malloc(PHD->USB.ExtentLength.bf.Length);
          Part_Info[i].MyMap = malloc(PHD->USB.ExtentLength.bf.Length);
          if (Part_Info[i].MyMap) {
            memset(Part_Info[i].MyMap, 0xff, PHD->USB.ExtentLength.bf.Length);
          }
        }
      }
    }
    if (hit != 1) {
      printf("  (M) partition %d was referenced by %d maps.\n", mPD->uPartNumber,
             hit);
    }
  } else {
    Fatal = TRUE;
  }
  DumpError();
  return 0;
}


/* 
 * The following routine checks the LVD.  It looks for legal values in most
 * fields and verifies that the main and reserve LVD are equivalent.  A few
 * fields are not checked or displayed:
 *   UINT8 aImplementationUse[128];
 */
int checkLVD(struct LogVolDesc *mLVD, struct LogVolDesc *rLVD)
{
  int i, offset, error;
  struct PartMap1   *sPartMap1;
  struct PartMapVAT *sPartMapVAT;
  struct PartMapSP  *sPartMapSP;

  error = CheckTag((struct tag *)mLVD, mLVD->sTag.uTagLoc, TAGID_LVD, 424, secsize);
  DumpError();
  if (error < CHECKTAG_OK_LIMIT) {
    if (!Is_Charspec(mLVD->sDesCharSet)) {
      printf("**(M) Description Character Set is: ");
      printCharSpec(mLVD->sDesCharSet);
    }
    if (RVDS_Len && !Is_Charspec(rLVD->sDesCharSet)) {
      printf("**(R) Description Character Set is: ");
      printCharSpec(rLVD->sDesCharSet);
    }

    memcpy(LogVolID, mLVD->uLogVolID, 128);

    printf("  (M) Logical Volume ID:     ");
    printDstring( mLVD->uLogVolID, 128);
    if (RVDS_Len && memcmp(mLVD->uLogVolID, rLVD->uLogVolID, 128)) {
      printf("**(R) Logical Volume ID:     ");
      printDstring( rLVD->uLogVolID, 128);
    }

    blocksize = mLVD->uLogBlkSize;
    if (blocksize < secsize) {
      printf("**(M) Block size of %d is less than sector size of %d!.\n",
             blocksize, secsize);
      Fatal = TRUE;
    } else {
      while (!((1 << bdivshift) & blocksize)) {
        bdivshift++;
      }
      s_per_b = blocksize / secsize;
    }
    if (mLVD->uLogBlkSize != secsize) {
      printf("**(M) Block size is %d, sector size is %d.\n", blocksize, secsize);
    }
    if (RVDS_Len && (rLVD->uLogBlkSize != mLVD->uLogBlkSize)) {
      printf("**(R) Block size is %d.\n", rLVD->uLogBlkSize);
    }

    if (CheckRegid((struct udfEntityId *)&mLVD->sDomainID, UDF_DOMAIN_ID)) {
      printf("**(M) Domain Identifier: ");
      DisplayUdfID((struct udfEntityId *)&mLVD->sDomainID);
    }

    if (RVDS_Len && CheckRegid((struct udfEntityId *)&rLVD->sDomainID, UDF_DOMAIN_ID)) {
      printf("**(R) Domain Identifier: ");
      DisplayUdfID((struct udfEntityId *)&rLVD->sDomainID);
    }

    printf("  (M) Impl. ID: ");
    DisplayImplID(&mLVD->sImplementationID);
    if (RVDS_Len && memcmp(&mLVD->sImplementationID, &rLVD->sImplementationID, sizeof(struct udfEntityId))) {
      printf("**(R) Impl. ID: ");
      DisplayImplID(&rLVD->sImplementationID);
    }

    memcpy(&FSD, &mLVD->uLogVolUse, 16);

    if (FSD.ExtentLength.bf.Length == 0) {
      Fatal = TRUE;
      printf("**");
    } else {
      printf("  ");
    }
    printf("(M) File Set Descriptor location is ");
    printLongAd(&FSD);

    if (RVDS_Len && memcmp(&FSD, &rLVD->uLogVolUse, 16)) {
      printf("**(R) File Set Descriptor Location is ");
      printLongAd((struct long_ad *)&rLVD->uLogVolUse);
    }

    for (i = 0; i < NUM_PARTS; i++) {
      Part_Info[i].type = PTN_TYP_NONE;
      Part_Info[i].Num = 0;
      Part_Info[i].Offs = 0;
      Part_Info[i].Len = 0;
      Part_Info[i].Space = -1;
      Part_Info[i].Extra = NULL;
      Part_Info[i].SpMap = NULL;
      Part_Info[i].MyMap = NULL;
    }

    printf("  (M) There %s %d partition map entr%s.\n", mLVD->uNumPartMaps == 1 ? "is" : "are", 
            mLVD->uNumPartMaps, mLVD->uNumPartMaps == 1 ? "y" : "ies");
    if (RVDS_Len && (mLVD->uNumPartMaps != rLVD->uNumPartMaps)) {
      printf("**(R) There %s %d partition map entr%s.\n", rLVD->uNumPartMaps == 1 ? "is" : "are",
              rLVD->uNumPartMaps, rLVD->uNumPartMaps == 1 ? "y" : "ies");
    }
    if (mLVD->uNumPartMaps > NUM_PARTS) {
      Error.Code = ERR_TOO_MANY_PARTS;
      DumpError();
      Fatal = TRUE;
    } else {
      PTN_no = mLVD->uNumPartMaps;
      if (PTN_no == 0) {
        printf("**No Partition Map Entries.\n");
        Fatal = TRUE;
      }
      offset = sizeof(struct LogVolDesc);
      for (i = 0; i < PTN_no; i++) {
        printf("  (M) Partition map entry %d is ", i);
        sPartMap1 = (struct PartMap1 *)((UINT8 *)mLVD + offset);
        sPartMapVAT = (struct PartMapVAT *)sPartMap1;
        sPartMapSP = (struct PartMapSP *)sPartMap1;

        switch(sPartMap1->uPartMapType) {
          case 1:
              Part_Info[i].type = PTN_TYP_REAL;
              Part_Info[i].Num  = sPartMap1->uPartNum;
              printf("type 1 (real) and references partition %d.\n", Part_Info[i].Num);
            break;

          case 2:
              if (!strncmp(E_REGID_CD_VP, sPartMapVAT->sVATIdentifier.aID, strlen(E_REGID_CD_VP))) {
                Part_Info[i].type = PTN_TYP_VIRTUAL;
                Part_Info[i].Num  = sPartMapVAT->uPartNum;
                printf("type 2 (virtual) and references partition %d.\n", Part_Info[i].Num);
              } else if (!strncmp(E_REGID_CD_SP, sPartMapVAT->sVATIdentifier.aID, strlen(E_REGID_CD_SP))) {
                Part_Info[i].type = PTN_TYP_SPARE;
                Part_Info[i].Num  = sPartMapSP->uPartNum;
                printf("type 2 (sparable) and references partition %d.\n", Part_Info[i].Num);
                Part_Info[i].Extra = malloc(sizeof(struct _sST_desc));
                if ((struct _sST_desc *)Part_Info[i].Extra) {
                  printf("  (M) %d sparing map(s), %d bytes long, mapping %d sectors each.\n",
                          sPartMapSP->N_ST, sPartMapSP->SpareSize, sPartMapSP->uPacketLength);
                  ((struct _sST_desc *)Part_Info[i].Extra)->Size = sPartMapSP->SpareSize;
                  ((struct _sST_desc *)Part_Info[i].Extra)->Count = sPartMapSP->N_ST;
                  ((struct _sST_desc *)Part_Info[i].Extra)->Extent = sPartMapSP->uPacketLength;
                  ((struct _sST_desc *)Part_Info[i].Extra)->Location[0] = sPartMapSP->SpareLoc[0];
                  ((struct _sST_desc *)Part_Info[i].Extra)->Location[1] = sPartMapSP->SpareLoc[1];
                  ((struct _sST_desc *)Part_Info[i].Extra)->Location[2] = sPartMapSP->SpareLoc[2];
                  ((struct _sST_desc *)Part_Info[i].Extra)->Location[3] = sPartMapSP->SpareLoc[3];
                }
              } else {
                Part_Info[i].type = PTN_TYP_NONE;
                printf("type 2 (unknown).\n");
              }
            break;
          default:
              Part_Info[i].type = PTN_TYP_NONE;
              printf("illegal type (%d).\n", sPartMap1->uPartMapType);
        }
        offset += sPartMap1->uPartMapLen;
      }
    }
    offset -= sizeof(struct LogVolDesc);
    if (offset != mLVD->uMapTabLen) {
      printf("**(M) Found %d bytes of map entries, LVD claims %d.\n", offset,
             mLVD->uMapTabLen);
    }
    if (RVDS_Len && (offset != rLVD->uMapTabLen)) {
      printf("**(R) Found %d bytes of map entries, LVD claims %d.\n", offset,
             rLVD->uMapTabLen);
    }

    printf("  (M) Integrity Sequence is %d bytes at %d.\n", 
           mLVD->integritySeqExtent.Length, mLVD->integritySeqExtent.Location);
    if (RVDS_Len && memcmp(&mLVD->integritySeqExtent, &rLVD->integritySeqExtent, 
               sizeof(struct extent_ad))) {
      printf("**(R) Integrity Sequence is %d bytes at %d.\n", 
             rLVD->integritySeqExtent.Length, rLVD->integritySeqExtent.Location);
    }
    track_volspace(mLVD->integritySeqExtent.Location, mLVD->integritySeqExtent.Length >> sdivshift, 
                   "Integrity Sequence");

    verifyLVID(mLVD->integritySeqExtent.Location, mLVD->integritySeqExtent.Length);
  } else {
    Fatal = TRUE;
  }
  DumpError();
  return Error.Code;
}


/* 
 * The following routine checks the USD.  It looks for legal values in most
 * fields and verifies that the main and reserve USD are equivalent.  
 */
int checkUSD(struct UnallocSpDesHead *mUSD, struct UnallocSpDesHead *rUSD)
{
  int i, error;

  error = CheckTag((struct tag *)mUSD, mUSD->sTag.uTagLoc, TAGID_USD, 8, secsize);
  DumpError();
  if (error < CHECKTAG_OK_LIMIT) {
    printf("  (M) Number of Allocation Descriptors: %d\n", mUSD->uNumAllocationDes);
    if (RVDS_Len && (mUSD->uNumAllocationDes != rUSD->uNumAllocationDes)) {
      printf("**(R) Number of Allocation Descriptors: %d\n", rUSD->uNumAllocationDes);
    }

    if (mUSD->uNumAllocationDes > 0) {
      printf("  (M) Space allocation descriptors:\n");
    }
    for (i = 0; i < mUSD->uNumAllocationDes; i++) {
      printf("    %d bytes (%d blocks) @ %d\n",
               *((UINT32 *) ((UINT8 *)mUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad))),
               *((UINT32 *) ((UINT8 *)mUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad))) >> sdivshift,
               *((UINT32 *) ((UINT8 *)mUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad)) + 1));
      track_volspace(*((UINT32 *)((UINT8 *)mUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad)) + 1),
                     *((UINT32 *)((UINT8 *)mUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad))) >> sdivshift,
                     "Unallocated Space");
    }
    if (RVDS_Len && (memcmp((UINT8 *)mUSD + sizeof(struct UnallocSpDesHead), 
               (UINT8 *)rUSD + sizeof(struct UnallocSpDesHead), 
               mUSD->uNumAllocationDes * sizeof(struct extent_ad)) || 
               (mUSD->uNumAllocationDes != rUSD->uNumAllocationDes))) {
      printf("**(R) Space allocation descriptors:\n");
      for (i = 0; i < rUSD->uNumAllocationDes; i++) {
        printf("    %d bytes (%d blocks) @ %d\n", 
               *((UINT32 *) ((UINT8 *)rUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad))),
               *((UINT32 *) ((UINT8 *)rUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad))) >> sdivshift,
               *((UINT32 *) ((UINT8 *)rUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad)) + 1));
      track_volspace(*((UINT32 *)((UINT8 *)rUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad)) + 1),
                     *((UINT32 *)((UINT8 *)rUSD + sizeof(struct UnallocSpDesHead) + i * sizeof(struct extent_ad))) >> sdivshift,
                     "Unallocated Space from Reserve USD");
      }
    }
  }
  return 0;
}

