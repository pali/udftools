#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include "protos.h"

/* Cache everything in units of packet_size.  packet_size will be filled
 * in for all media, packet or not. */

int ReadSectors(void *buffer, UINT32 address, UINT8 Count)
{
  int readOK, result, numsecs, i;
  void *curbuffer;

  //printf("  Reading sector %d.\n", address);
  readOK = FALSE;

  /* Search cache for existing bits */
  for (i = 0; i < NUM_CACHE; i++) {
    if ((Cache[i].Count > 0) && (address >= Cache[i].Address) && 
        ((address + Count) <= (Cache[i].Address + Cache[i].Count)) &&
         (address + Count > address)) {
      readOK = 1;
      curbuffer = Cache[i].Buffer + (address - Cache[i].Address) * secsize;
      bufno = i;
      i = NUM_CACHE;
    }
  }

  if (!readOK) {
    bufno++;
    bufno %= NUM_CACHE;
    if (Count > Cache[bufno].Allocated) {
      if (Cache[bufno].Buffer) {
        free(Cache[bufno].Buffer);
      }
      Cache[bufno].Buffer = malloc(secsize * Count);
      Cache[bufno].Allocated = Count;
    }
    if (Cache[bufno].Buffer) {
      if (scsi) {
        readOK = TRUE;
        for (i = 0; i < Count; i++) {
          scsi_read10(cdb, address + i, 1, secsize, 0, 0, 0);
          result = do_scsi(cdb, 10, Cache[bufno].Buffer + i * secsize,
                   secsize, 0, sensedata, sensebufsize);
          if (result) {
            readOK = FALSE;
          }
        }
        if (readOK) {
          Cache[bufno].Address = address;
          Cache[bufno].Count = Count;
        } else {
          Cache[bufno].Count = 0;
        }
      } else {
        result = lseek(device, address * secsize, SEEK_SET);
        if (result != -1) {
          result = read(device, Cache[bufno].Buffer, secsize * Count);
          if (result == -1) {
            printf("**Read error #%d in %d\n", errno, address);
            readOK = 0;
            Cache[bufno].Count = 0;
          } else {
            if (result < secsize * Count) {
              numsecs = result / secsize;
              Cache[bufno].Address = address;
              Cache[bufno].Count = numsecs;
              readOK = numsecs > 0;
              if (readOK) {
                printf("**Only read %d sector%s.\n", numsecs, numsecs == 1 ? "" : "s");
              }
            } else {
              readOK = 1;
              Cache[bufno].Address = address;
              Cache[bufno].Count = Count;
            }   /* partial read  */
          }     /* non-scsi read */
        } else {
          readOK = 0; /* Seek failure */
        }
      }       /* scsi */
    } else {
      printf("**Couldn't malloc space for %d %d byte sectors.\n", Count, secsize);
    }
    if (readOK) {
      curbuffer = Cache[bufno].Buffer;
    } else {
      curbuffer = 0;
    }
  }
  if (curbuffer) {
    memcpy(buffer, curbuffer, Count << sdivshift);
  }
  return !readOK;
}


/* Bug: If count crosses sparing boundary... */

int ReadLBlocks(void *buffer, UINT32 address, UINT16 p_ref, UINT8 Count)
{
  sST_desc *PM_ST;
  int i, j, error, spared;

  error = 0;
  if (p_ref < PTN_no) {
    switch(Part_Info[p_ref].type) {
      case PTN_TYP_REAL:
        if (address < Part_Info[p_ref].Len) {
          error = ReadSectors(buffer, 
                              (address * s_per_b) + Part_Info[p_ref].Offs,
                              Count * s_per_b);
        }
        break;

      case PTN_TYP_VIRTUAL:
        if ((address < Part_Info[p_ref].Len) && (Count == 1)) {
          error = ReadSectors(buffer, 
                              (Part_Info[p_ref].Extra[address] * s_per_b) + 
                                Part_Info[p_ref].Offs, Count * s_per_b);
        }
        break;

      case PTN_TYP_SPARE:
        PM_ST = (struct _sST_desc *)Part_Info[p_ref].Extra;
        if (PM_ST) {
          for (j = 0; j < Count; j++) {  /* Do each block independently */
            spared = FALSE;
            for (i = 0; (i < PM_ST->Size) && !spared; i++) {
              if (((address + j) >= PM_ST->Map[i].Original)  &&
                  ((address + j) < (PM_ST->Map[i].Original + PM_ST->Extent))) {
                printf("!!Getting sector from spare area!!\n");
                spared = TRUE;
                error = ReadSectors(buffer + (j << bdivshift), Part_Info[p_ref].Extra[2*i+1], s_per_b);
              }
            }
            if (!spared) {
              error = ReadSectors(buffer + (j << bdivshift), (address + j) * s_per_b
                                     + Part_Info[p_ref].Offs, s_per_b);
            }
          }
        } else {
          error = ReadSectors(buffer, address * s_per_b + Part_Info[p_ref].Offs, 
                              Count * s_per_b);
        }
        break;

      default:
        return 1;
    }
  }
  return error;
}

/*
 * The offset and count are specified in bytes.  
 */
