#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * The following routine loads a VDS into memory.  Only the prevailing
 * descriptors are retained, with oddities noted on the way.  Since UDF
 * allows only one of each descriptor, only one is kept.  All discarded
 * descriptors are noted.
 */
int ReadVDS(UINT8 *buf, char *name, UINT32 loc, UINT32 len)
{
  /*
   * All volume descriptors are one block in length.  This routine 
   * scans the Volume Descriptor Sequence, picking out the interesting
   * stuff.  The interesting stuff includes one PVD, one LVD, the UDF
   * IUVD, one USD, and one PD.  The buffer must be able to handle five
   * sectors.  Upon return, the buffer will contain the listed descriptors
   * in the order above.
   */
  UINT32                    i, j;
  UINT8*                    buffer;
  struct tag               *vdtag;
  UINT8                     zerotest;
  struct PrimaryVolDes     *PVD, *PVDt;
  struct LogVolDesc        *LVD, *LVDt;
  struct ImpUseDesc        *IUVD, *IUVDt;
  struct UnallocSpDesHead  *USD, *USDt;
  struct PartDesc          *PD, *PDt;

  printf("\n--Reading the %s Volume Descriptor Sequence.\n", name);
  ClearError();
  buffer = malloc(secsize);
  if (buffer) {
    memset(buf, 0, secsize * 5);

    /*
     * The following pointers are for convenience sake.
     */
    PVD  = (struct PrimaryVolDes *)(buf);
    LVD  = (struct LogVolDesc *)(buf + secsize);
    IUVD = (struct ImpUseDesc *)(buf + 2 * secsize);
    USD  = (struct UnallocSpDesHead *)(buf + 3 * secsize);
    PD   = (struct PartDesc *)(buf + 4 * secsize);

    vdtag = (struct tag *)buffer;
    PVDt  = (struct PrimaryVolDes *)buffer;
    LVDt  = (struct LogVolDesc *)buffer;
    IUVDt = (struct ImpUseDesc *)buffer;
    USDt  = (struct UnallocSpDesHead *)buffer;
    PDt   = (struct PartDesc *)buffer;
    
    for (i = 0; i < (len >> sdivshift); i++) {
      /*
       * Get a sector and test it.
       */
      ClearError();
      ReadSectors(buffer, loc + i, 1);
      CheckTag(vdtag, loc + i, -1, 0, 496);
      zerotest = 0;
      for (j = 0; j < secsize; j++) {
        zerotest |= buffer[j];
      }
      if (!zerotest) {
        ClearError();
      }
      if (!Error.Code) {
        printf("  %s VDS (%08x): ", name, loc + i);
        switch (vdtag->uTagID) {
          case 0:
                    printf("terminated by blank or zeroed sector.\n");
                    i = len;
                    break;

          case TAGID_PVD:
                    if (PVD->sTag.uTagID) {
                      /*
                       * A PVD already exists.  Replace if OK
                       */
                      if (memcmp(PVD->aVolID, PVDt->aVolID, 32) ||
                          memcmp(PVD->aVolSetID, PVDt->aVolSetID, 128) ||
                          memcmp(&PVD->sDesCharSet, &PVDt->sDesCharSet, sizeof(struct charspec))) {
                        printf("\n**A PVD that doesn't match the previous one was found.\n");
                      } else {
                        if (PVDt->uVolDescSeqNum > PVD->uVolDescSeqNum) {
                          printf("Replaced PVD seq. %d with %d.\n", PVD->uVolDescSeqNum,
                                 PVDt->uVolDescSeqNum);
                          memcpy(PVD, PVDt, secsize);
                        } else {
                          printf("Not replacing PVD seq. %d with %d.\n", PVD->uVolDescSeqNum,
                                 PVDt->uVolDescSeqNum);
                        }
                      }
                    } else {
                      memcpy(PVD, PVDt, secsize);
                      printf("Found first PVD.\n");
                    }
                    break;

          case TAGID_POINTER:
                    loc = ((struct VolDescPtr *)buffer)->sNextVDS.Location;
                    len = ((struct VolDescPtr *)buffer)->sNextVDS.Length;
                    track_volspace(loc, len >> sdivshift, "Next VDS sequence");
                    i = -1;
                    printf("Redirecting to 0x%08x (%x bytes).\n", loc, len);
                    break;

          case TAGID_IUD:
                    if (CheckRegid(&IUVDt->sImplementationIdentifier, E_REGID_IUVD)) {
                      printf("A non-UDF IUVD was found.  Skipping.\n");
                    } else {
                      if (IUVD->sTag.uTagID) {
                        /*
                         * A IUVD already exists.  Replace if OK
                         */
                        if (IUVDt->uVolDescSeqNum > IUVD->uVolDescSeqNum) {
                          printf("Replaced IUVD seq. %d with %d.\n", IUVD->uVolDescSeqNum,
                                 IUVDt->uVolDescSeqNum);
                          memcpy(IUVD, IUVDt, secsize);
                        } else {
                          printf("Not replacing IUVD seq. %d with %d.\n", IUVD->uVolDescSeqNum,
                                 IUVDt->uVolDescSeqNum);
                        }
                      } else {
                        printf("Found first IUVD.\n");
                        memcpy(IUVD, IUVDt, secsize);
                      }
                    }
                    break;
                    
          case TAGID_PD:
                    if (PD->sTag.uTagID) {
                      /*
                       * A PD already exists.  Replace if OK
                       */
                      if (PD->uPartNumber != PDt->uPartNumber) {
                        printf("\n**A PD that doesn't match the previous one was found.\n"
                               "Only one partition allowed per volume.\n");
                      } else {
                        if (PDt->uVolDescSeqNum > PD->uVolDescSeqNum) {
                          printf("Replaced PD seq. %d with %d.\n", PD->uVolDescSeqNum,
                                 PDt->uVolDescSeqNum);
                          memcpy(PD, PDt, secsize);
                        } else {
                          printf("Not replacing PD seq. %d with %d.\n", PD->uVolDescSeqNum,
                                 PDt->uVolDescSeqNum);
                        }
                      }
                    } else {
                      memcpy(PD, PDt, secsize);
                      printf("Found first PD.\n");
                    }
                    break;

          case TAGID_LVD:
                    if (LVD->sTag.uTagID) {
                      /*
                       * A LVD already exists.  Replace if OK
                       */
                      if (memcmp(LVD->uLogVolID, LVDt->uLogVolID, 128) ||
                          memcmp(&LVD->sDesCharSet, &LVDt->sDesCharSet, sizeof(struct charspec))) {
                        printf("\n**A LVD that doesn't match the previous one was found.\n");
                      } else {
                        if (LVDt->uVolDescSeqNum > LVD->uVolDescSeqNum) {
                          printf("Replaced LVD seq. %d with %d.\n", LVD->uVolDescSeqNum,
                                 LVDt->uVolDescSeqNum);
                          memcpy(LVD, LVDt, secsize);
                        } else {
                          printf("Not replacing LVD seq. %d with %d.\n", LVD->uVolDescSeqNum,
                                 LVDt->uVolDescSeqNum);
                        }
                      }
                    } else {
                      memcpy(LVD, LVDt, secsize);
                      printf("Found first LVD.\n");
                    }
                    break;

          case TAGID_USD:
                    if (USD->sTag.uTagID) {
                      /*
                       * A USD already exists.  Replace if OK
                       */
                      if (PVDt->uVolDescSeqNum > PVD->uVolDescSeqNum) {
                        printf("Replaced PVD seq. %d with %d.\n", PVD->uVolDescSeqNum,
                               PVDt->uVolDescSeqNum);
                        memcpy(PVD, PVDt, secsize);
                      } else {
                        printf("Not replacing PVD seq. %d with %d.\n", PVD->uVolDescSeqNum,
                               PVDt->uVolDescSeqNum);
                      }
                    } else {
                      memcpy(USD, USDt, secsize);
                      printf("Found first USD.\n");
                    }
                    break;

          case TAGID_TERM_DESC:
                    printf("terminated by a Terminating Descriptor.\n");
                    i = len;
                    break;

          default:
                    printf("\n**Unknown Tag (%d) found in VDS!\n", 
                           vdtag->uTagID);
        }
      } else {
        DumpError();
      }
    }  //run through extent  
  } else {
    Error.Code = ERR_NO_VD_MEM;
  }
  return Error.Code;
}
   
