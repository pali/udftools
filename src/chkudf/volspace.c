#include <stdio.h>
#include "chkudf.h"
#include "protos.h"

int track_volspace(UINT32 Location, UINT32 Length, char *Name)
{
  int i, j, error;

  error = 0;

  if (Length > 0) {  
    //point to the location where the new extent is >= the list member
    for (i = 0; (i < VolSpaceListLen) && (Location > VolSpace[i].Location); i++);
    if (i > 0) {
      if (((Location + Length - 1) >= VolSpace[i-1].Location) && 
          (Location < (VolSpace[i-1].Location + VolSpace[i-1].Length))) {
        Error.Code = ERR_VOL_SPACE_OVERLAP;
        Error.Sector = Location;
        error = TRUE;
      }
    }
    if (i < VolSpaceListLen) {
      if (((Location + Length - 1) >= VolSpace[i].Location) && 
          (Location < (VolSpace[i].Location + VolSpace[i].Length))) {
        Error.Code = ERR_VOL_SPACE_OVERLAP;
        Error.Sector = Location;
        error = TRUE;
      }
    }
    if (VolSpaceListLen < MAX_VOL_EXTS - 1) {
      for (j = VolSpaceListLen; j > i; j--) {
        VolSpace[j] = VolSpace[j - 1];
      }
      VolSpace[i].Location = Location;
      VolSpace[i].Length = Length;
      VolSpace[i].Name = Name;
      VolSpaceListLen++;
    } else {
      printf("**Too many volume extents.");
    }
  //  printf("\nVolume Allocation list:\n");
  //  for (i = 0; i < VolSpaceListLen; i++) {
  //    printf("  %8d %d\n", VolSpace[i].Location, VolSpace[i].Length);
  //  }
    if (error) {
      DumpError();
    }
  }
  return error;
}
