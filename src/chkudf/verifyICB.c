#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/*
 *  Read a File Entry and extract the basics.
 */

int checkICB(struct FileEntry *fe, struct long_ad FE, int dir)
{
  if (fe) {
    if (!CheckTag((struct tag *)fe, FE.Location_LBN, TAGID_FILE_ENTRY, 16, blocksize)) {
      printf("(%10d) ", fe->InfoLengthL);
    }

    if (dir && fe->sICBTag.FileType != FILE_TYPE_DIRECTORY) {
       printf("[Type: %d] ", fe->sICBTag.FileType);
    }

    if (!dir && fe->sICBTag.FileType != FILE_TYPE_RAW) {
       printf("[Type: %d] ", fe->sICBTag.FileType);
    }
  } else {
    Error.Code = ERR_READ;
    Error.Sector = FE.Location_LBN;
  }

/* Verify that the information length is consistent with the descriptors.
   Verify the recorded sectors field. */

  DumpError();
  return 0;
}

