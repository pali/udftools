/*	wrudf-desc.c  
 *
 * PURPOSE
 *	Routines to create/find/insert/delete FileIdentDesc's and FileEntries.
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *
 *	(C) 2001 Enno Fennema <e.fennema@dataweb.nl>
 *
 * HISTORY
 *  	16 Aug 01  ef  Created.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "wrudf.h"

/*	removeFID()
 *
 *	Physically remove a FileIdentDesc from the directory
 */
int
removeFID(Directory *dir, struct fileIdentDesc *fid) 
{
    uint32_t lenFID = (sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3;
    uint32_t lenMove;
    struct fileEntry *fe;

    fe = (struct fileEntry *)dir->fe;
    fe->informationLength -= lenFID;
    lenMove = fe->informationLength - ((char *)fid - dir->data);
    memcpy(fid, (char *)fid + lenFID, lenMove);
    dir->dirDirty = 1;
    return 0;
}

/*	deleteFID()
 *
 *	Remove an FID from the directory
 *	If fileLinkCount now zero, deallocate FileEntry and any data extents
 */
int 
deleteFID(Directory * dir, struct fileIdentDesc * fid)
{
    int		rv;
    struct fileEntry *fe;
    struct fileEntry *dirfe;
    struct logicalVolIntegrityDescImpUse *lvidiu;

    dirfe = (struct fileEntry *)dir->fe;

    rv = CMND_OK;

//    if( fid->icb.extLocation.partitionReferenceNum != pd->partitionNumber )
//	return NOT_IN_RW_PARTITION;

    fe = readTaggedBlock(fid->icb.extLocation.logicalBlockNum, fid->icb.extLocation.partitionReferenceNum);

    /* check permission */

    freeBlock(fid->icb.extLocation.logicalBlockNum, fid->icb.extLocation.partitionReferenceNum);

    if( fe->fileLinkCount > 1 ) {
	fe->fileLinkCount--;
	setChecksum(fe);
	if( medium == CDRW ) 
	    dirtyBlock(fid->icb.extLocation.logicalBlockNum, fid->icb.extLocation.partitionReferenceNum);
	else
	    vat[fid->icb.extLocation.logicalBlockNum] = writeCDR(fe);
    } else {
	if( medium == CDR ) {
	    if( fe->icbTag.fileType == ICBTAG_FILE_TYPE_DIRECTORY )
		dirfe->fileLinkCount--;
	    vat[fid->icb.extLocation.logicalBlockNum] = 0xFFFFFFFF;	
	} else {
	    if( fe->icbTag.fileType == ICBTAG_FILE_TYPE_DIRECTORY ) {
		dirfe->fileLinkCount--;
		lvidiu = (struct logicalVolIntegrityDescImpUse*)
		    (lvid->data + 2 * sizeof(uint32_t) * lvid->numOfPartitions);
		lvidiu->numDirs--;
	    } else {
		lvidiu = (struct logicalVolIntegrityDescImpUse*)
		    (lvid->data + 2 * sizeof(uint32_t) * lvid->numOfPartitions);
		lvidiu->numFiles--;
	    }

	    /* free file data */
	    switch( fe->icbTag.flags & ICBTAG_FLAG_AD_MASK ) {
	    case ICBTAG_FLAG_AD_IN_ICB:
		break;
	    case ICBTAG_FLAG_AD_SHORT:
		freeShortExtents((short_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr));
		break;
	    case ICBTAG_FLAG_AD_LONG:
		freeLongExtents((long_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr));
		break;
	    default:
		printf("UDF does not use Extended Alloc Descs\n");
		rv = CMND_FAILED;
	    }
	/* free the File Entry itself */
	markBlock(FREE, fid->icb.extLocation.logicalBlockNum);
	}
    }

    removeFID(dir, fid);
    dir->dirDirty = 1;

    return rv;
}

struct fileIdentDesc* 
findFileIdentDesc(Directory *dir, char* name) 
{
    uint64_t		i;
    char		*data;
    struct fileEntry	*fe;
    struct fileIdentDesc *fid;
    dchars              uName[256];
    size_t              uLen;

    uLen = encode_locale(uName, name, 256);
    if (uLen == (size_t)-1)
        return NULL;

    data = dir->data;
    fe = (struct fileEntry *)dir->fe;

    for( i = 0; i < fe->informationLength; 
	 i += (sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3 ) {

	fid = (struct fileIdentDesc*)(data + i);

	if( fid->descTag.tagIdent == TAG_IDENT_TE )
	    return NULL;

	if( fid->descTag.tagIdent == TAG_IDENT_IE ) {
	    printf("Indirect Entry not yet implemented\n");
	    continue;
	}
	if( fid->descTag.tagIdent != TAG_IDENT_FID )
	    fail("Unknown tag id %08X in directory\n", fid->descTag.tagIdent);

	// check CRC

	if( fid->fileCharacteristics == FID_FILE_CHAR_PARENT)
	    continue;

	if( fid->lengthFileIdent == uLen
	    && memcmp( fid->impUseAndFileIdent, uName, fid->lengthFileIdent) == 0 )
	    return fid;
    }
    return NULL;
}


/*
 *	Return pointer to new FileEntry in a 2k allocated block to be free'd when no longer required.
 *	Only basic data is filled in. 
 */
struct fileEntry* 
makeFileEntry() 
{
    struct fileEntry *fe = (struct fileEntry*) calloc(2048, 1);
    struct logicalVolHeaderDesc* lvhd = (struct logicalVolHeaderDesc*)lvid->logicalVolContentsUse;

    updateTimestamp(0,0);
    fe->descTag.tagIdent = TAG_IDENT_FE;
    fe->descTag.descVersion = 2;
    fe->descTag.tagSerialNum = lvd->descTag.tagSerialNum;
    fe->icbTag.strategyType = 4;
    fe->icbTag.numEntries = 1;
    fe->icbTag.fileType = ICBTAG_FILE_TYPE_REGULAR;
    /* User has all permission; Group/Others Read/Exec */
    fe->permissions = FE_PERM_U_READ | FE_PERM_U_WRITE | FE_PERM_U_DELETE | FE_PERM_U_CHATTR | FE_PERM_U_EXEC |
	FE_PERM_G_READ | FE_PERM_G_EXEC | FE_PERM_O_READ | FE_PERM_O_EXEC;
    fe->fileLinkCount = 1;
    fe->accessTime = timeStamp;
    fe->modificationTime = timeStamp;
    fe->attrTime = timeStamp;
    fe->impIdent = entityWRUDF;
    fe->uniqueID = lvhd->uniqueID++;
    return fe;
}


/*
 *	Return pointer to new FileIdentDesc in an allocated block to be free'd when no longer required.
 *	Does NOT fill: descTag.checksum, descTag.tagLocation, icbTag.characteristics, icb
 */
struct fileIdentDesc*
makeFileIdentDesc(char* name) 
{
    dchars uName[256];
    size_t uLen;
    struct fileIdentDesc *fid;

    fid = (struct fileIdentDesc*) calloc(512,1);
    fid->descTag.tagIdent = TAG_IDENT_FID;
    fid->descTag.descVersion = 2;
    fid->descTag.tagSerialNum = lvd->descTag.tagSerialNum;
    fid->fileVersionNum = 1;
    fid->icb.extLength = 2048;

    if( name[0] != 0 ) {				/* FID to parent directory has no name */
	uLen = encode_locale(uName, name, 256);
	if (uLen == (size_t)-1) {
		uLen = 1;
		uName[0] = 0x8;
	}
	fid->lengthFileIdent = uLen;
	memcpy(fid->impUseAndFileIdent + fid->lengthOfImpUse, uName, fid->lengthFileIdent);
    }
    return fid;
} 


int 
insertFileIdentDesc(Directory *dir, struct fileIdentDesc* fid) 
{
    uint32_t		lenFid;
    struct fileEntry	*fe;
    
    lenFid = (sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3;
    fid->descTag.descCRCLength = lenFid - sizeof(tag);
    setChecksum(fid);

    fe = (struct fileEntry *)dir->fe;

    if( fe->informationLength + lenFid > dir->dataSize ) {
	if( !(dir->data = realloc(dir->data, dir->dataSize + 2048)) ) {
	    fail("Realloc directory data failed\n");
	}
	dir->dataSize += 2048;
    }
    
    memcpy(dir->data + fe->informationLength, fid, lenFid);
    fe->informationLength += lenFid;
    dir->dirDirty = 1;
    return CMND_OK;
}
