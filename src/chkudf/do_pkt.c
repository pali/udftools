#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <linux/cdrom.h>
#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include "protos.h"

/*
 * Generic packet command processor.  The identification of the device is
 * done elsewhere, and assumed to be global.  In the case of the Linux
 * implementation, the device identification is a file handle kept in 
 * the "device" global.
 */
BOOL do_pkt(UINT8 *command, int cmd_len, UINT8 *buffer, UINT32 in_len, 
            UINT32 out_len, UINT8 *sense, int sense_len)
{
  struct cdrom_generic_command cgc;
  struct request_sense rs;
  int ret;

  memset(&cgc, 0, sizeof(cgc));
  memset(&rs, 0, sizeof(rs));
  memcpy(&cgc.cmd, command, cmd_len);
  cgc.buffer = buffer;
  cgc.buflen = MAX(in_len, out_len);
  cgc.sense = &rs;
  if (in_len && out_len)
    cgc.data_direction = CGC_DATA_UNKNOWN;
  else if (in_len)
    cgc.data_direction = CGC_DATA_WRITE;
  else if (out_len)
    cgc.data_direction = CGC_DATA_READ;
 else
    cgc.data_direction = CGC_DATA_NONE;
  ret = ioctl(device, CDROM_SEND_PACKET, &cgc);
  printf("ret=%d\n", ret);
  if (ret)
  {
    memcpy(sense, &rs, sense_len);
    printf("sense: valid=%d, error_code=%d, segment_num=%d, ili=%d, sense_key=%d, information=%x, add_sense_len=%d, command_info=%x\n",
      rs.valid, rs.error_code, rs.segment_number, rs.ili, rs.sense_key, *(int *)rs.information, rs.add_sense_len, *(int *)rs.command_info);
    printf("**PKT error %x/%02x/%02x**", sense[2] & 0xf, 
      (int)sense[12], (int)sense[13]);
    return TRUE;
  }
  else
    return FALSE;
}
