#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * Read the Sparing Table from the medium. This routine assumes that the 
 * sparing table is in memory identified by the Part_Info[n].Extra pointer.
 */

void GetMap(void)
{
  struct SparingTable *Spare;
  UINT16           SP;
  sST_desc         *PM_ST;
  int              i, found;

  SP = 0;
  found = FALSE;
  for (i = 0; (i < PTN_no) && !found; i++) {
    if (Part_Info[i].type == PTN_TYP_SPARE) {
      SP = i;
      found = TRUE;
    }
  }

  if (found) {
    PM_ST = (struct _sST_desc *)Part_Info[SP].Extra;
    if (PM_ST) {
      PM_ST->Map = NULL;
      printf("\n--Partition Reference %d is sparable, reading sparing maps.\n", SP);

      Spare = (struct SparingTable *)malloc(PM_ST->Size + secsize);
      if (Spare) {
        if (!ReadSectors(Spare, PM_ST->Location[0], 
                 (PM_ST->Size + secsize - 1) >> sdivshift)) {
          track_volspace(PM_ST->Location[0], 
                         (PM_ST->Size + secsize - 1) >> sdivshift,
                         "Sparing Table");
  
          CheckTag((struct tag *)Spare, PM_ST->Location[0], TAGID_NONE, 0, 16384);
          if (!Error.Code) {
            printf("  Sparing table candidate found\n");
            if(!CheckRegid(&Spare->sEntityId, E_REGID_SPARE)) {
              printf("  Structure is a sparing table.\n");
              printf("  Sparing Table contains %d entries.\n", Spare->uRT_L);
              printf("  Sparing sequence %d.\n", Spare->uSequence);
              PM_ST->Map = (struct _sMap_Entry *)malloc(Spare->uRT_L * 8);
              if (PM_ST->Map) {
                PM_ST->Size = Spare->uRT_L;
                memcpy(PM_ST->Map, Spare + 1, PM_ST->Size * 8);
              } else {
                printf("**No memory for Sparing Table. Future reads may be from the wrong place.\n");
                Error.Code = ERR_NOMAPMEM;
                Error.Sector = PM_ST->Location[0];
              }
              for (i = 0; i < PM_ST->Size; i++) {
                track_volspace(PM_ST->Map[i].Mapped, PM_ST->Extent,
                               "Set aside for sparing");
                printf("  %08x -> %08x\n", PM_ST->Map[i].Original, PM_ST->Map[i].Mapped);
              }
            } else {
              printf("**Bad Sparing Table. Future reads may be from the wrong place.\n");
              Error.Code = ERR_NO_MAP;
              Error.Sector = PM_ST->Location[0];
            }
          }
        } else {
          printf("**Couldn't read Sparing Table. Future reads may be from the wrong place.\n");
          Error.Code = ERR_NO_MAP;
          Error.Sector = PM_ST->Location[0];
        }
        free(Spare);
      }
    } else {
      printf("**Couldn't allocate memory for Sparing Partition Map Entry.\n");
    }
  } 
}