int ReadFileData(void *buffer, struct FileEntry *fe, UINT16 part, int offset_0, int count_0, UINT32 *data_start_loc)
{
  struct short_ad  *exts_ptr, *exts_end;
  struct long_ad   *extl_ptr, *extl_end;
  struct AllocationExtentDesc *AED = NULL;
  UINT32           sector;
  int              ADlength, error, offset, count, offset_im, count_im, firstpass;
  void            *t_buffer;

  firstpass = TRUE;
  error = 0;
  offset_im = offset_0;
  count_im  = count_0;
  t_buffer = buffer;
  while (count_im > 0 && !error) {
    offset = offset_im;
    count  = count_im;
    if (offset < U_endian32(fe->InfoLengthL)) {
      switch(U_endian16(fe->sICBTag.Flags) & ADTYPEMASK) {
        case ADSHORT:
          exts_ptr = (struct short_ad *)((char *)fe + sizeof(struct FileEntry) + U_endian32(fe->L_EA));
          exts_end = (struct short_ad *)((char *)exts_ptr + U_endian32(fe->L_AD));
          // The following while loop "eats" all unneeded extents.
          while (((offset >= (U_endian32(exts_ptr->ExtentLength.Length32) & 0x3FFFFFFF)) || 
                 ((U_endian32(exts_ptr->ExtentLength.Length32) >> 30) == E_ALLOCEXTENT)) &&
                 (exts_ptr < exts_end)) {
            if ((U_endian32(exts_ptr->ExtentLength.Length32) >> 30) == E_ALLOCEXTENT) {
              if (!AED) {
                AED = (struct AllocationExtentDesc *)malloc(blocksize);
              }
              if (AED) {
                error = ReadLBlocks(AED, U_endian32(exts_ptr->Location), part, 1);
                if (!error) {
                  error = CheckTag((struct tag *)AED, U_endian32(exts_ptr->Location), TAGID_ALLOC_EXTENT, 8, blocksize - 16);
                }
              } else {
                error = 1;
              }
              if (!error) {
                exts_ptr = (struct short_ad *)(AED + 1);
                exts_end = exts_ptr + (U_endian32(AED->L_AD) >> 3);
              } else {
                exts_ptr = exts_end;
              }
            } else {
              offset -= U_endian32(exts_ptr->ExtentLength.Length32) & 0x3FFFFFFF;
              exts_ptr++;
            }
          }
          //Now to read from the right extent
          if ((exts_ptr < exts_end) && (offset < (U_endian32(exts_ptr->ExtentLength.Length32) & 0x3FFFFFFF))) {
            sector = U_endian32(exts_ptr->Location) + (offset >> bdivshift);
            error = ReadLBlocks(t_buffer, sector, part, 1);
            if (firstpass) {
              *data_start_loc = sector;
            }
            if (!error) {
              offset_im += blocksize - (offset & (blocksize - 1));
              count_im  -= blocksize - (offset & (blocksize - 1));
              t_buffer += blocksize;
            }
          } else {
            error = 1;
          }
          free(AED);
          break;

        case ADLONG:
          extl_ptr = (struct long_ad *)((char *)fe + sizeof(struct FileEntry) + U_endian32(fe->L_EA));
          extl_end = (struct long_ad *)((char *)extl_ptr + U_endian32(fe->L_AD));
          // The following while loop "eats" all unneeded extents.
          while (((offset >= (U_endian32(extl_ptr->ExtentLength.Length32) & 0x3FFFFFFF)) || 
                 ((U_endian32(extl_ptr->ExtentLength.Length32) >> 30) == E_ALLOCEXTENT)) &&
                 (extl_ptr < extl_end)) {
            if ((U_endian32(extl_ptr->ExtentLength.Length32) >> 30) == E_ALLOCEXTENT) {
              if (!AED) {
                AED = (struct AllocationExtentDesc *)malloc(blocksize);
              }
              if (AED) {
                error = ReadLBlocks(AED, U_endian32(extl_ptr->Location_LBN), U_endian16(extl_ptr->Location_PartNo), 1);
                if (!error) {
                  error = CheckTag((struct tag *)AED, U_endian32(extl_ptr->Location_LBN), TAGID_ALLOC_EXTENT, 8, blocksize - 16);
                }
              } else {
                error = 1;
              }
              if (!error) {
                extl_ptr = (struct long_ad *)(AED + 1);
                extl_end = extl_ptr + (U_endian32(AED->L_AD) >> 4);
              } else {
                extl_ptr = extl_end;
              }
            } else {
              offset -= U_endian32(extl_ptr->ExtentLength.Length32) & 0x3FFFFFFF;
              extl_ptr++;
            }
          }
          if ((extl_ptr < extl_end) && (offset < (U_endian32(extl_ptr->ExtentLength.Length32) & 0x3FFFFFFF))) {
            sector = U_endian32(extl_ptr->Location_LBN) + (offset >> bdivshift);
            ReadLBlocks(t_buffer, sector, U_endian16(extl_ptr->Location_PartNo), 1);
            if (firstpass) {
              *data_start_loc = sector;
            }
            if (!error) {
              offset_im += blocksize - (offset & (blocksize - 1));
              count_im  -= blocksize - (offset & (blocksize - 1));
              t_buffer += blocksize;
            }
          } else {
            error = 1;
          }
          free(AED);
          break;
    
        case ADNONE:
          if ((U_endian32(fe->L_AD) != U_endian32(fe->InfoLengthL)) && !offset) {
            printf("**Embedded data error: L_AD = %d, Information Length = %d\n",
                   U_endian32(fe->L_AD), U_endian32(fe->InfoLengthL));
          }
          ADlength = MAX(U_endian32(fe->L_AD), U_endian32(fe->InfoLengthL));
          if (offset < ADlength) {
            *data_start_loc = U_endian32(fe->sTag.uTagLoc);
            memcpy(buffer, (char *)(fe + 1) + U_endian32(fe->L_EA), ADlength);
            count_im -= ADlength - offset_im;
            offset_im += ADlength - offset_im;
          } else {
            error = 1;
          }
          break;
      }
    } else {
      error = 1;
    }
    firstpass = FALSE;
  }
  memcpy(buffer, buffer + (offset_0 & (blocksize - 1)), count_0 - count_im);
  if (count_im < 0) count_im = 0;
  return count_im;
}

