#include <stdio.h>
#include <malloc.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * returns 1 if address 1 is greater than address 2, -1 if less than, and
 * 0 if equal. Used only by read_icb for sorting the icb list.
 */

int compare_address(UINT16 ptn1, UINT16 ptn2, UINT32 addr1, UINT32 addr2)
{
  if (ptn1 > ptn2) return 1;
  if (ptn1 < ptn2) return -1;
  if (addr1 > addr2) return 1;
  if (addr1 < addr2) return -1;
  return 0;
}

/*
 * The following routine takes a File Entry as input and tracks the space
 * used by the file data and by the Extended Attributes of that file.
 */
int track_file_allocation(struct FileEntry *FE, UINT16 ptn)
{
  int    file_length;
  int    error, sizeAD, Prev_Typ;
  struct long_ad *lad;
  struct short_ad *sad;
  struct AllocationExtentDesc *AED = NULL; 
  UINT16 ADlength;
  UINT32 Location_AEDP, ad_offset;
  UINT16 Next_ptn;
  UINT32 Next_LBN;
  UINT8 *ad_start;
  BOOL   isLAD;

  ad_offset = 0;   //Offset into the allocation descriptors
  Next_ptn = 0;    //The partition ref no of the previous AD
  Next_LBN = 0;    //The LBN of the sector after the previous AD
  Prev_Typ = -1;   //The type of the previous AD
  ADlength = U_endian32(FE->L_AD);
  switch(U_endian16(FE->sICBTag.Flags) & ADTYPEMASK) {
    case ADSHORT:
    case ADLONG:
             isLAD = (U_endian16(FE->sICBTag.Flags) & ADTYPEMASK) == ADLONG;
             sizeAD = isLAD ?  sizeof(struct long_ad) : sizeof(struct short_ad);
             file_length = 0;
             ad_start = (UINT8 *)(FE + 1) + U_endian32(FE->L_EA);
             printf("\n  [type=%s, ad_start=%p, ADlength=%d, info_length=%d]  ",
				isLAD ? "LONG" : "SHORT", ad_start, ADlength,
				U_endian32(FE->InfoLengthL));
             while (ad_offset < ADlength) {
               sad = (struct short_ad *)(ad_start + ad_offset);
               lad = (struct long_ad *)(ad_start + ad_offset);
               if (isLAD) {
                  ptn = U_endian16(lad->Location_PartNo);
               }
               printf("\n    [ad_offset=%d, atype=%d, loc=%d, len=%d, file_length=%d]  ",
					ad_offset,
					U_endian32(sad->ExtentLength.Length32) >> 30,
					U_endian32(sad->Location),
					U_endian32(sad->ExtentLength.Length32) & 0x3FFFFFFF,
					file_length);
               if (U_endian32(sad->ExtentLength.Length32) & 0x3FFFFFFF) {
                 switch(U_endian32(sad->ExtentLength.Length32) >> 30) {
                   case E_RECORDED:
                   case E_ALLOCATED:
                                   track_filespace(ptn, U_endian32(sad->Location), U_endian32(sad->ExtentLength.Length32) & 0x3FFFFFFF);
                                   if ((ptn == Next_ptn) && (U_endian32(sad->Location) == Next_LBN) && 
                                       ((U_endian32(sad->ExtentLength.Length32) >> 30) == Prev_Typ)) {
                                     Error.Code = ERR_SEQ_ALLOC;
                                     Error.Sector = FE->sTag.uTagLoc;
                                     Error.Expected = Next_LBN;
                                     DumpError();
                                   }
                                   Next_ptn = ptn;
                                   Next_LBN = U_endian32(sad->Location) + ((U_endian32(sad->ExtentLength.Length32) && 0x3FFFFFFF) >> bdivshift);
                                   Prev_Typ = U_endian32(sad->ExtentLength.Length32) >> 30;
                                   if (file_length >= U_endian32(FE->InfoLengthL)) {
                                     printf(" (Tail)");
                                   } else {
                                     file_length += U_endian32(sad->ExtentLength.Length32) & 0x3FFFFFFF;
                                   }
                                   ad_offset += sizeAD;
                                   break;
                   case E_UNALLOCATED:
                                   if (file_length >= U_endian32(FE->InfoLengthL)) {
                                     printf(" (ILLEGAL TAIL)");
                                   } else {
                                     file_length += U_endian32(sad->ExtentLength.Length32) & 0x3FFFFFFF;
                                   }
                                   ad_offset += sizeAD;
                                   printf(" --Unallocated Extent--");
                                   break;
                   case E_ALLOCEXTENT:
                                   track_filespace(ptn, U_endian32(sad->Location), U_endian32(sad->ExtentLength.Length32) & 0x3FFFFFFF);
                                   if (!AED) {
                                     AED = (struct AllocationExtentDesc *)malloc(blocksize);
                                   }
                                   if (AED) {
                                     Location_AEDP = U_endian32(sad->Location);
                                     error = ReadLBlocks(AED, Location_AEDP, ptn, 1);
                                     if (!error) {
                                       error = CheckTag((struct tag *)AED, Location_AEDP, TAGID_ALLOC_EXTENT, 8, blocksize - 16);
                                     }
                                   } else {
                                     error = 1;
                                   }
                                   if (error == 2) {
                                     if (U_endian32(AED->sTag.uTagLoc) == 0xffffffff) {
                                       error = 0;
                                       Error.Code = 0;
                                     } else {
                                       DumpError();
                                       error = 0;
                                     }
                                   }
                                   if (!error) {
                                     ad_start = (UINT8 *)(AED + 1);
                                     ADlength = U_endian32(AED->L_AD);
                                     ad_offset = 0;
                                   } else {
                                     ad_offset = ADlength;
                                   }
									printf("\n      [NEW ad_start=%p, ADlength=%d]  ",
										ad_start, ADlength);
                                   break;
                 }
               } else {
                 ad_offset = ADlength;
               }
             }
				printf("  [file_length=%d]  ", file_length);
             if (file_length != U_endian32(FE->InfoLengthL)) {
               if (((FE->InfoLengthL + blocksize - 1) & ~(blocksize - 1)) == 
                   file_length) {
                 printf(" **ADs rounded up");
               } else {
                 Error.Code = ERR_BAD_AD;
                 Error.Sector = U_endian32(FE->sTag.uTagLoc);
                 Error.Expected = U_endian32(FE->InfoLengthL);
                 Error.Found = file_length;
               }
             }
             free (AED);
             break;

    case ADNONE:
             if (U_endian32(FE->InfoLengthL) != U_endian32(FE->L_AD)) {
               Error.Code = ERR_BAD_AD;
               Error.Sector = U_endian32(FE->sTag.uTagLoc);
               Error.Expected = U_endian32(FE->InfoLengthL);
               Error.Found = U_endian32(FE->L_AD);
             }
             break;
  }

  return error;
}

