#include <stdio.h>
#include "chkudf.h"
#include "../nsrHdrs/nsr.h"
#include "protos.h"

void DumpError(void)
{
  if (Error.Code > 0) {
    printf("**[%08x] ", Error.Sector);
    printf(Error_Msgs[Error.Code - 1], Error.Expected, Error.Found);
    printf(".\n");
  }
  ClearError();
}

void ClearError(void)
{
  Error.Code     = ERR_NONE;
  Error.Sector   = 0;
  Error.Expected = 0;
  Error.Found    = 0;
}
