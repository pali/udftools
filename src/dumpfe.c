/*
 * dumpfe.c
 *
 * PURPOSE
 *	A simple utility to dump disk sectors tags.
 *
 * DESCRIPTION
 *	I got tired of using dd and hexdump :-)
 *
 *	Usage: dumpfe device start_sector | less
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

#include <linux/udf_udf.h>

Uint8 sector[2048];

struct VolDesc
{
	tag descTag;
	Uint32 volDescSeqNum;
	Uint8 reserved[488];
};

void dump_tag(tag * descTag);
void dump_icbtag(icbtag * icbTag);

void dump_tag(tag * descTag) {
	printf("Ident: %Xh ", descTag->tagIdent);
	printf("Ver:  %u ", descTag->descVersion);
	printf("Csum: %Xh ", descTag->tagChecksum);
	printf("SNum: %d ", descTag->tagSerialNum);
	printf("CRCL: %d ", descTag->descCRCLength);
	printf("CRC:  %Xh ", descTag->descCRC);
	printf("Loc: %u\n", descTag->tagLocation);
}

void dump_icbtag(icbtag * icbTag)
{
	printf("PRE: %d ", icbTag->priorRecordedNumDirectEntries);
	printf("Strat: %d ", icbTag->strategyType);
	/*printf("PARM: %d) ", icbTag->strategyParameter);*/
	printf("numE: %d ", icbTag->numEntries);
	printf("ICB(FileType: %s (%u) ", 
		icbTag->fileType == 4 ? "DIR" : "OTHER",
		icbTag->fileType);
	printf("Par(LBN: %d ", icbTag->parentICBLocation.logicalBlockNum);
	printf("PRN: %d) ", icbTag->parentICBLocation.partitionReferenceNum);
	switch (icbTag->flags & 0x7) {
		case 0: printf("Alloc: short "); break;
		case 1: printf("Alloc: long "); break;
		case 2: printf("Alloc: extended "); break;
		case 3: printf("Alloc: in_icb "); break;
	}
	printf("FLG: %x)\n", icbTag->flags & 0xFFF8);
}

void dump_hex(unsigned char * p, int len, char * prefix)
{
	int i,j;

	for (i = 0; i < len; i += 16) {
		printf("%s%03Xh: ", prefix, i);

		for (j = 0; (j < 16)&&(i+j < len); j++)
			printf((j == 7) ? "%02x-": "%02x ", p[i + j]);
		for ( ; j<16; j++)
			printf((j == 7) ? "%2s-": "%2s ", "");
			
		for (j = 0; (j < 16)&&(i+j < len); j++) {
			if ( p[i + j] < 31 ||  p[i+j] > 126)
				printf(".");
			else
				printf("%c", p[i+j]);
		}
		printf("\n");
	}
}

int main(int argc, char **argv)
{
	int fd, retval;
	unsigned long sec = 0;
	unsigned long endsec = 0;
	struct VolDesc *vd;
	struct VolStructDesc *vs;
#if 0
	int partstart = 0;
#endif
	int verbose=0;

	if (argc < 3) {
		printf("usage: dumpfe <device> <sector>\n");
		return -1;
	}
	verbose=1;
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return -1;
	}

	sec = strtoul(argv[2], NULL, 0);
	endsec = sec;

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
			printf("%8lu: 0x%04x - ", 
					sec, 
					vd->descTag.tagIdent);
			switch (vd->descTag.tagIdent) {
			    case 1:  printf("PrimaryVolDesc\n"); break;
			    case 2:  printf("AnchorVolDescPtr\n"); break;
			    case 3:  printf("VolDescPtr\n"); break;
			    case 4:  printf("ImpUseVolDesc\n"); break;
			    case 5:  printf("PartitionDesc\n"); break;
			    case 6:  printf("LogicalVolDesc\n"); break;
			    case 7:  printf("UnallocatedSpaceDesc\n"); break;
			    case 8:  printf("TerminatingDesc\n"); break;
			    case 9:  printf("LogicalVolIntegrityDesc\n"); break;
			    case 0x100: printf("FileSetDesc\n"); break;
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
				       printf("\tTAG-> \t"); 
				       dump_tag(&fid->descTag);

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
			 	    printf("\tFCH: %3xh ", fid->fileCharacteristics);
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
				printf("FileEntry\n");

				printf("   TAG-> \t");
				dump_tag(&fe->descTag);

				printf("ICBTAG-> \t");
				dump_icbtag(&fe->icbTag);

				printf("\t\tUID: %d ", fe->uid);
				printf("GID: %d ", fe->gid);
				printf("Perm: %Xh ", fe->permissions);
				printf("Links: %d ", fe->fileLinkCount);
				/*
				    printf("RFM: %d ", fe->recordFormat);
				    printf("RDA: %d ", fe->recordDisplayAttr);
				    printf("RLN: %d ", fe->recordLength);
				*/
				printf("Length: %Ld ", fe->informationLength);
				printf("Blocks: %Ld\n", fe->logicalBlocksRecorded);

				printf("\t\tCHK: %d ", fe->checkpoint);
				printf("uID: %Ld ", fe->uniqueID);
				printf("LEA: %d ", fe->lengthExtendedAttr);
				printf("LAD: %d\n", fe->lengthAllocDescs);
				switch (fe->icbTag.fileType) {
				    case 0:
				    case 4:
				    {
				        struct FileIdentDesc *fid;
				        long_ad *lad;
				        fid = (struct FileIdentDesc *)(&(fe->extendedAttr[0]));

					printf("\t\tFileIdentDesc: EA\n");
					dump_hex(&fe->extendedAttr[0], fe->lengthExtendedAttr, "\t\t");

					printf("\n\t\tFileIdentDesc: Alloc\n");
					dump_hex(&fe->extendedAttr[0]+fe->lengthExtendedAttr, fe->lengthAllocDescs, "\t\t");

				        printf("\n\t\tTAG-> ");
					dump_tag(&fid->descTag);

				        printf("\t\tFVN: %d ", fid->fileVersionNum);
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
				    break;
				    default:
						if ( (verbose) && (fe->lengthExtendedAttr) )
						{
							printf("\t\tFileIdentDesc: EA\n");
							dump_hex(&fe->extendedAttr[0], fe->lengthExtendedAttr, "\t\t");
						}
						break;
				}
				break;
			    }
			    case 0x106:  printf("ExtendedAttrHeaderDesc\n"); break;
			    case 0x107:  printf("UnallocatedSpaceEntry\n"); break;
			    case 0x108:  printf("SpaceBitmap\n"); break;
			    case 0x109:  printf("PartitionIntegrityEntry\n"); break;
			    default: 
				printf("\n"); break;
			}
		    }
		}
		retval = read(fd, sector, 2048);
		sec++;
	}
	
	close(fd);
	return 0;
}
