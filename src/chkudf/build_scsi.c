#include <string.h>
#include "../nsrHdrs/nsr.h"
#include "protos.h"

/*
 * The following routine builds a MODE SENSE 10 CDB in the area pointed to
 * by buffer.
 */
char *scsi_modesense10(char *buffer, int DBD, int PC, int pagecode, 
                       int pagelength)
{
  /* 
   * Mode header and block descriptor are each 8 bytes, thus we have to
   * add stuff...
   */
  if (DBD) {
    buffer[1] |= 0x08;
    pagelength += 8;
  } else {
    pagelength += 16;
  }
  memset(buffer, 0,  pagelength);
  buffer[0] = 0x5a; /* Mode Sense   */
  buffer[2] = ((PC << 6) & 0xc0) | (pagecode & 0x3f);
  buffer[7] = (pagelength >> 8) & 0xff;
  buffer[8] = pagelength & 0xff;
  return buffer;
}

/*
 * The following routine builds a READ 10 CDB in the area pointed to by
 * buffer.
 */
char *scsi_read10(char *buffer, int LBA, int length, int sectorsize,
                  int DPO, int FUA, int RelAdr)
{
  int  *ip;

  memset(buffer, 0, 10);
  buffer[0] = 0x28;
  if (DPO) {
    buffer[1] |= 0x10;
  }
  if (FUA) {
    buffer[1] |= 0x08;
  }
  if (RelAdr) {
    buffer[1] |= 0x01;
  }
  ip = (int *)(buffer + 2);
  *ip = S_endian32(LBA);
  buffer[7] = (length >> 8) & 0xff;
  buffer[8] = length & 0xff;
  return buffer;
}

