#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/*
 * The VRS is not at a fixed sector number.  The following routine reads
 * descriptor (i), where i is the VRS offset from 32K.  
 */
int ReadVRD (UINT8 *VRD, int i)
{
  UINT32 sector, count;

  count = 2048 >> sdivshift;
  if (count == 0) count = 1;
  sector = (32768 >> sdivshift) + i * count + SS;
  printf("  VRS %d (sector %d): ", i, sector);
  return ReadSectors(VRD, sector, count);
}

/*
 * The following routine looks for the Volume Recognition sequence.  
 * There is nothing fatal here.
 */
int VerifyVRS(void)
{
  int error = 0;
  UINT32 i;
  int Term = 0, VRS_OK = TRUE, NSR_Found = 0, BEA_Found = 0;
  UINT8 *VRS;

  VRS = (UINT8 *)malloc(MAX(secsize, 2048));
  if (VRS) {
    printf("\n--Verifying the Volume Recognition Sequence.\n");
    /* Process ISO9660 VRS */
    i = 0;
    while (VRS_OK) {
      error = ReadVRD(VRS, i);
      if (!error) {
        VRS_OK = !strncmp(VRS+1, VRS_ISO9660, 5);
        if (VRS_OK) {
          Term = VRS[0] == 0xff;
          switch (VRS[0]) {
            case 0: printf("ISO 9660 Boot Record\n"); break;
            case 1: printf("ISO 9660 Primary Volume Descriptor\n"); break;
            case 2: printf("ISO 9660 Supplementary Volume Descriptor\n"); break;
            case 3: printf("ISO 9660 Volume Partition Descriptor\n"); break;
            case 255: printf("ISO 9660 Volume Descriptor Set Terminator\n"); break;
            default: printf("9660 VRS (code %u)\n", (int)*VRS);
          }
          i++;
        }
      } else {
        VRS_OK = FALSE;
      }
    }
    if (i) {
      printf(" %d ISO 9660 descriptors found.\n", i);
      if (!Term) {
        printf("**However, it was not terminated!\n");
      }
    } else {
      printf(" No ISO 9660 descriptors found.\n");
    }

    /* Process ISO 13346 */
    Term = 0;  //No terminating descriptor yet
    if (!error) {
      printf("  VRS %d            : ", i);
      VRS_OK = !strncmp(VRS+1, VRS_ISO13346_BEGIN, 5);
      if (VRS_OK) {
        BEA_Found = 1;
        printf("Beginning Extended Area descriptor found.\n");
      } else {
        printf("**BEA01 is not present, skipping remaining VRS.\n");
      }
    }
    while (VRS_OK && !Term) {
      i++;
      error = ReadVRD(VRS, i);
      if (!error) {
        if (!NSR_Found) {
          NSR_Found = !strncmp(VRS+1, VRS_ISO13346_NSR, 4);
          if (NSR_Found) {
            UDF_Version = VRS[5] & 0x0f;
            Version_OK = TRUE;
            printf("NSR0%d descriptor found.\n", UDF_Version);
          }
        } else {
          if (!strncmp(VRS+1, VRS_ISO13346_NSR, 4)) {
            printf("\n**Found an extra NSR descriptor.\n");
          }
        }
        Term = !strncmp(VRS+1, VRS_ISO13346_END, 5);
        if (Term) {
          i++;
        }
      } else {
        VRS_OK = FALSE;
      }
    }
    if (BEA_Found && !NSR_Found) {
      printf("\n**NSR0x is not present in the VRS!\n");
    }
    if (BEA_Found) {
      if (Term) {
        printf("VRS sequence was terminated.\n");
      } else {
        printf("\n**TEA01 is not present in the VRS!\n");
      }
    } else {
      printf("\n**No Extended VRS found.\n");
    }
    if (i) {
      track_volspace(SS + (32768 >> sdivshift), 
                     i * ((2048 >> sdivshift) ? (2048 >> sdivshift) : 1), 
                     "Volume Recognition Sequence");
    }
    free(VRS);
  } else {
    printf("\n**Unable to allocate memory for reading the VRS.\n");
  }
  return error;
}
