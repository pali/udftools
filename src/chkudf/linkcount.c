#include <stdio.h>
#include "chkudf.h"
#include "protos.h"

int TestLinkCount(void)
{
  UINT32 i;

  printf("\n--Testing link counts.\n");

  for (i = 0; i < ICBlist_len; i++) {
    if (ICBlist[i].Link != ICBlist[i].LinkRec) {
      printf("**ICB at %04x:%08x has a link count of %d, found %d link%s.\n",
             ICBlist[i].Ptn, ICBlist[i].LBN, ICBlist[i].LinkRec, 
             ICBlist[i].Link, ICBlist[i].Link == 1 ? "" : "s");
    }
  }

  return 0;
}
