#ifndef NSR_H
#define NSR_H
/********************************************************************/
/*  nsr.h - NSR structures, tags and definitions                    */
/*                                                                  */
/*      Unpublished Confidential Information of Hewlett-Packard     */
/*      Company. Do not disclose.  Copyright (c) Hewlett-Packard    */
/*      Company 1996. All rights reserved                           */
/*                                                                  */
/* The contents of this software are proprietary and confidential   */
/* the Hewlett- Packard Company, and are limited in distribution to */
/* those with a direct need to know.  Individuals having access to  */
/* this software are responsible for maintaining the                */
/* confidentiality of the content and for keeping the software      */
/* secure when not in use.  Transfer to any party is strictly       */
/* forbidden other than as expressly permitted in writing by        */
/* Hewlett-Packard Company.  Unauthorized transfer to or possession */
/* by any unauthorized party may be a criminal offense.             */
/*                                                                  */
/*                   RESTRICTED RIGHTS LEGEND                       */
/*                                                                  */
/*      Use,  duplication,  or disclosure by the                    */
/*      U. S. Government is subject to restrictions as set forth    */
/*      in subparagraph (c) (1) (ii) in the Rights in Technical     */
/*      Data and Computer Software clause in DFARS 252.227-7013     */
/*      or any other successor clause.                              */
/*                                                                  */
/*                       HEWLETT-PACKARD COMPANY                    */
/*                       3000 Hanover St.                           */
/*                       Palo Alto, CA  94304 U.S.A.                */
/*                                                                  */
/*      Rights for non-DOD U.S. Government Departments and Agencies */
/*      are as set forth in FAR 52.227-19 (c) (1,2) or any other    */
/*      successor clause.                                           */
/*                                                                  */
/********************************************************************/
/*
 $Source: /tmp/cvs/udf/tools/src/nsrHdrs/nsr.h,v $
 $Revision: 1.25 $      $Author: bfennema $
 $State: Exp $          $Locker:  $
 $Date: 1999/11/23 07:48:00 $
 ********************************************************************/

#include "nsr_sys.h"
#include "nsr_part1.h"
#include "nsr_part2.h"
#include "nsr_part3.h"
#include "nsr_part4.h"
#include "udf.h"

#define byteClear   0x00

#define BITZERO     0x01
#define BITONE      0x02
#define BITTWO      0x04
#define BITTHREE    0x08
#define BITFOUR     0x10
#define BITFIVE     0x20
#define BITSIX      0x40
#define BITSEVEN    0x80
#define BITEIGHT    0x100
#define BITNINE     0x200
#define BITTEN      0x400
#define BITELEVEN   0x800
#define BITTWELVE   0x1000
#define BITTHIRTEEN 0x2000
#define BITFOURTEEN 0x4000
#define BITFIFTEEN  0x8000


#define bitsPerByte 0x08

#define FALSE 0
#define TRUE  1

/* tag id's for ISO/IEC 13346 structures */

#define TAGID_NONE             (UINT16) 0   /* no tag */
#define TAGID_PVD              (UINT16) 1   /* primary volume desc */
#define TAGID_ANCHOR           (UINT16) 2   /* anchor desc */
#define TAGID_POINTER          (UINT16) 3   /* pointer desc */
#define TAGID_IUD              (UINT16) 4   /* implementation use desc */
#define TAGID_PD               (UINT16) 5   /* volume partition desc */
#define TAGID_LVD              (UINT16) 6   /* logical volume desc */
#define TAGID_USD              (UINT16) 7   /* unallocated volume space desc */
#define TAGID_TERM_DESC        (UINT16) 8   /* terminator desc */
#define TAGID_LVID             (UINT16) 9   /* logical volume integrity desc */
#define TAGID_FSD              (UINT16) 256  /* file set desc */
#define TAGID_FILE_ID          (UINT16) 257  /* file identifier desc */
#define TAGID_ALLOC_EXTENT     (UINT16) 258  /* Allocation extent desc */
#define TAGID_INDIRECT         (UINT16) 259  /* Indirect entry */
#define TAGID_TERM_ENTRY       (UINT16) 260  /* Terminal entry */
#define TAGID_FILE_ENTRY       (UINT16) 261  /* File entry */
#define TAGID_EXT_ATTR         (UINT16) 262  /* Extended attribute desc */
#define TAGID_UNALLOC_SP_ENTRY (UINT16) 263  /* Unallocated space entry (WORM)*/
#define TAGID_SPACE_BMAP       (UINT16) 264  /* Space bitmap desc */
#define TAGID_PART_INTEGRITY   (UINT16) 265  /* Partition integrity desc */

/* Stuff for display tool */

#define OSTA_LVINFO_ID ("*UDF LV Info")
#define USE_HdrLen     (sizeof(struct UnallocSpEntry))
#define USE_ADStart(x) (void *)((char *)(x)+USE_HdrLen)
#define FE_EAStart(x)  (void *)((char *)(x)+(sizeof(struct FileEntry)))
#define FE_HdrLen(x)   (sizeof(struct FileEntry)+(x)->L_EA)
#define FE_ADStart(x)  (void *)((char *)(x)+FE_HdrLen(x))
#define AE_HdrLen      (sizeof(struct AllocationExtentDesc))
#define AE_ADStart(x)  (void *)((char *)(x)+AE_HdrLen)

#define DIRTYREGID   BITZERO
#define PROTECTREGID BITONE

#define WRPROTECT_HARD BITZERO
#define WRPROTECT_SOFT BITONE

#endif  /* NSR_H */
