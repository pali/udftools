/*
 * taglist.c
 *
 * PURPOSE
 *	A simple utility to dump disk sectors tags.
 *
 * DESCRIPTION
 *	I got tired of using dd and hexdump :-)
 *
 *	Usage: taglist device start_sector end_sector| less
 *	The sector can be specified in decimal, octal, or hex.
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * HISTORY
 * 10/1/98 dgb			written and tinkered with
 * 11/23/98 Ben Fennema		added much debug info
 * 11/26/98 dgb			added -v option
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include <linux/udf_fs.h>

Uint8 sector[2048];

struct VolDesc
{
	tag descTag;
	Uint32 volDescSeqNum;
	Uint8 reserved[488];
};

int main(int argc, char **argv)
{
	int fd, retval;
	unsigned long sec = 0;
	unsigned long endsec = 0;
	struct VolDesc *vd;
	struct VolStructDesc *vs;
	int partstart = 0;
	int verbose=0;

	if (argc < 4) {
		printf("usage: taglist <device> <start_sector> <end_sector>\n");
		return -1;
	}
	if ( strcmp(argv[1], "-v") == 0 ) {
		verbose++;
		argv++; argc--;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return -1;
	}

	sec = strtoul(argv[2], NULL, 0);
	endsec = strtoul(argv[3], NULL, 0);

	retval = lseek(fd, sec << 11, SEEK_SET);
	if (retval < 0) {
		fprintf(stderr, "error seeking to %lu\n", sec << 11);
		return -1;
	}

	retval = read(fd, sector, 2048);
	while ((retval > 0) && (sec <= endsec)) {
		fflush(stdout);
		/* check for ISO structures */
		vs = (struct VolStructDesc *)sector;
		if (!strncmp(vs->stdIdent, STD_ID_BEA01, STD_ID_LEN) ||
		    !strncmp(vs->stdIdent, STD_ID_BOOT2, STD_ID_LEN) ||
		    !strncmp(vs->stdIdent, STD_ID_CD001, STD_ID_LEN) ||
		    !strncmp(vs->stdIdent, STD_ID_CDW02, STD_ID_LEN) ||
		    !strncmp(vs->stdIdent, STD_ID_NSR02, STD_ID_LEN) ||
		    !strncmp(vs->stdIdent, STD_ID_TEA01, STD_ID_LEN))
		{
		    char ident[STD_ID_LEN+1];
		    strncpy(ident, vs->stdIdent, STD_ID_LEN);
		    ident[STD_ID_LEN] = '\0';
		    printf("%8lu: [ISO] %s\n", sec, ident);
		}

		/* check for UDF structures */
		vd = (struct VolDesc *)sector;
		if ( (vd->descTag.tagIdent>0) && (vd->descTag.tagIdent <= 0x109) )
		{
#if 0
		    if (1 || vd->descTag.tagLocation == sec || vd->descTag.tagLocation == sec - partstart)
#endif
		    {
			if ( verbose ) 
				printf("%8lu /%8u: 0x%04x - ", 
					sec, 
					vd->descTag.tagLocation, 
					vd->descTag.tagIdent);
			else
				printf("%8lu: 0x%04x - ", 
					sec, 
					vd->descTag.tagIdent);
			switch (vd->descTag.tagIdent) {
			    case 1:  printf("PrimaryVolDesc\n"); break;
			    case 2:
		   	    {
				struct AnchorVolDescPtr *avdp;
				avdp = (struct AnchorVolDescPtr *)sector;
				printf("AnchorVolDescPtr\n");
				printf("\tMain -> %8u - %8u\n",
					avdp->mainVolDescSeqExt.extLocation,
					avdp->mainVolDescSeqExt.extLocation +
					avdp->mainVolDescSeqExt.extLength / 2048);
				printf("\tRes  -> %8u - %8u\n",
					avdp->reserveVolDescSeqExt.extLocation,
					avdp->reserveVolDescSeqExt.extLocation +
					avdp->reserveVolDescSeqExt.extLength / 2048);
				break;
			    }
			    case 3:  printf("VolDescPtr\n"); break;
			    case 4:  printf("ImpUseVolDesc\n"); break;
			    case 5:
			    {
				struct PartitionDesc *pd;
				pd = (struct PartitionDesc *)sector;
				partstart = pd->partitionStartingLocation;
				printf("PartitionDesc\n");
				printf("\tPartition sector: %d length: %d (in blocks) access: %d\n", 
					partstart, pd->partitionLength, pd->accessType);
				break;
			    }
			    case 6:  
			    {
				struct LogicalVolDesc *p;
				long_ad * la;
				p=(struct LogicalVolDesc *)sector;
				la=(long_ad *)p->logicalVolContentsUse;
			    	printf("LogicalVolDesc\n"); 
				printf("\tFileSetDesc block: %lu extLen: %u\n",
				 	(long unsigned)la->extLocation.logicalBlockNum,
					la->extLength);
				break;
			    }
			    case 7:  printf("UnallocatedSpaceDesc\n"); break;
			    case 8:  printf("TerminatingDesc\n"); break;
			    case 9:  
			    {
				struct LogicalVolIntegrityDescImpUse *iu;
				struct LogicalVolIntegrityDesc *lvd;
				dstring * p;

				printf("LogicalVolIntegrityDesc\n"); 

				lvd=(struct LogicalVolIntegrityDesc *)sector;
				printf("\tType: %x lenImpUse: %u ",
					lvd->integrityType, lvd->lengthOfImpUse);

				if ( lvd->lengthOfImpUse ) {
					p=(dstring *)lvd->impUse;
					p += lvd->numOfPartitions * 8;
					iu=(struct LogicalVolIntegrityDescImpUse *)p;
					printf("Files: %u Dirs: %u: minR: %04x minW: %04x maxW: %04x\n",
						iu->numFiles, iu->numDirs, 
						iu->minUDFReadRev, iu->minUDFWriteRev, iu->maxUDFWriteRev);
				}
				break;
			    }
			    case 0x100:
			    {
				struct FileSetDesc *fsd;
				fsd = (struct FileSetDesc *)sector;
				printf("FileSetDesc\n");
				if ( verbose ) {
				    printf("\tCSL: %x\n", fsd->charSetList);
				    printf("\tFSN: %d\n", fsd->fileSetNum);
				    printf("\tFSD: %d\n", fsd->fileSetDescNum);
				    printf("\tICB: LEN: %d\n", fsd->rootDirectoryICB.extLength);
				} else {
				    printf("\tRootDir: (%d,%d) ", 
			fsd->rootDirectoryICB.extLocation.logicalBlockNum,
			fsd->rootDirectoryICB.extLength);
				    printf("nextExt: (%d,%d) ", 
			fsd->nextExt.extLocation.logicalBlockNum,
			fsd->nextExt.extLength);
				    printf("streamDir: (%d,%d)\n", 
			fsd->streamDirectoryICB.extLocation.logicalBlockNum,
			fsd->streamDirectoryICB.extLength);
				}
				break;
			    }
			    case 0x101:
			    {
				struct FileIdentDesc *fid;
				int pos = 0;
				int i;
				fid = (struct FileIdentDesc *)sector;
				printf("FileIdentDesc\n");
				if ( verbose ) {
				    while (pos < 2048) {
				       fid =(struct FileIdentDesc *)&(sector[pos]);
				       printf("\tTAG: %3d POS: %4d ", fid->descTag.tagIdent, pos);
				       printf("\tVer:  %x (%d)\n", fid->descTag.descVersion, fid->descTag.descVersion);
				       printf("\tCsum: %x (%d)\n", fid->descTag.tagChecksum, fid->descTag.tagChecksum);
				       printf("\tSNum: %x (%d)\n", fid->descTag.tagSerialNum, fid->descTag.tagSerialNum);
				       printf("\tCRCL: %x (%d)\n", fid->descTag.descCRCLength, fid->descTag.descCRCLength);
				       printf("\tCRC:  %x (%d)\n", fid->descTag.descCRC, fid->descTag.descCRC);
				       printf("\tFVN: %4d ", fid->fileVersionNum);
			 	       printf("\tFCH: %3d ", fid->fileCharacteristics);
				       printf("\tLFI: %2d ", fid->lengthFileIdent);
				       printf("\tICB(ELEN: %d ", fid->icb.extLength);
				       printf("\tELOC(LBN: %d ", fid->icb.extLocation.logicalBlockNum);
				       printf("\tPRN: %d))", fid->icb.extLocation.partitionReferenceNum);
				       printf("\tLIU: %d\n", fid->lengthOfImpUse);
				       if (fid->lengthFileIdent) {
					    printf("ID: \"");
					    for (i=2; i<fid->lengthFileIdent; i+=2)
					        printf("%c", fid->fileIdent[i]);
				    	    printf("\"\n");
				       }
				       pos += sizeof(struct FileIdentDesc) + fid->lengthFileIdent + fid->lengthOfImpUse;
				       pos += (4 - (pos % 4)) % 4;
				    }
				} else {
					/* not verbose */
			 	    printf("\tfileChar: %3xh ", fid->fileCharacteristics);
				    printf("lenIdent: %2d ", fid->lengthFileIdent);
				    printf("ICB(extLen: %d ", fid->icb.extLength);
				    printf("block: %d) ", fid->icb.extLocation.logicalBlockNum);
				    printf("lenImpUse: %d ", fid->lengthOfImpUse);
				    if (fid->lengthFileIdent) {
					    printf("ID: \"");
					    for (i=2; i<fid->lengthFileIdent; i+=2)
					        printf("%c", fid->fileIdent[i]);
				    	    printf("\"\n");
				       }
				    if (fid->lengthFileIdent) {
					int i;
					printf("ID: ");
					for (i=2; i<fid->lengthFileIdent; i+=2)
					        printf("%c", fid->fileIdent[i]);
				    }
				    printf("\n");
				}
				break;
			    }
			    case 0x102:  printf("AllocExtDesc\n"); break;
			    case 0x103:  printf("IndirectEntry\n"); break;
			    case 0x104:  printf("TerminalEntry\n"); break;
			    case 0x105:
			    {
				struct FileEntry *fe;
				fe = (struct FileEntry *)sector;
				printf("FileEntry");
				if ( verbose ) {
				    printf("\n");
				    printf("PRE: %d ", fe->icbTag.priorRecordedNumDirectEntries);
				    printf("STR(TYPE: %d ", fe->icbTag.strategyType);
				    printf("PARM: %d) ", fe->icbTag.strategyParameter);
				    printf("MAE: %d ", fe->icbTag.numEntries);
				    printf("FTY: %s (%u)", 
				fe->icbTag.fileType == 4 ? "DIR" : "OTHER",
				fe->icbTag.fileType);
				    printf("ICB(LBN: %d ", fe->icbTag.parentICBLocation.logicalBlockNum);
				    printf("PRN: %d) ", fe->icbTag.parentICBLocation.partitionReferenceNum);
				    printf("ALL: %s ", (fe->icbTag.flags & 0x7) == 0 ? "short" : (fe->icbTag.flags & 0x7) == 1 ? "long" : "extended");
				    printf("FLG: %x ", fe->icbTag.flags & 0xFFF8);
				    printf("UID: %d ", fe->uid);
				    printf("GID: %d ", fe->gid);
				    printf("PER: %x ", fe->permissions);
				    printf("FLC: %d ", fe->fileLinkCount);
				    printf("RFM: %d ", fe->recordFormat);
				    printf("RDA: %d ", fe->recordDisplayAttr);
				    printf("RLN: %d ", fe->recordLength);
				    printf("ILN: %Ld ", fe->informationLength);
				    printf("LBR: %Ld ", fe->logicalBlocksRecorded);
				    printf("CHK: %d ", fe->checkpoint);
				    printf("uID: %Ld ", fe->uniqueID);
				    printf("LEA: %d ", fe->lengthExtendedAttr);
				    printf("LAD: %d\n", fe->lengthAllocDescs);
				    if (fe->icbTag.fileType == 4 || fe->icbTag.fileType == 0)
				    {
				        struct FileIdentDesc *fid;
				        long_ad *lad;
				        fid = (struct FileIdentDesc *)(&(fe->extendedAttr[0]));
				        printf("TAG: %d ", fid->descTag.tagIdent);
				        printf("Ver:  %x (%d)\n", fid->descTag.descVersion, fid->descTag.descVersion);
				        printf("Csum: %x (%d)\n", fid->descTag.tagChecksum, fid->descTag.tagChecksum);
				        printf("SNum: %x (%d)\n", fid->descTag.tagSerialNum, fid->descTag.tagSerialNum);
				        printf("CRCL: %x (%d)\n", fid->descTag.descCRCLength, fid->descTag.descCRCLength);
				        printf("CRC:  %x (%d)\n", fid->descTag.descCRC, fid->descTag.descCRC);
				        printf("FVN: %d ", fid->fileVersionNum);
				        printf("FCH: %d ", fid->fileCharacteristics);
				        printf("LFI: %d ", fid->lengthFileIdent);
				        printf("ICB(ELEN: %d ", fid->icb.extLength);
				        printf("ELOC(LBN: %d ", fid->icb.extLocation.logicalBlockNum);
				        printf("PRN: %d))", fid->icb.extLocation.partitionReferenceNum);
				        printf("LIU: %d\n", fid->lengthOfImpUse);

				        lad = (long_ad *)(&(fe->extendedAttr[0]));
				        printf("LEN: %d ", lad->extLength);
				        printf("LBN: %d PRN: %d\n", lad->extLocation.logicalBlockNum, lad->extLocation.partitionReferenceNum);
				    } /* end type 4 */	
				} else {
				    int a;
				    printf("\tuID: %Ld ", fe->uniqueID);
				    printf("Type: %Xh ", fe->icbTag.fileType);
				    printf("Len: %Ld ", fe->informationLength);
				    a=fe->icbTag.flags & 0x7;
				    switch (a) {
					case 0:
				    		printf("Alloc: SHORT "); 
						break;
					case 1:
				    		printf("Alloc: LONG "); 
						break;
					case 2:
				    		printf("Alloc: EXTENDED "); 
						break;
					case 3:
				    		printf("Alloc: IN_ICB "); 
						break;
				    }
				    printf("\n");
				} /* end verbose */
				break;
			    }
			    case 0x106:  printf("ExtendedAttrHeaderDesc\n"); break;
			    case 0x107:  printf("UnallocatedSpaceEntry\n"); break;
			    case 0x108:  printf("SpaceBitmap\n"); break;
			    case 0x109:  printf("PartitionIntegrityEntry\n"); break;
			    default: 
				printf("\n"); break;
			}
#if 0
			printf("VDSN: %x (%d)\n", vd->volDescSeqNum, vd->volDescSeqNum);
			printf("Ver:  %x (%d)\n", vd->descTag.descVersion, vd->descTag.descVersion);
			printf("Csum: %x (%d)\n", vd->descTag.tagChecksum, vd->descTag.tagChecksum);
			printf("SNum: %x (%d)\n", vd->descTag.tagSerialNum, vd->descTag.tagSerialNum);
			printf("CRCL: %x (%d)\n", vd->descTag.descCRCLength, vd->descTag.descCRCLength);
			printf("CRC:  %x (%d)\n", vd->descTag.descCRC, vd->descTag.descCRC);
#endif
		    }
		}
		retval = read(fd, sector, 2048);
		sec++;
	}
	
	close(fd);
	return 0;
}
