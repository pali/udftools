#include <sys/types.h>
#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <ctype.h>
#include <linux/cdrom.h>
#include <linux/fs.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * The following routine attempts to find the true last block based on
 * the last AVDP.  This is only needed on CD media.  The places are based
 * on the relationships between the last user data sector and the first
 * sector of the lead-out.  There can be no gap, a gap of two due to 
 * run-out blocks, a gap of 150 due to post-gap, or a gap of 152 due to
 * both run-out and post-gap.  For each of these end places, the AVDP
 * can be at N or N - 256.
 */

int AVDP_Places[] = {-2, -258, 0, -256, -152, -150, -408, -406};
int End_Places[]  = {-2,   -2, 0,    0, -152, -150, -152, -150};
int Num_Places = 8;

BOOL Get_Last_BGS()
{
  unsigned long buffer;
  int      result;
  BOOL     success = FALSE;

  result = ioctl(device, BLKGETSIZE, buffer);
  if (!result)
  {
      LastSector = buffer;
      success = TRUE;
  }
  return success;
}

/*
 * This routine uses the HP 4020/6020 non-standard READ TRACK INFORMATION
 * command to find the last sector.  This code does not cover the case
 * where the first track is numbered other than 1.
 */

BOOL Get_Last_PRTI()
{
  char     *buffer;
  int      result = 0;
  int      *ip;
  int      track_no;
  BOOL     success = FALSE;
  UINT32   trackstart, tracklength, freeblocks;

  buffer = malloc(128);
  ip = (int *)buffer;
  if (buffer) {
    memset(cdb, 0, 12);     /* Clear the request buffer */
    cdb[0] = 0xe5;          /* 4020/6020 Read Track Info */
    cdb[5] = 1;             /* Really should be from Read TOC, but who ever uses != 1? */
    cdb[8] = 20;            /* Allocation length in CDB */
    result = do_scsi(cdb, 10, buffer, 20, 0, sensedata, sensebufsize);
    if (!result) {
      track_no = buffer[1];     /* This is really number of tracks, not last TNO! */
      printf("  Proprietary Read Track Info worked; last track is %d.\n", track_no);
      cdb[5] = track_no;
      result = do_scsi(cdb, 10, buffer, 20, 0, sensedata, sensebufsize);
      if (!result) {
        trackstart = S_endian32(*(UINT32 *)(buffer + 2));
        tracklength = S_endian32(*(UINT32 *)(buffer + 6));
        freeblocks = S_endian32(ip[3]);
        /*
         * Track length includes two run-outs and link 
         */
        LastSector = trackstart + tracklength - 3;
        if (freeblocks) {
          /*
           * If the whole track isn't written, subtract the free blocks
           * and the run-in and run-out sectors that go between the written
           * and free blocks
           */
          LastSector = LastSector - freeblocks - 6;
        }
        LastSectorAccurate = TRUE;
        printf("  Proprietary RTI (e5) worked.\n");
        success = TRUE;
      }
    }
    free(buffer);
  }
  return success;
}

/*
 * The following routine determines the last written sector using the 
 * MMC READ DISC INFORMATION and READ TRACK INFORMATION Commands.  This 
 * works on most newer CD-R/RW drives.
 */
BOOL Get_Last_RTI()
{
  char     *buffer;
  int      result = 0;
  int      *ip;
  int      track_no;
  BOOL     success = FALSE;
  UINT32   trackstart, tracklength, freeblocks;

  buffer = malloc(128);
  ip = (int *)buffer;
  if (buffer) {
    memset(cdb, 0, 12);         /* Clear the request buffer */
    cdb[0] = 0x51;              /* Generic Read Disk Info */
    cdb[8] = 32;                /* Allocation length in CDB */
    result = do_scsi(cdb, 10, buffer, 32, 0, sensedata, sensebufsize);
    if (!result) {
      track_no = buffer[6];
      printf("  Generic Read Disc Info worked; last track is %d.\n", track_no);
      memset(buffer, 0, 128);
      cdb[0] = 0x52;       /* Read Track Information      */
      cdb[1] = 1;          /* For a track (not session)   */
      cdb[5] = track_no;   /* The last track              */
      cdb[8] = 36;         /* Allocation Length           */
      result = do_scsi(cdb, 10, buffer, 36, 0, sensedata, sensebufsize);
      if (!result) {
        if (buffer[6] & 0x40) {
          /*
           * Track is blank; we want the previous one
           */
          cdb[5] = track_no - 1;  /* The right last track        */
          result = do_scsi(cdb, 10, buffer, 36, 0, sensedata, sensebufsize);
        }
        if (!result) {
          trackstart = S_endian32(ip[2]);
          tracklength = S_endian32(ip[6]);
          freeblocks = S_endian32(ip[4]);
          printf("  start %d, length %d, freeblocks %d.\n", trackstart, tracklength, freeblocks);
          if (buffer[6] & 0x10) {
            printf("  Packet size %d.\n", S_endian32(ip[5]));
            LastSector = trackstart + tracklength - 1;
          } else {
            printf("  Variable packet written track.\n");
            LastSector = trackstart + tracklength - 1;
            if (freeblocks) {
              LastSector = LastSector - freeblocks - 7;
            }
          }
          LastSectorAccurate = TRUE;
          printf("  Generic RDI/RTI worked.\n");
          success = TRUE;
        }
      }
    }
    free(buffer);
  }
  return success;
}

