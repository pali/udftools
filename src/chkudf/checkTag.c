#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

int CheckTag(struct tag *TagPtr, UINT32 uTagLoc, UINT16 TagID,
               int crc_min, int crc_max)
{
  UINT8 checksum;
  int i, result = CHECKTAG_TAG_GOOD;
  UINT16 CRC;

  checksum = 0;
  for (i=0; i<4; i++) checksum += *((UINT8 *)TagPtr + i);
  for (i=5; i<16; i++) checksum += *((UINT8 *)TagPtr + i);
  if (TagPtr->uTagChecksum != checksum) {
    Error.Code = ERR_TAGCHECKSUM;
    Error.Sector = uTagLoc;
    Error.Expected = checksum;
    Error.Found = TagPtr->uTagChecksum;
    result = CHECKTAG_NOT_TAG;
  }

  if (!Error.Code) {
    if ((TagID != (UINT16)-1) && (TagID != U_endian16(TagPtr->uTagID))) {
      Error.Code = ERR_TAGID;
      Error.Sector = uTagLoc;
      Error.Expected = TagID;
      Error.Found = U_endian16(TagPtr->uTagID);
      result = CHECKTAG_WRONG_TAG;
    }
  }

  if (!Error.Code) {
    if ((U_endian16(TagPtr->uCRCLen) >= crc_min) && (U_endian16(TagPtr->uCRCLen) <= crc_max)) {
      CRC = doCRC((UINT8 *)TagPtr + 16, U_endian16(TagPtr->uCRCLen));
      if (CRC != U_endian16(TagPtr->uDescriptorCRC)) {
        Error.Code = ERR_TAGCRC;
        Error.Sector = uTagLoc;
        Error.Expected = CRC;
        Error.Found = U_endian16(TagPtr->uDescriptorCRC);
        result = CHECKTAG_NOT_TAG;
      }
    } else {
      Error.Code = ERR_CRC_LENGTH;
      Error.Sector = uTagLoc;
      Error.Expected = crc_min;
      Error.Found = U_endian16(TagPtr->uCRCLen);
      result = CHECKTAG_TAG_DAMAGED;
    }
  }
 
  if (!Error.Code) {
    if (uTagLoc != U_endian32(TagPtr->uTagLoc)) {
      Error.Code = ERR_TAGLOC;
      Error.Sector = uTagLoc;
      Error.Expected = uTagLoc;
      Error.Found = U_endian32(TagPtr->uTagLoc);
      result = CHECKTAG_TAG_DAMAGED;
    }
  }

  if (!Error.Code) {
    if (Version_OK && (U_endian16(TagPtr->uDescriptorVersion) != UDF_Version)) {
      Version_OK = FALSE;
      Error.Code = ERR_NSR_VERSION;
      Error.Sector = uTagLoc;
      Error.Expected = UDF_Version;
      Error.Found = U_endian16(TagPtr->uDescriptorVersion);
      result = CHECKTAG_TAG_DAMAGED;
    }
  }

  if (!Error.Code) {
    if (Serial_OK && (U_endian16(TagPtr->uTagSerialNum != Serial_No))) {
      Version_OK = FALSE;
      Error.Code = ERR_SERIAL;
      Error.Sector = uTagLoc;
      Error.Expected = Serial_No;
      Error.Found = U_endian16(TagPtr->uTagSerialNum);
      result = CHECKTAG_TAG_DAMAGED;
    }
  }

  return result;
}
