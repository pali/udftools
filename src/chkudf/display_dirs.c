#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/*
 *  Read the FSD and get the root directory ICB address
 */

int GetRootDir(void)
{

  struct FileSetDesc *FSDPtr;
  int i, error, result;

  error = ERR_NO_FSD;
  FSDPtr = (struct FileSetDesc *)malloc(blocksize);
  if (FSDPtr) {
    track_filespace(U_endian16(FSD.Location_PartNo), U_endian32(FSD.Location_LBN), U_endian32(FSD.ExtentLength.Length32) & 0x3FFFFFFF);
    for (i = 0; i < (U_endian32(FSD.ExtentLength.Length32) & 0x3FFFFFFF) >> bdivshift; i++) {
      result = ReadLBlocks(FSDPtr, U_endian32(FSD.Location_LBN) + i, 
                U_endian16(FSD.Location_PartNo), 1);
      if (!result) {
        result = CheckTag((struct tag *)FSDPtr, U_endian32(FSD.Location_LBN) + i, 
                          TAGID_FSD, 496, 496);
        DumpError();
        if (result < CHECKTAG_OK_LIMIT) {
          RootDirICB = FSDPtr->sRootDirICB;
          error = 0;
          if (U_endian32(FSDPtr->sNextExtent.ExtentLength.Length32) & 0x3FFFFFFF) {
            printf("  Found another FSD extent.\n");
            FSD = FSDPtr->sNextExtent;
            i = -1;
          }
        } else {  /* All zeros, terminator, anything but an FSD */
          i = U_endian32(FSD.ExtentLength.Length32) & 0x3FFFFFFF;
        }
      } else {  /* Unreadable/blank block */
        i = U_endian32(FSD.ExtentLength.Length32) & 0x3FFFFFFF;
      }
    }
    free(FSDPtr);
  } else {
    printf("**Couldn't allocate memory for loading FSD.\n");
    return ERR_NO_FSD;
  }
  return error;
}

/*
 *  Display a directory hierarchy
 */ 

