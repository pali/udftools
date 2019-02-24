/* 	wrudf-cmnd.c
 *
 * PURPOSE
 *	High level wrudf command functions
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *
 *	(C) 2001 Enno Fennema <e.fennema@dataweb.nl>
 *
 * HISTORY
 *	16 Aug 01  ef  Created.
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "wrudf.h"


int	deleteDirectory(Directory *dir, struct fileIdentDesc *fid);
Directory *makeDir(Directory *dir, char* name);
int	questionOverwrite(Directory *dir, struct fileIdentDesc *fid, char* name);
int	directoryIsEmpty(Directory *dir);

char	*hdWorkingDir;

/*	copyFile()
 *	Write File Entry immediately followed by data
 *	A verify error on CDR causes a further packet with
 *	the new FE followed by copies of the defective blocks only
 */
int
copyFile(Directory *dir, char* inName, char*newName, struct stat *fileStat) 
{
    uint32_t	i=0;
    int		fd, blkno;
    uint32_t	nBytes, blkInPkt;
    uint32_t	maxVarPktSize;		// in bytes
    struct fileIdentDesc *fid;
    struct fileEntry *fe;
    struct allocDescImpUse *adiu;
    struct logicalVolIntegrityDescImpUse *lvidiu;
    uint8_t	p[2048];

    fd = open(inName, O_RDONLY);
    if( fd == 0 ) {
	printf("'%s' does not exist\n", inName);
	return CMND_FAILED;
    }

    printf("Copy file %s\n", inName);
    fid = findFileIdentDesc(dir, newName);

    if( fid  && questionOverwrite(dir, fid, newName) ) {
	close(fd);
	return CMND_OK;
    }

    fid = makeFileIdentDesc(newName);				// could reuse FID if overwrite allowed
	    
    fe = makeFileEntry();
    fe->uid = fileStat->st_uid;
    fe->gid = fileStat->st_gid;
    if( fileStat->st_mode & S_IRUSR ) fe->permissions |= FE_PERM_U_READ;
    if( fileStat->st_mode & S_IWUSR ) fe->permissions |= FE_PERM_U_WRITE | FE_PERM_U_DELETE | FE_PERM_U_CHATTR;;
    if( fileStat->st_mode & S_IXUSR ) fe->permissions |= FE_PERM_U_EXEC;
    if( fileStat->st_mode & S_IRGRP ) fe->permissions |= FE_PERM_G_READ;
    if( fileStat->st_mode & S_IWGRP ) fe->permissions |= FE_PERM_G_WRITE | FE_PERM_G_DELETE | FE_PERM_G_CHATTR;
    if( fileStat->st_mode & S_IXGRP ) fe->permissions |= FE_PERM_G_EXEC;
    if( fileStat->st_mode & S_IROTH ) fe->permissions |= FE_PERM_O_READ;
    if( fileStat->st_mode & S_IWOTH ) fe->permissions |= FE_PERM_O_WRITE | FE_PERM_O_DELETE | FE_PERM_O_CHATTR;;
    if( fileStat->st_mode & S_IXOTH ) fe->permissions |= FE_PERM_O_EXEC;
    if( fileStat->st_mode & S_ISUID ) fe->icbTag.flags |= ICBTAG_FLAG_SETUID;
    if( fileStat->st_mode & S_ISGID ) fe->icbTag.flags |= ICBTAG_FLAG_SETGID;

    fe->informationLength = fileStat->st_size;
    updateTimestamp(fileStat->st_atime, 0);
    fe->accessTime = timeStamp;
    updateTimestamp(fileStat->st_mtime, 0);
    fe->modificationTime = timeStamp;
    updateTimestamp(fileStat->st_ctime, 0);
    fe->attrTime = timeStamp;

    /* check whether embedding of data is possible  */
    fe->logicalBlocksRecorded = ((fileStat->st_size + 2047) & ~2047) >> 11;

    if( medium == CDR ) {
	/*	File data written in physical space but fileEntry in virtual space
	 *	so must use long allocation descriptors
	 *
	 *	Variable packet length restricted by drive buffer size.
	 *	Must break up long file over several var packets.
	 *	Only 116 long_ad's fit in fileEntry without going to extended alloc descs
	 */
	long_ad	*ad;
	uint32_t	loc;

	maxVarPktSize = getMaxVarPktSize();
	
	if( fe->informationLength / maxVarPktSize > 116 ) {
	    printf("Cannot handle files longer than %d\n", 116 * maxVarPktSize);
	    close(fd);
	    free(fe);
	    free(fid);
	    return CMND_FAILED;
	}

	fe->icbTag.flags |= ICBTAG_FLAG_AD_LONG;
	/* +1 as the fileEntry itself occupies block NWA */
	loc = getNWA() + 1 - pd->partitionStartingLocation;
	nBytes = (uint32_t) fe->informationLength ;

	for( ad = (long_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr); nBytes > 0; ad++ ) {
	    if( nBytes > maxVarPktSize ) {
		ad->extLength = maxVarPktSize;
		nBytes -= maxVarPktSize;
	    } else {
		ad->extLength = nBytes;
		nBytes = 0;
	    }
	    ad->extLocation.logicalBlockNum = loc;
	    ad->extLocation.partitionReferenceNum = pd->partitionNumber;
	    adiu = (struct allocDescImpUse*)(ad->impUse);
	    memcpy(&adiu->impUse, &fe->uniqueID, sizeof(uint32_t));
	    loc += ((ad->extLength + 2047) >> 11) + 7;
	}

	if( ((ad-1)->extLength & 2047) == 0 ) {
	    memset(ad, 0, sizeof(long_ad));
	    adiu = (struct allocDescImpUse*)(ad->impUse);
	    memcpy(&adiu->impUse, &fe->uniqueID, sizeof(uint32_t));
	    ad++;
	}

	fe->lengthAllocDescs = (uint8_t*)ad - (fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
	fe->descTag.descCRCLength = sizeof(struct fileEntry) + 
	    fe->lengthExtendedAttr + fe->lengthAllocDescs - sizeof(tag);
	fe->descTag.tagLocation = newVATentry();

	for( ;; ) {	    					/* retry loop for verify failure */
	    int		blknoFE, skipped, retries;
	    long_ad	*extent;

	    setChecksum(fe);
	    skipped = 0;
	    retries = 0;
	    lseek(fd, 0, SEEK_SET);

	    /* write FE */
	    blknoFE = vat[fe->descTag.tagLocation] = writeCDR(fe) - pd->partitionStartingLocation;
	    fid->icb.extLocation.logicalBlockNum = fe->descTag.tagLocation; 
	    fid->icb.extLocation.partitionReferenceNum = virtualPartitionNum;

	    /* write file data */
	    extent = (long_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);

	    for( blkInPkt = nBytes = 0; nBytes < fe->informationLength; extent++ ) {
		blkno = extent->extLocation.logicalBlockNum;
		if( blkno < blknoFE ) {
		    nBytes += extent->extLength;
		    skipped = 1;
		    continue;
		}
		for( i = 0; i < extent->extLength; blkno++, i += 2048 ) {
		    memset(p, 0, 2048);
		    if( skipped ) {
			lseek(fd, nBytes, SEEK_SET);
			skipped = 0;
		    }
		    nBytes += read(fd, p, 2048);
		    writeCDR(p);
		    if( ++blkInPkt == (maxVarPktSize >> 11) ) {
			syncCDR();
			blkInPkt = 0;
		    }
		}	
	    }

	    syncCDR();
	    if( verifyCDR(fe) == 0 ) 			// verify the file data
		break;

	    if( retries++ > 3 ) {
		printf("Retry count 3 exceeded\n");
		break;
	    }
	}
    } else {
	short_ad	*extent, extentFE[2];

	if( getExtents(2048, extentFE ) != 16 ) {		/* extent for File Entry */
	    printf("No space for File Entry\n");
	    close(fd);
	    free(fe);
	    free(fid);
	    return CMND_FAILED;
	}
	fe->descTag.tagLocation = extentFE[0].extPosition;
	// must mark allocated; otherwise allocated for a second time before being written
	markBlock(ALLOC, fe->descTag.tagLocation);

	fe->lengthAllocDescs = 					/* extents for the file data */
	    getExtents(fe->informationLength, (short_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr));
	if( !fe->lengthAllocDescs ) {
	    printf("No space for file\n");
	    close(fd);
	    free(fe);
	    free(fid);
	    return CMND_FAILED;
	}
	/* write FE */
	fe->descTag.descCRCLength = sizeof(struct fileEntry) + 
	    fe->lengthExtendedAttr + fe->lengthAllocDescs - sizeof(tag);
	setChecksum(fe);
	writeBlock(fe->descTag.tagLocation, pd->partitionNumber, fe);
	fid->icb.extLocation.logicalBlockNum = fe->descTag.tagLocation;
	fid->icb.extLocation.partitionReferenceNum = pd->partitionNumber;

	/* write file data */
	extent = (short_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
	for( nBytes = 0; nBytes < fe->informationLength; extent++ ) {
	    blkno = extent->extPosition;
	    for( i = 0; i < extent->extLength; blkno++, i += 2048 ) {
		memset(p, 0, 2048);
		nBytes += read(fd, p, 2048);
		writeBlock(blkno, pd->partitionNumber, p);
	    }	
	}
    }

    if( devicetype == DISK_IMAGE && medium == CDR )
	writeHDlink();

    adiu = (struct allocDescImpUse*)(fid->icb.impUse);
    memcpy(&adiu->impUse, &fe->uniqueID, sizeof(uint32_t));
    insertFileIdentDesc(dir, fid);

    lvidiu = (struct logicalVolIntegrityDescImpUse*)
	(lvid->data + 2 * sizeof(uint32_t) * lvid->numOfPartitions);
    lvidiu->numFiles++;

    close(fd);
    free(fe);
    free(fid);
    return CMND_OK;
}


int
copyDirectory(Directory *dir, char* name) 
{
    DIR		*srcDir;
    Directory	*workDir;
    struct dirent *dirEnt;
    struct stat dirEntStat;
    struct fileIdentDesc *fid;

    if( !(srcDir = opendir(name)) ) {
	printf("Open dir '%s': %s\n", name, strerror(errno));
	return CMND_FAILED;
    }

    if( chdir(name) != 0 ) {
	printf("Change dir '%s': %s\n", name, strerror(errno));
	closedir(srcDir);
	return CMND_FAILED;
    }

    printf("Now in %s\n", getcwd(NULL, 0));
    workDir = dir;

    while( (dirEnt = readdir(srcDir)) ) {
	if( !strcmp(dirEnt->d_name, ".") || !strcmp(dirEnt->d_name, "..") )
	    continue;
	
	if( lstat(dirEnt->d_name, &dirEntStat) != 0 ) {		// do not follow links
	    printf("Stat dirEnt '%s' failed: %s\n", dirEnt->d_name, strerror(errno));
	    continue;
	}

	if( S_ISDIR(dirEntStat.st_mode) ) {
	    if( !(options & OPT_RECURSIVE) ) {
		printf("Not recursive. Ignoring '%s' directory\n", dirEnt->d_name);
		continue;
	    }
	    fid = findFileIdentDesc(dir, dirEnt->d_name);

	    if( fid  && !(fid->fileCharacteristics & FID_FILE_CHAR_DIRECTORY ) ) {
		printf("'%s' exists but is not a directory\n", dirEnt->d_name);
		continue;
	    }
	    if( fid && (fid->fileCharacteristics & FID_FILE_CHAR_DELETED) ) {
		removeFID(dir, fid);
		fid = NULL;
	    }
	    if( !fid )
		workDir = makeDir(dir, dirEnt->d_name);
	    else
		workDir = readDirectory(dir, &fid->icb, dirEnt->d_name);
	    copyDirectory(workDir, dirEnt->d_name);
	} else {
	    if( S_ISREG(dirEntStat.st_mode) )
		copyFile(workDir, dirEnt->d_name, dirEnt->d_name, &dirEntStat);
	}
    }

    if( chdir("..") != 0 )
	printf("Change dir '..': %s\n", strerror(errno));

    closedir(srcDir);
    return CMND_OK;
}


/*	deleteDirectory()
 *
 *	Recursing into subdirectories.
 *	If anywhere no permission, query deletion but proceed if 'yes'
 *	Returns 0 if fid freed, 1 if fid not empty
 */
int 
deleteDirectory(Directory *dir, struct fileIdentDesc* fid) 
{
    uint64_t		i;
    int			rv, notEmpty;
    char		*name;
    struct fileIdentDesc *childFid;
    Directory		*childDir;
    struct fileEntry	*fe;
    
    rv = 0;

    if( fid->fileCharacteristics & FID_FILE_CHAR_DELETED ) 
	return CMND_OK;

    if( (fid->fileCharacteristics & FID_FILE_CHAR_DIRECTORY) == 0 ) 
	return deleteFID(dir, fid);			/* delete regular files */

    name = malloc(fid->lengthFileIdent + 1);
    strncpy(name, (char *)(fid->impUseAndFileIdent + fid->lengthOfImpUse), fid->lengthFileIdent);
    name[fid->lengthFileIdent] = 0;
    readDirectory(dir, &fid->icb, name);
    childDir = dir->child;

    /* check permission */

    notEmpty = 0;
    fe = (struct fileEntry *)childDir->fe;

    for( i = 0; i < fe->informationLength;
	 i += ((sizeof(struct fileIdentDesc) + childFid->lengthOfImpUse + childFid->lengthFileIdent + 3) & ~3) ) 
    {
	childFid = (struct fileIdentDesc*)(childDir->data + i);
	if( childFid->fileCharacteristics & (FID_FILE_CHAR_DELETED | FID_FILE_CHAR_PARENT) ) 
	    continue;
	if( childFid->fileCharacteristics & FID_FILE_CHAR_DIRECTORY )
	    deleteDirectory( childDir, childFid);
	else
	    deleteFID(childDir, childFid);
    }

    if( directoryIsEmpty(childDir) ) {
	rv |= deleteFID(dir, fid);
    } else {
	childDir->dirDirty = 1;
	updateDirectory(childDir);
	rv = notEmpty;
    }
    free(name);
    return rv;
}


/*	readDirectory()
 *	All fileIdentDesc's are put into the data area of the Directory structure.
 *	irrespective whether they were embedded or separately in allocated extents.
 *	updateDirectory() will embed or write in separate extents as appropriate.
 */
Directory * 
readDirectory(Directory *parentDir, long_ad* icb, char *name) 
{
    char	*p;
    uint32_t	len;
    Directory	*dir;
    struct fileEntry	*fe;

    if( parentDir == NULL ) 
	dir = rootDir;
    else {
	dir = parentDir->child;
	if( dir == NULL ) {
	    parentDir->child = dir = (Directory*)malloc(sizeof(Directory));
	    memset(dir, 0, sizeof(Directory));
	    dir->parent = parentDir;
	    dir->dataSize = 4096;
	    dir->data = malloc(4096);
	    if( dir->name ) free(dir->name);
	    dir->name = malloc(strlen(name) + 1);
	    strcpy(dir->name, name);
	} else 
	    if( dir->icb.extLocation.logicalBlockNum == icb->extLocation.logicalBlockNum
		&& dir->icb.extLocation.partitionReferenceNum == icb->extLocation.partitionReferenceNum )
		return dir;		// already the one requested
    }

    if( dir->dirDirty )
	updateDirectory(dir);

    dir->icb = *icb;
    if( name[0] ) {
	if( dir->name ) free(dir->name);
	dir->name = malloc(strlen(name) + 1);
	strcpy(dir->name, name);
    }
    p = readTaggedBlock(icb->extLocation.logicalBlockNum, icb->extLocation.partitionReferenceNum);
    memcpy(dir->fe, p, 2048);
    fe = (struct fileEntry *)dir->fe;

    if( (fe->icbTag.flags & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_IN_ICB ) {
	memcpy(dir->data, fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr, fe->lengthAllocDescs);
	memset(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr, 0, fe->lengthAllocDescs);
    } else {
	if( fe->informationLength > dir->dataSize ) {
	    len = (fe->informationLength + 2047) & ~2047;
	    dir->data = realloc(dir->data, len);
	    if( !dir->data ) {
		printf("Realloc directory data failed\n");
		return NULL;
	    }
	    dir->dataSize = len;
	}
	readExtents(dir->data, (fe->icbTag.flags & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT,
	    fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
	if( medium == CDRW ) {
	    if( (fe->icbTag.flags & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_SHORT )
		freeShortExtents((short_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr));
	    else if( (fe->icbTag.flags & ICBTAG_FLAG_AD_MASK) == ICBTAG_FLAG_AD_LONG )
		freeLongExtents((long_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr));
	}
    }
    printf("Read dir %s\n", dir->name);
    return dir;
}


/*	updateDirectory()
 *	Extents and ICB AllocMask are unchanged since reading the directory from CD.
 *	Based on current informationLength will have to decide embedding or on output extents required
 *	and free any superfluous extents
 */
int 
updateDirectory(Directory* dir) 
{
    uint64_t i;
    struct fileIdentDesc *fid;
    struct fileEntry *fe;

    if( dir->child )
	updateDirectory(dir->child);

    if( !dir->dirDirty )
	return CMND_OK;

    fe = (struct fileEntry *)dir->fe;

    if( sizeof(struct fileEntry) + fe->lengthExtendedAttr + fe->informationLength <= 2048 ) {			
        /* fileIdentDescs embedded in directory ICB */
	fe->logicalBlocksRecorded = 0;
	fe->icbTag.flags = (fe->icbTag.flags & ~ICBTAG_FLAG_AD_MASK) | ICBTAG_FLAG_AD_IN_ICB;
	fe->lengthAllocDescs = fe->informationLength;
	memcpy(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr, dir->data, fe->lengthAllocDescs);

	/* UDF2.00 2.2.1.3  - For a structure in virtual space, tagLocation is the virtual location */
	for( i = 0; i < fe->informationLength;
	     i += (sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3 ) 
	{
	    fid = (struct fileIdentDesc*) (fe->extendedAttrAndAllocDescs + i);
	    fid->descTag.tagLocation = dir->icb.extLocation.logicalBlockNum;
	    setChecksum(&fid->descTag);
	}
    } else {
	/* get new extents for the directory data */
	fe->logicalBlocksRecorded = ((fe->informationLength + 2047) & ~2047) >> 11;

	if( medium == CDR ) {
	    long_ad *ad;
	    struct allocDescImpUse *adiu;
	    uint32_t	blkno;

	    fe->icbTag.flags = (fe->icbTag.flags & ~ ICBTAG_FLAG_AD_MASK) | ICBTAG_FLAG_AD_LONG;
	    ad = (long_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
	    ad->extLength = fe->informationLength;
	    ad->extLocation.logicalBlockNum  =  blkno = getNWA() + 1 - pd->partitionStartingLocation;
	    ad->extLocation.partitionReferenceNum = pd->partitionNumber;
	    adiu = (struct allocDescImpUse*)(ad->impUse);
	    memcpy(&adiu->impUse, &fe->uniqueID, sizeof(uint32_t));
	    memset(ad + 1, 0, sizeof(long_ad));			/* necessary only if infolength multiple of 2048 */
	    fe->lengthAllocDescs = 2 * sizeof(long_ad);

	    /* set tagLocation in all FIDs */
	    for( i = 0; i < fe->informationLength;
		 i += (sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3 ) 
	    {
		fid = (struct fileIdentDesc*) (dir->data + i);
		fid->descTag.tagLocation = blkno + (i >> 11);
		fid->descTag.descCRCLength = 
		    ((sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3) - sizeof(tag);
		setChecksum(fid);
	    }

	} else {
	    uint32_t	*blocks, blkno, len;
	    short_ad	*extent;

	    extent =(short_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr);
	    fe->lengthAllocDescs = getExtents(fe->informationLength, extent);
	    fe->icbTag.flags = (fe->icbTag.flags & ~ICBTAG_FLAG_AD_MASK) | ICBTAG_FLAG_AD_SHORT;

	    /* find which blocks are to going be used to set tagLocations */
	    blocks = (uint32_t*)malloc(fe->logicalBlocksRecorded * sizeof(uint32_t));
	    blkno = extent->extPosition;
	    len = extent->extLength;
	    for( i = 0; i < fe->logicalBlocksRecorded; i++ ) {
		blocks[i] = blkno;
		if( len <= 2048 ) {
		    extent++;
		    blkno = extent->extPosition;
		    len = extent->extLength;
		} else {
		    len -= 2048;
		    blkno++;
		}
	    }
	    /* set tagLocation */
	    for( i = 0; i < fe->informationLength;
		 i += (sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3 ) 
	    {
		fid = (struct fileIdentDesc*) (dir->data + i);
		fid->descTag.tagLocation = blocks[i >> 11];
		fid->descTag.descCRCLength = 
		    ((sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3) - sizeof(tag);
		setChecksum(fid);
	    }
	    free(blocks);
	}
    }

    fe->descTag.descCRCLength = 
	sizeof(struct fileEntry) + fe->lengthExtendedAttr + fe->lengthAllocDescs - sizeof(tag);
    setChecksum(&dir->fe);

    if( medium == CDRW ) {
	/* write the directory fileEntry */
	writeBlock(dir->icb.extLocation.logicalBlockNum, dir->icb.extLocation.partitionReferenceNum, &dir->fe);

	if( fe->logicalBlocksRecorded  ) {
	    /* write any directory data */
	    writeExtents(dir->data, 1, (short_ad*)(fe->extendedAttrAndAllocDescs + fe->lengthExtendedAttr));
	}
    } else {		/* medium == CDR */
	int retries;

	retries = 0;

	for( ;; ) {					/* loop only when verify failed */
	    uint32_t	pbn;

	    /* write the directory fileEntry */
	    pbn = writeCDR(&dir->fe);
	    vat[((long_ad*)&dir->icb)->extLocation.logicalBlockNum] = pbn - pd->partitionStartingLocation;

	    if( fe->logicalBlocksRecorded )
		for( i = 0; i < fe->informationLength; i += 2048 )
		    writeCDR(dir->data + i);

	    if( verifyCDR((struct fileEntry *)dir->fe) == 0 )
		break;

	    if( ++retries > 3 ) {
		printf("updateDirectory: '%s' failed\n", dir->name);
		return CMND_FAILED;
	    }
	}
    }
    dir->dirDirty = 0;
    printf("Wrote dir %s\n", dir->name);
    return CMND_OK;
}


/*	Create subdirectory called 'name' in 'dir' 
 */
Directory * 
makeDir(Directory *dir, char* name ) 
{
    Directory		*newDir;
    struct fileEntry	*fe;
    struct fileEntry	*dirfe;
    struct fileIdentDesc *backFid, *forwFid;
    struct allocDescImpUse *adiu;
    struct logicalVolIntegrityDescImpUse *lvidiu;
    short_ad		allocDescs[2];


    /* back reference to parent in new directory */
    backFid = makeFileIdentDesc("");
    backFid->icb = dir->icb;
    backFid->fileCharacteristics = FID_FILE_CHAR_DIRECTORY | FID_FILE_CHAR_PARENT;
    backFid->descTag.descCRCLength = ((sizeof(struct fileIdentDesc) + 3) & ~3) - sizeof(tag);

    fe = makeFileEntry();
    fe->icbTag.fileType = ICBTAG_FILE_TYPE_DIRECTORY;
    fe->icbTag.flags = ICBTAG_FLAG_AD_IN_ICB;
    fe->informationLength = (sizeof(struct fileIdentDesc) + 3) & ~3;

    /* forward reference to new directory in parent directory */
    forwFid = makeFileIdentDesc(name);
    forwFid->fileCharacteristics = FID_FILE_CHAR_DIRECTORY;
    adiu = (struct allocDescImpUse*)forwFid->icb.impUse;
    memcpy(&adiu->impUse, &fe->uniqueID, sizeof(uint32_t));

    if( medium == CDR ) {
	fe->descTag.tagLocation = newVATentry();
	forwFid->icb.extLocation.logicalBlockNum = fe->descTag.tagLocation;
	forwFid->icb.extLocation.partitionReferenceNum = virtualPartitionNum;
    } else {
	if(  getExtents( 2048, allocDescs) != 16 )
	    fail("makeDir: Could not get File Entry extent\n");

	markBlock(ALLOC, allocDescs[0].extPosition);
	fe->descTag.tagLocation = allocDescs[0].extPosition;
	backFid->descTag.tagLocation = allocDescs[0].extPosition;
	forwFid->icb.extLocation.logicalBlockNum = allocDescs[0].extPosition;
	forwFid->icb.extLocation.partitionReferenceNum = pd->partitionNumber;
	setChecksum(&backFid->descTag);
	fe->descTag.descCRCLength = sizeof(struct fileEntry) + fe->lengthAllocDescs - sizeof(tag);
	setChecksum(fe);
    }

    insertFileIdentDesc(dir, forwFid);
    dirfe = (struct fileEntry *)dir->fe;
    dirfe->fileLinkCount++;
    dir->dirDirty = 1;

    /* setup directory structure for new directory */
    newDir = dir->child;

    if( newDir != NULL ) 
	updateDirectory(newDir);
    else {
	newDir = (Directory*)malloc(sizeof(Directory));
	memset(newDir, 0, sizeof(Directory));
	newDir->parent = dir;
	newDir->dataSize = 4096;
	newDir->data = malloc(4096);
	dir->child = newDir;
    }
    
    memset(newDir->data, 0, newDir->dataSize);
    newDir->name = malloc(strlen(name)+1);
    strcpy(newDir->name, name);
    newDir->icb = forwFid->icb;
    memcpy(&newDir->fe, fe, 2048);
    memcpy(newDir->data, backFid, fe->informationLength);
    newDir->dirDirty = 1;

    lvidiu = (struct logicalVolIntegrityDescImpUse*)
	(lvid->data + 2 * sizeof(uint32_t) * lvid->numOfPartitions);
    lvidiu->numDirs++;

    free(backFid);
    free(forwFid);
    free(fe);
    return newDir;
}


/*	analyzeDest()
 *
 *	The last argument in a command is the destination for copying or the object
 *	to act on. The argument consists of / separated components 
 *	eg. /src/test/udf/wrudf.h or /src/test/iso/
 *	or just a single component eg. . or / or wrudf.c
 *
 *	All but the last component must be existing directories and curDir points to
 *	the last directopry in that chain, eg. udf, otherwise result DIR_INVALID.
 *
 *	When the last component does not exist, return result DOES_NOT_EXIST.
 *	name --> last component eg. wrudf.h or iso
 *
 *	If the last component identifies a directory return EXISTING_DIR or DELETED_DIR,
 *	*fid = NULL, *name = last component name eg. iso.
 *
 *	If the last component is a file  return EXISTING_FILE or DELETED_FILE.
 *	*fid points to FID entry in curDir with name equal the last component.
 *	*name --> last component name eg. wrudf.h
 */
enum RV 
analyzeDest(char* arg, struct fileIdentDesc** fid, char** name) 
{
    int		len;
    char 	*comp, *endComp;

    if( arg[0] == '/' ) {
	curDir = rootDir;
	comp = arg + 1;
    } else {
	comp = arg;
    }

    len = strlen(comp);
    if( len > 1 && comp[len-1] == '/' )
	comp[len-1] = 0;				/* remove any trailing slash */

    for(  ; ( endComp = strchr(comp, '/') ) != NULL; comp = ++endComp ) {
	*endComp = 0;

	if( strcmp(comp, ".") == 0 )
	    continue;

	if( strcmp(comp, "..") == 0 ) {
	    if( !curDir->parent )
		return DIR_INVALID;
	    curDir = curDir->parent;
	    continue;
	}
	
	*fid = findFileIdentDesc(curDir, comp);
	if( *fid == NULL )
	    return DIR_INVALID;
	if( ! ((*fid)->fileCharacteristics & FID_FILE_CHAR_DIRECTORY ))
	    return DIR_INVALID;
	if( (*fid)->fileCharacteristics & FID_FILE_CHAR_DELETED )
	    return DIR_INVALID;

//	curDir = readDirectory(curDir->child, &(*fid)->icb, comp); 
	curDir = readDirectory(curDir, &(*fid)->icb, comp); 
    }

    // final component
    *name = comp;

    if( comp[0] == 0 || strcmp(comp, ".") == 0 )
	return EXISTING_DIR;

    if( strcmp(comp, "..") == 0 ) {
	if( !curDir->parent )
	    return DIR_INVALID;
	curDir = curDir->parent;
	return EXISTING_DIR;
    }

    *fid = findFileIdentDesc(curDir, comp);

    if( *fid == NULL )
	return DOES_NOT_EXIST;

    if( (*fid)->fileCharacteristics & FID_FILE_CHAR_DIRECTORY ) {
	if( (*fid)->fileCharacteristics & FID_FILE_CHAR_DELETED )
	    return DELETED_DIR;
	else {
	    curDir = readDirectory(curDir, &(*fid)->icb, comp); 
	    return EXISTING_DIR;
	}
    } else {
	if( (*fid)->fileCharacteristics & FID_FILE_CHAR_DELETED )
	    return DELETED_FILE;
    }
    return EXISTING_FILE;    
}


int 
cpCommand(void) 
{
    int		i, rv;
    enum RV	state;
    char	*srcname, *name, *p;
    Directory	*cpyDir;
    struct fileIdentDesc *fid, *newFid;
    struct stat fileStat;

    if( cmndc < 2 )
	return WRONG_NO_ARGS;

    name = NULL;
    state = analyzeDest(cmndv[cmndc-1], &fid, &name);

    if( state == DIR_INVALID )
	return DIR_INVALID;

    if( state == DELETED_DIR ) {
	printf("Cannot reuse deleted directory name yet\n");
	return CMND_FAILED;
    }
    
    if( state == DELETED_FILE ) {
	removeFID(curDir, fid);
	state = DOES_NOT_EXIST;
    }
	
    if( state == EXISTING_FILE ) {
	if( questionOverwrite(curDir, fid, name) )
	    return CMND_FAILED;
	state = DOES_NOT_EXIST;
    }

    /* do I have write permission for the destination directory ? */

    /* process source arguments */
    for( i = 0; i < cmndc - 1; i++ ) {
	p = strrchr(cmndv[i], '/');

	if( p && p > cmndv[i] && *(p+1) == 0 ) *p = 0;	// remove any trailing slash (except in "/")

	p = strrchr(cmndv[i], '/');

	if( p ) {
	    srcname = p+1;
	} else
	    srcname = cmndv[i];

	if( (rv = lstat(cmndv[i], &fileStat)) < 0 ) {	// do not follow soft links
	    printf("stat failed on %s\n", cmndv[i]);
	    continue;
	}

	if( S_ISDIR(fileStat.st_mode) ) {
	    if( state == EXISTING_DIR ) {
		newFid = findFileIdentDesc(curDir, srcname);
		if( newFid == NULL ) {
		    cpyDir = makeDir(curDir, srcname);
		    newFid = findFileIdentDesc(curDir, srcname);
		} else {
		    if( newFid->fileCharacteristics != FID_FILE_CHAR_DIRECTORY ) {
			printf("Destination is not a directory\n");
			return CMND_FAILED;
		    }
		    cpyDir = readDirectory(curDir, &newFid->icb, srcname);
		}
		if( chdir(cmndv[i]) != 0 )
			printf("Change dir '%s': %s\n", cmndv[i], strerror(errno));
		else
			copyDirectory(cpyDir, ".");
	    } else 
		printf("Destination is not a directory\n");

	    if( chdir(hdWorkingDir) != 0 ) {
		printf("Change dir '%s': %s\n", hdWorkingDir, strerror(errno));
		return CMND_FAILED;
	    }
	    continue;
	}

	if( !S_ISREG(fileStat.st_mode) ) {
	    printf("Can only copy regular files or directories\n");
	    return CMND_FAILED;
	}

	if( state == DOES_NOT_EXIST || state == EXISTING_DIR )
	    name = srcname;

	copyFile(curDir, cmndv[i], name, &fileStat);
    }
    return CMND_OK;
}


int 
rmCommand(void) 
{
    enum RV		state;
    int			i;
    struct fileIdentDesc *fid;
    char*	name;

    if( cmndc < 1 )
	return CMND_ARG_INVALID;

    for( i = 0; i < cmndc; i++ ) {
	state = analyzeDest(cmndv[i], &fid, &name);

	if( state == EXISTING_FILE ) {
	    deleteFID(curDir, fid);
	    continue;
	}

	if( state != EXISTING_DIR  )
	    return state;

	if( options & OPT_RECURSIVE ) {
	    deleteDirectory(curDir, fid);
	    continue;
	} else
	    printf("Cannot delete directory '%s' without -r option\n", cmndv[i]);
    }
    return CMND_OK;
}


int
mkdirCommand(void) 
{
    int		rv;
    char	*name;
    struct fileIdentDesc *fid;
    Directory 	*newDir;

    if( cmndc != 1 )
	return WRONG_NO_ARGS;

    rv = analyzeDest(cmndv[0], &fid, &name);

    if( rv == DIR_INVALID )
	return DIR_INVALID;

    if( rv == EXISTING_FILE || rv == EXISTING_DIR ) {
	if( fid->fileCharacteristics & FID_FILE_CHAR_DELETED ) {
	    removeFID(curDir, fid);
	} else 
	    return rv;
    }

    newDir = makeDir(curDir, name);

    if( !newDir )
	return CMND_FAILED;

    curDir = newDir;
    return CMND_OK;
}


int
rmdirCommand(void) 
{
    int		state;
    struct fileIdentDesc *fid;
    char*	name;

    if( cmndc != 1 )
	return WRONG_NO_ARGS;

    state = analyzeDest(cmndv[0], &fid, &name);

    if( state != EXISTING_DIR )
	return state;

    if( !directoryIsEmpty(curDir) )
	return DIR_NOT_EMPTY;

    if( !curDir->parent ) {
	printf("Cannot remove root directory\n");
	return CMND_FAILED;
    }
    return deleteFID(curDir, fid);
}

int cdcCommand() {
    int		state;
    struct fileIdentDesc *fid;
    char*	name;
    
    if( cmndc != 1 )
	return WRONG_NO_ARGS;

    state = analyzeDest(cmndv[0], &fid, &name);

    if( state != EXISTING_DIR )
	return state;

    return CMND_OK;
}

int lscCommand(void) {
    struct fileIdentDesc *fid;
    struct fileEntry *fe;
    struct fileEntry *curfe;
    char	*name, filename[512];
    uint64_t	i;
    int		state;

    if( cmndc > 1 )
	return WRONG_NO_ARGS;

    if( cmndc == 1 ) {
	state = analyzeDest(cmndv[0], &fid, &name);

	if( state != EXISTING_DIR )
	    return state;
    }

    curfe = (struct fileEntry *)curDir->fe;

    for( i = 0; i < curfe->informationLength;
	 i += ((sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3) ) 
    {
	fid = (struct fileIdentDesc*)(curDir->data + i);
	if( fid->fileCharacteristics & FID_FILE_CHAR_DELETED  ) 
	    continue;

	if( fid->fileCharacteristics & FID_FILE_CHAR_PARENT )
	    strcpy(filename, "..");
	else
	    decode_locale((dchars *)(fid->impUseAndFileIdent + fid->lengthOfImpUse), filename, fid->lengthFileIdent, sizeof(filename));

	fe = readTaggedBlock( fid->icb.extLocation.logicalBlockNum, fid->icb.extLocation.partitionReferenceNum);

	printf("%s %6d:%c%c%c%c%c %6d:%c%c%c%c%c other:%c%c%c%c%c links:%2d info:%12d %s\n",
	    fe->icbTag.fileType == ICBTAG_FILE_TYPE_DIRECTORY ? "DIR" : "   ", 
	    fe->uid, 
	    fe->permissions & FE_PERM_U_READ ? 'r' : '-',
	    fe->permissions & FE_PERM_U_WRITE ? 'w' : '-',
	    fe->permissions & FE_PERM_U_EXEC ? 'x' : '-',
	    fe->permissions & FE_PERM_U_CHATTR ? 'a' : '-',
	    fe->permissions & FE_PERM_U_DELETE ? 'd' : '-',
	    fe->gid,
	    fe->permissions & FE_PERM_G_READ ? 'r' : '-',
	    fe->permissions & FE_PERM_G_WRITE ? 'w' : '-',
	    fe->permissions & FE_PERM_G_EXEC ? 'x' : '-',
	    fe->permissions & FE_PERM_G_CHATTR ? 'a' : '-',
	    fe->permissions & FE_PERM_G_DELETE ? 'd' : '-',
	    fe->permissions & FE_PERM_O_READ ? 'r' : '-',
	    fe->permissions & FE_PERM_O_WRITE ? 'w' : '-',
	    fe->permissions & FE_PERM_O_EXEC ? 'x' : '-',
	    fe->permissions & FE_PERM_O_CHATTR ? 'a' : '-',
	    fe->permissions & FE_PERM_O_DELETE ? 'd' : '-',
	    fe->fileLinkCount, (uint32_t)fe->informationLength, filename);
    }
    return CMND_OK;
}

int cdhCommand() 
{
    if( cmndc > 1 )
	return WRONG_NO_ARGS;

    if( chdir(cmndv[0]) != 0 ) {
	printf("Change dir '%s': %s\n", cmndv[0], strerror(errno));
	return CMND_FAILED;
    }

    if( hdWorkingDir ) free(hdWorkingDir);
    hdWorkingDir = getcwd(NULL, 0);
    printf("Harddisk working directory set to %s\n", cmndv[0]);
    return CMND_OK;
}


int lshCommand() {
    char	cmnd[128];

    if( cmndc > 1 )
	return WRONG_NO_ARGS;

    strcpy(cmnd, "ls -l ");

    if( cmndc == 1 )
	strncat(cmnd, cmndv[0], sizeof(cmnd)-strlen("ls -l ")-1);

    if( system(cmnd) != 0 )
	return CMND_FAILED;

    return CMND_OK;
}



/*	directoryIsEmpty()
 *	Return 1 if a directory contains only a parent dir entry and, possibly 0, deleted entries;
 *	Otherwise return 0;
 */
int
directoryIsEmpty(Directory *dir) 
{
    uint64_t i;
    struct fileIdentDesc *fid;
    struct fileEntry *fe;

    fe = (struct fileEntry *)dir->fe;

    for( i = 0; i < fe->informationLength;
	 i += ((sizeof(struct fileIdentDesc) + fid->lengthOfImpUse + fid->lengthFileIdent + 3) & ~3) ) 
    {
	fid = (struct fileIdentDesc*)(dir->data + i);
	if( fid->fileCharacteristics & (FID_FILE_CHAR_DELETED | FID_FILE_CHAR_PARENT) ) 
	    continue;
	return 0;
    }
    return 1;
}


/*	questionOverwrite()
 *	Query whether to overwrite existing file.
 *	Remove the FID entry from the directory if reply was 'y' and return 0.
 *	Return 1 if don't overwrite.
 */
int
questionOverwrite(Directory *dir, struct fileIdentDesc *fid, char* name)
{
    printf("File %s already exists. Overwrite ? (y/N) : ", name);
#ifdef USE_READLINE
    readLine(NULL);
#else
    if (!fgets(line, 256, stdin))
	line[0] = 0;
#endif
#ifdef USE_READLINE
    if( !line )
	return 1;
#endif
    if( line[0] != 'y' )
	return 1;
    deleteFID(dir, fid);
    return 0;
}


