#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * Read the VAT from the medium. 
 * This can be made to work with blocksize != sectorsize, but since
 * it's not allowed by UDF, we won't go to the extra effort.
 */


void GetVAT(void)
{
  struct FileEntry *VATICB;
  int              i, found, result;
  UINT16           VirtPart;

  found = FALSE;
  for (i = 0; (i < PTN_no) && !found; i++) {
    if (Part_Info[i].type == PTN_TYP_VIRTUAL) {
      VirtPart = i;
      found = TRUE;
    }
  }

  if (found && (s_per_b == 1)) {
    VATICB = (struct FileEntry *)malloc(blocksize);
    if (VATICB) {
      printf("\n--Partition Reference %d is virtual, finding VAT ICB.\n", VirtPart);
      ReadSectors(VATICB, LastSector, 1);
      result = CheckTag((struct tag *)VATICB, LastSector - Part_Info[VirtPart].Offs,
                        TAGID_FILE_ENTRY, 20, blocksize);
      if (result > CHECKTAG_OK_LIMIT) {
        printf("**No VAT in the last sector.  Trying back 150 sectors.\n");
        ReadSectors(VATICB, LastSector - 150, 1);
        result = CheckTag((struct tag *)VATICB, 
                          LastSector - Part_Info[VirtPart].Offs - 150,
                          TAGID_FILE_ENTRY, 20, blocksize);
      }
      if (result < CHECKTAG_OK_LIMIT) {
        printf("  VAT ICB candidate was found.\n");
        // We have a good ICB 
        if (VATICB->sICBTag.FileType == 0) {
          Part_Info[VirtPart].Extra = malloc(VATICB->InfoLengthL + blocksize);
          if (Part_Info[VirtPart].Extra) {
            printf("  Allocated %d (0x%04x) bytes for the VAT.\n", VATICB->InfoLengthL, VATICB->InfoLengthL);
            ReadFileData(Part_Info[VirtPart].Extra, VATICB, Part_Info[VirtPart].Num,
                         0, VATICB->InfoLengthL, &i);
            Part_Info[VirtPart].Len = (VATICB->InfoLengthL - 36) >> 2;
            printf("  Virtual partition is %d sectors long.\n", Part_Info[VirtPart].Len);
            printf("%sVAT Identifier is: ", CheckRegid((struct udfEntityId *)(Part_Info[VirtPart].Extra + (VATICB->InfoLengthL >> 2) - 9), E_REGID_VAT) ? "**" : "  ");
            DisplayRegIDID((struct regid *)(Part_Info[VirtPart].Extra + (VATICB->InfoLengthL >> 2) - 9));
            printf("\n");
//            for (i = 0; (i < 50) && (i < Part_Info[VirtPart].Len); i++) {
//              printf("%02x: %08x\n", i, Part_Info[VirtPart].Extra[i]);
//            }
          } else {
            Error.Code = ERR_NOVATMEM;
            Error.Sector = LastSector - Part_Info[VirtPart].Offs;
            Fatal = TRUE;
          }
        } else {
          Error.Code = ERR_NOVAT;
          Error.Sector = LastSector - Part_Info[VirtPart].Offs;
          Fatal = TRUE;
        }
      } else {
        Error.Code = ERR_NOVAT;
        Error.Sector = LastSector - Part_Info[VirtPart].Offs;
        Fatal = TRUE;
      }
      free(VATICB);
    } else {
      printf("**Can't malloc memory for VAT ICB.\n");
      Fatal = TRUE;
    }
  } else {
    if (found) {
      Error.Code = ERR_NOVAT;
      Error.Sector = LastSector - Part_Info[VirtPart].Offs;
      Fatal = TRUE;
    }
  }
}
