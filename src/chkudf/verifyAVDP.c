#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

void VerifyAVDP(void)
{
  int avdp_count = 0;
  int front_avdp[] = {256, 512, -1};
  int back_avdp[] = {0, 150, 256, 406, -1};
  int i, result;
  struct AnchorVolDesPtr *AVDPtr;

  AVDPtr = (struct AnchorVolDesPtr *)malloc(secsize);
  if (AVDPtr) {
    printf("\n--Verifying the Anchor Volume Descriptor Pointers.\n");
    
    for (i = 0; front_avdp[i] != -1; i++) {
      printf("  Checking %d: ", SS + front_avdp[i]);
      result = ReadSectors(AVDPtr, SS + front_avdp[i], 1);
      if (!result) {
        result = CheckTag((struct tag *)AVDPtr, SS + front_avdp[i], TAGID_ANCHOR, 
                      16, 496);
        if (result < CHECKTAG_OK_LIMIT) {
          printf("AVDP present.\n");
          DumpError();
          track_volspace(SS + front_avdp[i], 1, "Front AVDP");
          if (!avdp_count) {
            VDS_Loc = AVDPtr->sMainVDSAdr.Location;
            VDS_Len = AVDPtr->sMainVDSAdr.Length;
            RVDS_Loc = AVDPtr->sReserveVDSAdr.Location;
            RVDS_Len = AVDPtr->sReserveVDSAdr.Length;
            track_volspace(VDS_Loc, VDS_Len >> sdivshift, "Main VDS (Front AVDP)");
            track_volspace(RVDS_Loc, RVDS_Len >> sdivshift, "Reserve VDS (Front AVDP)");
          } else {
            if (VDS_Loc != AVDPtr->sMainVDSAdr.Location ||
                VDS_Len != AVDPtr->sMainVDSAdr.Length ||
                RVDS_Loc != AVDPtr->sReserveVDSAdr.Location ||
                RVDS_Len != AVDPtr->sReserveVDSAdr.Length) {
              Error.Code = ERR_VDS_NOT_EQUIVALENT;
              Error.Sector = SS + front_avdp[i];
              DumpError();
            }
          } /* If first AVDP */
          avdp_count++;
        } else {
          printf("No AVDP.\n");
          ClearError();
        } /* If is AVDP */
      } else {
        printf("read error.\n");
      }
    }

    /* Check the end referenced AVDPs */
    for (i = 0; back_avdp[i] != -1; i++) {
      printf("  Checking %d (n - %d): ", LastSector - back_avdp[i], back_avdp[i]);
      result = ReadSectors(AVDPtr, LastSector - back_avdp[i], 1);
      if (!result) {
        result = CheckTag((struct tag *)AVDPtr, LastSector - back_avdp[i], 
                     TAGID_ANCHOR, 16, 496);
        if (result < CHECKTAG_OK_LIMIT) {
          printf("AVDP present.\n");
          DumpError();
          track_volspace(LastSector - back_avdp[i], 1, "Back AVDP");
          if (!avdp_count) {
            VDS_Loc = AVDPtr->sMainVDSAdr.Location;
            VDS_Len = AVDPtr->sMainVDSAdr.Length;
            RVDS_Loc = AVDPtr->sReserveVDSAdr.Location;
            RVDS_Len = AVDPtr->sReserveVDSAdr.Length;
            track_volspace(VDS_Loc, VDS_Len >> sdivshift, "Main VDS (Back AVDP)");
            track_volspace(RVDS_Loc, RVDS_Len >> sdivshift, "Reserve VDS (Back AVDP)");
          } else {
            if (VDS_Loc != AVDPtr->sMainVDSAdr.Location ||
                VDS_Len != AVDPtr->sMainVDSAdr.Length ||
                RVDS_Loc != AVDPtr->sReserveVDSAdr.Location ||
                RVDS_Len != AVDPtr->sReserveVDSAdr.Length) {
              Error.Code = ERR_VDS_NOT_EQUIVALENT;
              Error.Sector = LastSector - back_avdp[i];
              DumpError();
            }
          } /* If first AVDP */
          avdp_count++;
        } else {
          printf("No AVDP.\n");
          ClearError();
        } /* If is AVDP */
      } else {
        printf("read error.\n");
      }
    }

    /* Check the N/59 referenced AVDPs */
//    for (i = 1; i < 59; i++) {
//      result = ReadSectors(AVDPtr, LastSector / 59 * i, 1);
//      if (!result) {
//        CheckTag((struct tag *)AVDPtr, LastSector / 59 * i, TAGID_ANCHOR, 16, 496);
//        if (!Error.Code) {
//          track_volspace(LastSector / 59 * i, 1);
//          if (!avdp_count) {
//            *VDS_Loc = AVDPtr->sMainVDSAdr.Location;
//            *VDS_Len = AVDPtr->sMainVDSAdr.Length;
//            *RVDS_Loc = AVDPtr->sReserveVDSAdr.Location;
//            *RVDS_Len = AVDPtr->sReserveVDSAdr.Length;
//            track_volspace(*VDS_Loc, *VDS_Len >> sdivshift);
//            track_volspace(*RVDS_Loc, *RVDS_Len >> sdivshift);
//          } else {
//            if (*VDS_Loc != AVDPtr->sMainVDSAdr.Location ||
//                *VDS_Len != AVDPtr->sMainVDSAdr.Length ||
//                *RVDS_Loc != AVDPtr->sReserveVDSAdr.Location ||
//                *RVDS_Len != AVDPtr->sReserveVDSAdr.Length) {
//              error = ERR_AVDP_NOT_EQUIVALENT;
//            }
//          } /* If first AVDP */
//          avdp_count++;
//        } else {
//          error = 0;
//        } /* If is AVDP */
//      } else {
//        printf("ReadSectors(%u, 1) failed.\n", LastSector / 59 * i);
//      }  /* If sector is read */
//    }
    if (avdp_count) {
      printf("%sFound %u Anchor Volume Descriptor Pointer%s.\n", 
             avdp_count > 1 ? "  " : "**", avdp_count,
             avdp_count == 1 ? "" : "s");
      printf("%sMain Volume Descriptor Sequence is at %u, %u bytes long.\n",
               VDS_Len < 32768 ? "**" : "  ", VDS_Loc, VDS_Len);
      printf("%sReserve Volume Descriptor Sequence is at %u, %u bytes long.\n",
               RVDS_Len < 32768 ? "**" : "  ", RVDS_Loc, RVDS_Len);
      if (!VDS_Len && !RVDS_Len) {
        printf("**Both Volume Descriptor Sequences have zero length.\n");
        Fatal = TRUE;
      }
    } else {
      printf("**No Anchor Volume Descriptor Pointers found.\n");
      Error.Code = ERR_NOAVDP;
      Error.Sector = LastSector;
      Fatal = TRUE;
    }
    free(AVDPtr);
  } else {
    printf("**Couldn't allocate memory for reading AVDP.\n");
    Fatal = TRUE;
  }
}