int VerifyVDS()
{
  int i;
  struct PrimaryVolDes    *mainPVD,  *reservePVD;
  struct ImpUseDesc       *mainIUVD, *reserveIUVD;
  struct UnallocSpDesHead *mainUSD,  *reserveUSD;
  struct LogVolDesc       *mainLVD,  *reserveLVD;
  struct PartDesc         *mainPD,   *reservePD;
  UINT8 *buffer_main, *buffer_reserve;
  
  printf("\n--Verifying Volume Descriptor Sequences.\n");

  buffer_main = malloc(secsize * 5);
  buffer_reserve = malloc(secsize * 5);

  if (buffer_main && buffer_reserve) {
    ReadVDS(buffer_main, "Main", VDS_Loc, VDS_Len);
    DumpError();
    ReadVDS(buffer_reserve, "Reserve", RVDS_Loc, RVDS_Len);
    DumpError();

    /* 
     * The important Volume Descriptors are in memory.  Let's run!
     * The following definitions are for easy access to each VD.
     */

    mainPVD  = (struct PrimaryVolDes *)(buffer_main);
    mainLVD  = (struct LogVolDesc *)(buffer_main + secsize);
    mainIUVD = (struct ImpUseDesc *)(buffer_main + 2 * secsize);
    mainUSD  = (struct UnallocSpDesHead *)(buffer_main + 3 * secsize);
    mainPD   = (struct PartDesc *)(buffer_main + 4 * secsize);

    reservePVD  = (struct PrimaryVolDes *)(buffer_reserve);
    reserveLVD  = (struct LogVolDesc *)(buffer_reserve + secsize);
    reserveIUVD = (struct ImpUseDesc *)(buffer_reserve + 2 * secsize);
    reserveUSD  = (struct UnallocSpDesHead *)(buffer_reserve + 3 * secsize);
    reservePD   = (struct PartDesc *)(buffer_reserve + 4 * secsize);

    if (!Fatal) {
      printf("\n--Primary Volume Descriptor Information:\n");
      checkPVD(mainPVD, reservePVD);
    }
  

    /* Verify the existence of the USD.  Print its contents. */
    if (!Fatal) {
      printf("\n--Unallocated Volume Space Descriptor Information:\n");
      checkUSD(mainUSD, reserveUSD);
    }
  

  /* Verify the existence of the LVD.  Print its contents and extract useful
     information. */

    if (!Fatal) {
      printf("\n--Logical Volume Descriptor Information:\n");
      checkLVD(mainLVD, reserveLVD);
    }


    /* Verify the existence of the PD(s).  Print contents and extract useful
       information. */
    if (!Fatal) {
      printf("\n--Partition Descriptor Information:\n");
      checkPD(mainPD, reservePD);
    }

    GetVAT();

    if (Error.Code) {
      DumpError();
    }

    GetMap();

    if (Error.Code) {
      DumpError();
    }

    ReadSpaceMap();

    if (Error.Code) {
      DumpError();
    }

    /* Verify the existence of the IUVD.  Print its contents. */
    if (!Fatal) {
      printf("\n--Implementation Use Volume Descriptor information:\n");
      checkIUVD(mainIUVD, reserveIUVD);
    }
  
  
    if (!Fatal) {
      printf("\n--Volume space report:\n");
      for (i = 0; i < VolSpaceListLen; i++) {
        printf("  %8d : %8d - %s\n", VolSpace[i].Location, 
               VolSpace[i].Location + VolSpace[i].Length - 1, VolSpace[i].Name);
      }
    } 
    free(buffer_main);
    free(buffer_reserve);
  }
  return Error.Code;
}