/* 
 * This routine walks an ICB hierarchy, marking space as allocated as it
 * goes.  The authoritative FE is noted in the FE_ptn and FE_LBN fields
 * of the entry in the ICB list.
 *
 * Currently, the space map does not have "owners" attached to allocation.
 * This means that on write once media, errors will be generated when more
 * than one File Entry in an ICB hierarchy identifies the same space.
 */
int walk_icb_hierarchy(struct FileEntry *FE, UINT16 ptn, UINT32 Location, 
                   UINT32 Length, int ICB_offs)
{
  int i, error;

  /*
   * Mark the ICB extent as allocated
   */
  track_filespace(ptn, Location, Length);

  /*
   * Read each sector in turn (1 sector == 1 ICB)
   */
  for (i = 0; i < (Length >> bdivshift); i++) {
    error = ReadLBlocks(FE, Location + i, ptn, 1);
    if (!error) {
      if (!CheckTag((struct tag *)FE, Location + i, TAGID_FILE_ENTRY, 16, Length)) {
        ICBlist[ICB_offs].LinkRec = U_endian16(FE->LinkCount);
        ICBlist[ICB_offs].UniqueID_L = U_endian32(FE->UniqueIdL);
        ICBlist[ICB_offs].FE_LBN = Location + i;
        ICBlist[ICB_offs].FE_Ptn = ptn;
        track_file_allocation(FE, ptn);
      } else {
        /*
         * A descriptor was found that wasn't a File Entry.
         */
        ClearError();
        if (!CheckTag((struct tag *)FE, Location + i, TAGID_INDIRECT, 16, Length)) {
          walk_icb_hierarchy(FE, U_endian32(((struct IndirectEntry *)FE)->sIndirectICB.Location_LBN),
                       U_endian16(((struct IndirectEntry *)FE)->sIndirectICB.Location_PartNo),
                       U_endian32(((struct IndirectEntry *)FE)->sIndirectICB.ExtentLength.Length32) & 0x3FFFFFFF,
                       ICB_offs);
        } else {
          DumpError();  // Wasn't a file entry, but should have been.
        }
      }
    } else {
      Error.Code = ERR_READ;
      Error.Sector = Location;
      i = Length;
    }  /* Read/didn't read sector */
  }    /* Do each ICB in the extent */
  return error;
}


/*
 * This routine takes a partition, location, and length of an ICB extent as
 * input, marks the appropriate space as allocated in the space map, 
 * maintains a link count for the ICB, and returns the appropriate File
 * File Entry data in *FE.
 *
 * If FID == 0, this is a root entry or a re-read of an ICB.  If FID == 1,
 * this is the first read of an FE from a particular FID.  Note that the FE
 * might have been read before, but due to another FID.  
 *
 * Summary:
 *   FID == 0, space is not tracked and link counts not incremented.
 *   FID == 1, space is tracked and link counts are incremented.
 */