BOOL Get_Last_ReadCap()
{
  char     *buffer;
  int      result;
  BOOL     success = FALSE;

  buffer = malloc(128);
  if (buffer) {
    memset(cdb, 0, 12);
    cdb[0] = 0x25;
    result = do_scsi(cdb, 10, buffer, 8, 0, sensedata, sensebufsize);
    if (!result) {
      LastSector = S_endian32(*(UINT32 *)buffer);
      LastSectorAccurate = TRUE;
      success = TRUE;
    }
    free(buffer);
  }
  return success;
}

BOOL Get_Last_ReadTOC()
{
  char     *buffer;
  int      result;
  BOOL     success = FALSE;
  struct cdrom_tocentry *toc;

  buffer = malloc(128);
  if (buffer) {
    memset(buffer, 0, 128);
    toc = (struct cdrom_tocentry *)buffer;
    toc->cdte_format = CDROM_LBA;
    toc->cdte_track = 0xaa;
    result = ioctl(device, CDROMREADTOCENTRY, buffer);
    if (!result) {
      LastSector = toc->cdte_addr.lba - 1;
      success = TRUE;
    }
    free(buffer);
  }
  return success;
}


void SetLastSector(void)
{
  LastSector = -1;
  LastSectorAccurate = FALSE;

  if (scsi) {
    if (isType5) {             /* Check for CD device */
      if (!Get_Last_BGS()) {            /* Block Get Size      */
        if (!Get_Last_PRTI()) {         /* Proprietary RTI     */
          if (!Get_Last_RTI()) {        /* Generic RTI         */
            if (!Get_Last_ReadCap()) {  /* Read Capacity       */
              if (!Get_Last_ReadTOC()) {/* Read TOC            */
                printf("  Couldn't determine location of last sector.\n");
              }
            }
          }
        }
      }
    } else {
      if (!Get_Last_BGS()) {      /* Block Get Size      */
        if (!Get_Last_ReadCap()) {/* Read Capacity       */
          printf("  Couldn't read capacity.\n");
        }
      }
    }
  } else {
    if (!Get_Last_BGS()) {      /* Block Get Size      */
      if (!Get_Last_ReadTOC()) {/* Read TOC            */
        printf("  Couldn't determine location of last sector.\n");
      }
    }
  }
}

void SetLastSectorAccurate(void)
{
  UINT32   TrialAddress;
  int      i, error;
  int      found = FALSE;
  struct tag *avdptag;

  avdptag = (struct tag *)malloc(secsize);
  if (avdptag) {
    /* Maybe it's CD-RW media - check first. */
    isCDRW = FALSE;
    TrialAddress = 32 * ((LastSector + 38) / 39) - 1;
    error = ReadSectors(avdptag, TrialAddress, 1);
    if (!error) {
      found = !CheckTag(avdptag, TrialAddress, 2, 0, secsize);
      ClearError();
    }
    if (found) {
      LastSector = TrialAddress;
      isCDRW = TRUE;
    } else {
      TrialAddress -= 256;
      error = ReadSectors(avdptag, TrialAddress, 1);
      if (!error) {
        found = !CheckTag(avdptag, TrialAddress, 2, 0, secsize);
        ClearError();
      }
      if (found) {
        LastSector = TrialAddress + 256;
        isCDRW = TRUE;
      }
    }

    for (i = 0; (i < Num_Places) && (!found); i++) {
      TrialAddress = LastSector + AVDP_Places[i];
      error = ReadSectors(avdptag, TrialAddress, 1);
      if (!error) {
        found = !CheckTag(avdptag, TrialAddress, 2, 0, secsize);
        ClearError();
      }
      if (found) {
        LastSector += End_Places[i];
      }
    }
  } else {
    printf("**Couldn't allocate memory for setting last block accurately.\n");
  }
  if (found) {
    printf("  Adjusted last sector to %d.\n", LastSector);
  }
}
