#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * The following routine verifies the Logical Volume Integrity Descriptor
 * sequence.
 */
int verifyLVID(UINT32 loc, UINT32 len)
{
  UINT32                             i, j;
  UINT32                            *Table;
  UINT8                             *buffer;
  struct LogicalVolumeIntegrityDesc *LVID;
  struct LVIDImplUse                 *LVIDIU;

  printf("  Verifying the Logical Volume Integrity Descriptor Sequence.\n");
  buffer = malloc(secsize);
  if (buffer) {
    LVID = (struct LogicalVolumeIntegrityDesc *)buffer;
    for (i = 0; i < (len >> sdivshift); i++) {
      /*
       * Get a sector and test it.
       */
      ReadSectors(buffer, loc + i, 1);
      CheckTag((struct tag *)buffer, loc + i, TAGID_LVID, 0, secsize - 16);
      if (!Error.Code) {
        printf("  LVID at %08x: recorded at ", loc + i);
        printTimestamp(LVID->sRecordingTime);
        switch (LVID->integrityType) {
          case 0:
                   printf(" [Open]\n");
                   break;
          case 1:
                   printf(" [Close]\n");
                   break;
          default:
                   printf(" Illegal! (%d)\n", LVID->integrityType);
                   break;
        }
        ID_UID = LVID->UniqueIdL;
        LVIDIU = (struct LVIDImplUse *)(buffer + 80 + LVID->N_P * 8);
        ID_Files = LVIDIU->numFiles;
        ID_Dirs = LVIDIU->numDirectories;
        printf("  %d directories, %d files, highest UniqueID is %d.\n",
               ID_Dirs, ID_Files, ID_UID);
        printf("  Min read ver. %x, min write ver. %x, max write ver %x.\n",
               LVIDIU->MinUDFRead, LVIDIU->MinUDFWrite, LVIDIU->MaxUDFWrite);
        printf("  Recorded by: ");
        DisplayImplID(&(LVIDIU->implementationID));
        Table = (UINT32 *)(buffer + 80);
        for (j = 0; j < LVID->N_P; j++) {
          printf("  Partition reference %d has %d of %d blocks available.\n",
                 j, Table[j], Table[j + LVID->N_P]);
        }
        printf("%sLength of Implementation use is %d.\n", LVID->L_IU == 
               46 ? "  " : "**", LVID->L_IU);
        if (LVID->nextIntegrityExtent.Length) {
          len = LVID->nextIntegrityExtent.Length;
          loc = LVID->nextIntegrityExtent.Location;
          i = -1;
          printf("  Next extent is %d bytes at %d.\n", len, loc);
          track_volspace(loc, len >> sdivshift, "Integrity Sequence Extension");
        }
      } else {
        i = len;
        printf("  End of LVID sequence.\n");
        ClearError();
      }
    }  //run through extent  
    free(buffer);
  } else {
    Error.Code = ERR_NO_VD_MEM;
  }
  return Error.Code;
}
   