int read_icb(struct FileEntry *FE, UINT16 ptn, UINT32 Location, UINT32 Length, 
             int FID)
{
  UINT32 interval;
  INT32  ICB_offs;
  int    error, temp;
  struct FileEntry *EA;

  error = 0;

  if (Length) {
    /*
     * If there's something to track...
     */
    ICB_offs = ICBlist_len >> 1; // start halfway for binary search
    temp = ICB_offs;
    interval = 1;
    while (temp) {
      interval <<= 1;
      temp >>= 1;
    }                          // set interval to 1/4 - 1/2 for binary search
    temp = 1;              /* temp is used as a relative position 
                            * indicator.  If temp = 0, ICB_offs exactly
                            * identifies the entry.  If temp = -1, ICB_offs
                            * points to an entry that should come before the
                            * new one.  If temp = 1, ICB_offs points to an
                            * entry that should come after the new one (or
                            * the end of the list)
                            */
                                
    while ((interval > 0) && ICBlist_len) {
      interval >>= 1;
      temp = compare_address(ICBlist[ICB_offs].Ptn, ptn, ICBlist[ICB_offs].LBN, Location);
      if (temp == 0) {
        interval = 0;
        if (FID) {
          /*
           * A FID was pointing to this ICB, which we already have tracked.
           * Increment our link count to note the fact.
           */
          ICBlist[ICB_offs].Link++;
        }
        ReadLBlocks(FE, ICBlist[ICB_offs].FE_LBN, ICBlist[ICB_offs].FE_Ptn, 1);
      } else if (temp == 1) {
        ICB_offs -= interval;
        if (ICB_offs < 0) ICB_offs = 0;
      } else {
        ICB_offs += interval;
        if (ICB_offs >= ICBlist_len) ICB_offs = ICBlist_len - 1;
      }
    }
    if (temp) {
      /*
       * No match was found in the tracked list.
       * This code inserts an entry for a new ICB hierarchy.
       * The above code may have left the pointer to a point either
       * before or after the insertion point.
       */
      while ((temp == -1)  && (ICB_offs < (ICBlist_len - 1))) {
        ICB_offs++;
        temp = compare_address(ICBlist[ICB_offs].Ptn, ptn, ICBlist[ICB_offs].LBN, Location);
      }
  
      /*
       * ICB_offs now points to the first entry greater than the one we 
       * are inserting or the end of the list.
       */
      ICBlist_len++;
      if (ICBlist_len > ICBlist_alloc) {
        ICBlist = realloc(ICBlist, (ICBlist_alloc + ICB_Alloc) * sizeof(struct _sICB_trk));
        if (ICBlist) {
          ICBlist_alloc += ICB_Alloc;
        } else {
          Error.Code = ERR_NO_ICB_MEM;
        }
      }
      for (temp = ICBlist_len -1; temp > ICB_offs; temp--) {
        ICBlist[temp] = ICBlist[temp - 1];
      }
      if ((compare_address(ICBlist[ICB_offs].Ptn, ptn, ICBlist[ICB_offs].LBN, Location) < 0) &&
         (ICB_offs < (ICBlist_len - 1))) {
        ICB_offs++;
      }
      ICBlist[ICB_offs].LBN = Location;
      ICBlist[ICB_offs].Ptn = ptn;
      ICBlist[ICB_offs].UniqueID_L = 0;
      ICBlist[ICB_offs].LinkRec = 0;
      if (FID) {
        ICBlist[ICB_offs].Link = 1;
      } else {
        ICBlist[ICB_offs].Link = 0;
      }
      walk_icb_hierarchy(FE, ptn, Location, Length, ICB_offs);
      switch(FE->sICBTag.FileType) {
          case FILE_TYPE_UNSPECIFIED:
                           Num_Type_Err++;
                           break;
          case FILE_TYPE_DIRECTORY:
                           Num_Dirs++;
                           break;
          case FILE_TYPE_RAW:
                           Num_Files++;
                           break;
      }
      if (U_endian32(FE->sExtAttrICB.ExtentLength.Length32) & 0x3FFFFFFF) {
        EA = (struct FileEntry *)malloc(blocksize);
        if (EA) {
          if (U_endian16(EA->sExtAttrICB.Location_PartNo) < PTN_no) {
            printf(" EA: [%x:%08x]", U_endian16(FE->sExtAttrICB.Location_PartNo), 
                   U_endian32(FE->sExtAttrICB.Location_LBN));
            read_icb(EA, U_endian16(FE->sExtAttrICB.Location_PartNo), U_endian32(FE->sExtAttrICB.Location_LBN),
                  U_endian32(FE->sExtAttrICB.ExtentLength.Length32) & 0x3FFFFFFF, 0);
          } else {
            printf("\n**EA field contains illegal partition reference number.\n");
          }
          free(EA);
        }
      }   
    }
  }        /* If something to track */      
  if (error) {
    DumpError();
  }
  return error;
}
