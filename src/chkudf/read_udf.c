#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

void Check_UDF(void)
{
  VerifyVRS(); /* Verify NSR and other descriptors; extract version */

  VerifyAVDP();

  if (!Fatal) {
    VerifyVDS();
  }

  if (!Fatal) {
    DisplayDirs();
  }

  if (!Fatal) {
    TestLinkCount();
  } 

  if (!Fatal) {
    check_filespace();
  }

  if (!Fatal) {
    check_uniqueid();
  }
}
