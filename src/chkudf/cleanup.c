#include <stdlib.h>
#include "chkudf.h"
#include "protos.h"

void cleanup(void)
{
  int i;

  for (i = 0; i < NUM_CACHE; i++) {
    if (Cache[i].Buffer) {
      free(Cache[i].Buffer);
    }
  }
  for (i = 0; i < PTN_no; i++) {
    switch (Part_Info[i].type) {
      case PTN_TYP_VIRTUAL:
        free(Part_Info[i].Extra);
        break;

      case PTN_TYP_SPARE:
        free(((struct _sST_desc *)Part_Info[i].Extra)->Map);
        free(Part_Info[i].Extra);
        break;
    }
  }
}