int DisplayDirs(void)
{
  int    offs[MAX_DEPTH + 1];   // Offset into current directory data 
  UINT32 addr[MAX_DEPTH + 1];   // Address of ICB of current dir      
  UINT16 part[MAX_DEPTH + 1];   // Partition of ICB of current dir    
  int depth, i, error;
  struct FileIDDesc *File;      // Directory entry for the current file
  struct FileEntry *ICB;        // ICB for current directory

  UINT32 address;
  UINT16 partition;
  int length;

  printf("\n--File Space report:\n");

  GetRootDir();

  address = U_endian32(RootDirICB.Location_LBN);
  partition = U_endian16(RootDirICB.Location_PartNo);
  length = (U_endian32(RootDirICB.ExtentLength.Length32) & 0x3FFFFFFF) >> bdivshift;

  printf("\nDisplaying directory hierarchy:\n%04x:%08x: \\", partition, address);
  File = (struct FileIDDesc *)malloc(blocksize * 2);
  ICB = (struct FileEntry *)malloc(blocksize);
  if (File && ICB) {
    depth = 1;
    offs[depth] = 0;
    addr[depth] = address;
    part[depth] = partition;
    read_icb(ICB, part[depth], addr[depth], U_endian32(RootDirICB.ExtentLength.Length32) & 0x3FFFFFFF, 
             0);
    printf("\n");
    do {
      printf("ICB %x:%05x offset %4x\n", part[depth], addr[depth], offs[depth]);
      if (offs[depth] >= U_endian32(ICB->InfoLengthL)) {
        for (i = 1; i <= depth; i++) printf("   ");
        printf("++End of directory\n");
        depth--;
      } else {
        error = GetFID(File, ICB, part[depth], offs[depth]);
        if (!error) {
          for (i = 0; i < depth; i++) printf("   ");
          if (File->Characteristics & DIR_ATTR) {
            printf("+");
          } else {
            printf("-");
          }
          if (File->Characteristics & PARENT_ATTR) {
            printf("%04x:%08x: [parent] ", U_endian16(File->ICB.Location_PartNo), 
                                           U_endian32(File->ICB.Location_LBN));
            if (File->L_FI) {
              printf("ILLEGAL NAME ");
              printDchars((char *)File + 38 + U_endian16(File->L_IU), File->L_FI);
            } else {
              printf("NAME OK");
            }
            if (depth == 1 && ((U_endian16(File->ICB.Location_PartNo) != part[depth]) ||
                               (U_endian32(File->ICB.Location_LBN)    != addr[depth]))) {
              printf(" BAD PARENT OF ROOT (should be %04x:%08x", part[depth], addr[depth]);
            } else if (depth > 1 && ((U_endian16(File->ICB.Location_PartNo) != part[depth - 1]) ||
                      (U_endian32(File->ICB.Location_LBN)    != addr[depth - 1]))) {
              printf(" BAD PARENT (should be %04x:%08x", part[depth - 1], addr[depth - 1]);
            } else {
              printf(" parent location OK");
            }
            read_icb(ICB, U_endian16(File->ICB.Location_PartNo), U_endian32(File->ICB.Location_LBN),
                      U_endian32(File->ICB.ExtentLength.Length32) & 0x3FFFFFFF, 1);
          } else {
            printf("%04x:%08x: ", U_endian16(File->ICB.Location_PartNo), U_endian32(File->ICB.Location_LBN));
            /*
             * Note: the following makes the assumption that a deleted file is no
             * longer allocated.  THIS IS WRONG according to the spec, but most
             * implementations do it this way.  This should instead be a check for
             * an extent length of zero for the ICB.
             */
            if (File->Characteristics & DELETE_ATTR) {
              printf("[DELETED] ");
              printDchars((char *)File + 38 + U_endian16(File->L_IU), File->L_FI);
            } else {
              printDchars((char *)File + 38 + U_endian16(File->L_IU), File->L_FI);
              read_icb(ICB, U_endian16(File->ICB.Location_PartNo), U_endian32(File->ICB.Location_LBN),
                        U_endian32(File->ICB.ExtentLength.Length32) & 0x3FFFFFFF, 1);
              checkICB(ICB, File->ICB, File->Characteristics & DIR_ATTR);
            }
          }
          printf("\n");
          DumpError();
          offs[depth] += (38 + File->L_FI + U_endian16(File->L_IU) + 3) & ~3;
          if ((File->Characteristics & DIR_ATTR) && 
              ! (File->Characteristics & PARENT_ATTR) &&
              ! (File->Characteristics & DELETE_ATTR)) {
            if (depth < MAX_DEPTH) {
              depth++;
              offs[depth] = 0;
              addr[depth] = U_endian32(File->ICB.Location_LBN);
              part[depth] = U_endian16(File->ICB.Location_PartNo);
            } else {
              for (i = 0; i <= depth; i++) printf("   ");
              printf(" +more subdirectories (not displayed)\n");
            }
          }
        } else {
          printf("**Error in directory\n");
          DumpError();
          depth--;
        }
      }
      /*
       * get the right ICB back.  We lost the length, but don't need it since
       * this ICB has been seen before.  So we don't disrupt the counts, we
       * claim no FID is identifying this ICB.
       */
      if (depth > 0) {
        read_icb(ICB, part[depth], addr[depth], blocksize, 0);
      }
    } while (depth > 0);
    free(File);
    free(ICB);
  } else { 
    printf("**Couldn't allocate space for FID buffer.\n");
  }
  return 0;
}

int GetFID(struct FileIDDesc *FID, struct FileEntry *fe, UINT16 part, int offset)
{
  int count;
  UINT32 location;
  
  count = ReadFileData(FID, fe, part, offset, blocksize, &location);
  if (count < blocksize) {
    CheckTag((struct tag *)FID, location, TAGID_FILE_ID, 0, blocksize - 16);
    if (Error.Code == ERR_TAGLOC) {
      printf("** Wrong Tag Location. Expected %d, Found %d (%d)\n",
         Error.Expected, Error.Found, location);
      FID_Loc_Wrong++;
      Error.Code = 0;
    }
    if (Error.Code == ERR_CRC_LENGTH) {
      DumpError();
    }
    return Error.Code;
  } else {
    return ERR_READ;
  }
}

