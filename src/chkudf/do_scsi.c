#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include "protos.h"

/*
 * Generic SCSI command processor.  The identification of the device is
 * done elsewhere, and assumed to be global.  In the case of the Linux
 * implementation, the device identification is a file handle kept in 
 * the "device" global.
 */
BOOL do_scsi(UINT8 *command, int cmd_len, UINT8 *buffer, UINT32 in_len, 
             UINT32 out_len, UINT8 *sense, int sense_len)
{
  UINT32   *ip;
  UINT32   buffer_needed;
  BOOL     fail = TRUE;
  int      result;

  buffer_needed = MAX(in_len, out_len) + 8 + cmd_len;
  if (buffer_needed > scsibufsize) {
    scsibuf = realloc(scsibuf, buffer_needed);
  }
  if (scsibuf) {
    memset(scsibuf, 0, buffer_needed);
    ip = (UINT32 *)scsibuf;
    ip[0] = out_len;
    ip[1] = in_len;
    memcpy(scsibuf + 8, command, cmd_len);
    memcpy(scsibuf + 8 + cmd_len, buffer, out_len);
    result = ioctl(device, 1, scsibuf);
    if (result == 0) {
      memcpy(buffer, scsibuf + 8, in_len);
      fail = FALSE;
    } else {
      if (result == 0x28000000) {
        memcpy(sense, scsibuf + 8, sense_len);
        printf("**SCSI error %x/%02x/%02x**", sense[2] & 0xf, 
               (int)sense[12], (int)sense[13]);
      } else if (result == 0x25040000) {
        printf("SCSI error - can't talk to drive.\n");
      } else if (result < 0) {
        printf("ioctl error %d.\n", -result);
      } else {
        printf("Unknown ioctl error 0x%08x.\n", result);
      }
    }
  } //if scsibuf allocated
  return fail;
}
