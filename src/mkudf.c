/*
 * #include <unistd.h>
 * off_t lseek(int fd, off_t offset, int whence = SEEK_SET)
 * retunrs -1 on error
 */

/*
 * mkudf.c
 *
 * PURPOSE
 *      Create a Linux native UDF file system image.
 *
 * DESCRIPTION
 *      It is _vital_ that all possible error conditions be checked when
 *      writing to the file system image file!
 *
 *      All errors should be output on stderr (not stdout)!
 *
 * CONTACTS
 *      E-mail regarding any portion of the Linux UDF file system should be
 *      directed to the development team mailing list (run by majordomo):
 *              linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL) version 2.0. Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * HISTORY
 *
 * 11/20/98 blf  Took skeleton and started adding code to create a very simple,
 *               but legal udf filesystem
 *
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <udfdecl.h>

/* These should be changed if you make any code changes */
#define VERSION_MAJOR	1
#define VERSION_MINOR	0
#define VERSION_BUILD	0

/*
 * Command line option token values.
 *	0x0000-0x00ff	Single characters
 *	0x1000-0x1fff	Long switches (no arg)
 *	0x2000-0x2fff	Long settings (arg required)
 */
#define OPT_HELP		0x1000
#define OPT_VERBOSE		0x1001
#define OPT_VERSION		0x1002

#define OPT_BLOCKS		0x2000
#define OPT_MEDIA		0x2001

#define OPT_BLK_SIZE	0x2010
#define OPT_LVOL_ID		0x2011

#define OPT_VOL_ID		0x2020
#define OPT_SET_ID		0x2021
#define OPT_ABSTRACT	0x2022
#define OPT_COPYRIGHT	0x2023
#define OPT_VOL_NUM		0x2024
#define OPT_VOL_MAX_NUM	0x2025
#define OPT_VOL_ICHANGE	0x2026
#define OPT_VOL_MAX_ICHANGE	0x2027

#define OPT_FILE_SET_ID	0x2030
#define OPT_FILE_SET_COPYRIGHT	0x2031
#define OPT_FILE_SET_ABSTRACT	0x2032

/* Long command line options */
struct option long_options[] = {
	{ "help", no_argument, NULL, OPT_HELP },
	{ "verbose", no_argument, NULL, OPT_VERBOSE },
	{ "version", no_argument, NULL, OPT_VERSION },
	/* Media Data */
	{ "blocks", required_argument, NULL, OPT_BLOCKS },
	{ "media-type", required_argument, NULL, OPT_MEDIA },
	/* Logical Volume Descriptor */
	{ "blocksize", required_argument, NULL, OPT_BLK_SIZE },
	{ "logical-volume-id", required_argument, NULL, OPT_LVOL_ID },
	/* Primary Volume Descriptor */
	{ "volume-id", required_argument, NULL, OPT_VOL_ID },
	{ "volume-set-id", required_argument, NULL, OPT_SET_ID },
	{ "volume-abstract", required_argument, NULL, OPT_ABSTRACT },
	{ "volume-copyright", required_argument, NULL, OPT_COPYRIGHT },
	{ "volume-number", required_argument, NULL, OPT_VOL_NUM },
	{ "volume-max-number", required_argument, NULL, OPT_VOL_MAX_NUM },
	{ "volume-ichange", required_argument, NULL, OPT_VOL_ICHANGE },
	{ "volume-max-ichange", required_argument, NULL, OPT_VOL_MAX_ICHANGE },
	/* File Set Descriptor */
	{ "file-set-id", required_argument, NULL, OPT_FILE_SET_ID },
	{ "file-set-copyright", required_argument, NULL, OPT_FILE_SET_COPYRIGHT },
	{ "file-set-abstract", required_argument, NULL, OPT_FILE_SET_ABSTRACT }
};

typedef enum media_types
{
	MT_FILE,
	MT_DISK,
	MT_CDR,
	MT_CDRW,
	MT_DVDR,
	MT_DVDRW,
	MT_DVDRAM
} media_types;

/* Command line globals */
char *opt_filename;
unsigned opt_blocks = 0U;

unsigned opt_blocksize = 0U;

/* Generic globals */
int fs_img;
unsigned blocksize;
unsigned blocksize_bits;
unsigned blocks;
unsigned lastblock;
dstring vol_id[32];
dstring lvol_id[128];
dstring set_id[128];
dstring file_set_id[32];

void write_system_area(void);
void get_blocksize(void);
void get_blocks(void);
void write_anchor(int, int, int, int, int);
void write_primaryvoldesc(int, int, timestamp);
void write_logicalvoldesc(int, int, int, int, int, int, int, int);
timestamp query_timestamp(struct timeval *, struct timezone *);
tag query_tag(Uint16, Uint16, Uint16, Uint32, void *, size_t);
icbtag query_icbtag(Uint32, Uint16, Uint16, Uint16, Uint8, Uint32, Uint16, Uint16);

int compute_ident_length(int);

/*
 * usage
 *
 * DESCRIPTION
 *	Output a usage message to stderr, and exit.
 *
 * HISTORY
 *	December 2, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
usage(void)
{
	fprintf(stderr, "usage:\n"
		"\tmkudf [options] FILE\n"
		"Switches:\n"
		"\t--help\n"
		"\t--verbose\n"
		"\t--version\n"
		"Settings:\n"
		"\t--blocks=NUMBER\n"
		"\t--media-type=ow|ro|rw|wo\n"

		"\t--blocksize=NUMBER\n"
		"\t--logical-volume-id=STRING\n"

		"\t--volume-id=STRING\n"
		"\t--volume-set-id=STRING\n"
		"\t--volume-abstract=FILE\n"
		"\t--volume-copyright=FILE\n"
		"\t--volume-number=NUMBER\n"
		"\t--volume-max-number=NUMBER\n"
		"\t--volume-ichange=NUMBER\n"
		"\t--volume-max-ichange=NUMBER\n"

		"\t--file-set-id=STRING\n"
		"\t--file-set-copyright=STRING\n"
		"\t--file-set-abstract=STRING\n"
	);
	exit(0);
}

/*
 * parse_args
 *
 * PURPOSE
 *	Parse the command line args.
 *
 * HISTORY
 *	December 2, 1997 - Andrew E. Mileski
 *	Written, tested, and released
 */
void
parse_args(int argc, char *argv[])
{
	int retval;
	struct ustr instr;
	while (argc > 1) {
		retval = getopt_long(argc, argv, "V", long_options, NULL);
		switch (retval)
		{
			case OPT_HELP:
				usage();
				break;
			case OPT_VERBOSE:
				break;
			case 'V':
			case OPT_VERSION:
				fprintf(stderr, "mkudf (Linux UDF tools) "
					"%d.%d (%d)\n", VERSION_MAJOR,
					VERSION_MINOR, VERSION_BUILD);
				exit(0);

			case OPT_BLOCKS:
				opt_blocks = strtoul(optarg, 0, 0);
				break;
			case OPT_MEDIA:
				break;

			case OPT_BLK_SIZE:
				opt_blocksize = strtoul(optarg, 0, 0);
				break;
			case OPT_LVOL_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 126);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(lvol_id, &instr, 128);
				break;

			case OPT_VOL_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 30);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(vol_id, &instr, 32);
				break;
			case OPT_SET_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 126);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(set_id, &instr, 128);
				break;
			case OPT_ABSTRACT:
			case OPT_COPYRIGHT:
			case OPT_VOL_NUM:
			case OPT_VOL_MAX_NUM:
			case OPT_VOL_ICHANGE:
			case OPT_VOL_MAX_ICHANGE:

			case OPT_FILE_SET_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 30);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(file_set_id, &instr, 32);
			case OPT_FILE_SET_COPYRIGHT:
			case OPT_FILE_SET_ABSTRACT:
				break;

			case EOF:
				return;

			default:
				fprintf(stderr, "Try `mkudf --help' "
					"for more information\n");
				exit(-1);
		}
	}

	if (optind == argc)
		opt_filename = argv[optind];
	else {
		fprintf(stderr, "Try `mkudf --help' for more information\n");
		exit(-1);
	}
}

void write_system_area()
{
	lseek(fs_img, 32768, SEEK_SET);
}

void get_blocksize()
{
	switch (opt_blocksize)
	{
		case 512:
			blocksize = 512;
			blocksize_bits = 9;
			break;
		case 1024:
			blocksize = 1024;
			blocksize_bits = 10;
			break;
		case 4096:
			blocksize = 4096;
			blocksize_bits = 12;
			break;
		case 2048:
		default:
			blocksize = 2048;
			blocksize_bits = 11;
			break;
	}
	fprintf(stderr,"blocksize=%d bits=%d\n", blocksize, blocksize_bits);
}

void get_blocks()
{
	blocks = lseek(fs_img, 0, SEEK_END) >> blocksize_bits;
#if 0
	if (opt_blocks && opt_blocks < blocks)
#else
	if (opt_blocks)
#endif
		blocks = opt_blocks;
	lastblock = blocks-1;
	fprintf(stderr,"blocks=%d opt_blocks=%d lastblock=%d\n", blocks, opt_blocks, lastblock);
}
		

/*
 * write_vrs
 *
 * PURPOSE
 *	Write the Volume Recognition Sequence (VRS).
 *
 * DESCRIPTION
 *	The Linux native VRS contains only 3 descriptors:
 *		BEA01 NSR02 TEA01
 *	This is covered in ECMA-167 part 2, and 3/9.
 *
 * HISTORY
 *	December 3, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
void
write_vrs(void)
{
	struct VolStructDesc vd;
	unsigned index;
	ssize_t retval;

	memset(&vd, 0, sizeof(struct VolStructDesc));

	/* We don't write ISO9660 data so don't write ISO9660 headers */
#if 0
	vd.structType = 0x01U;
	memcpy(vd.stdIdent, STD_ID_CD001, STD_ID_LEN);
	for (index = 0; index < 2048; index += blocksize)
	{
		retval = write(fs_img, &vd, blocksize);
		if (retval == -1)
		{
			perror("error writing CD001 descriptor");
			exit(-1);
		}
	}

	vd.structType = 0xFFU;
	memcpy(vd.stdIdent, STD_ID_CD001, STD_ID_LEN);
	for (index = 0; index < 2048; index += blocksize)
	{
		retval = write(fs_img, &vd, blocksize);
		if (retval == -1)
		{
			perror("error writing CD001 descriptor");
			exit(-1);
		}
	}
#endif

	vd.structType = 0x00U;
	vd.structVersion = 0x01U;

	memcpy(vd.stdIdent, STD_ID_BEA01, STD_ID_LEN);
	for (index = 0; index < 2048; index += blocksize)
	{
		retval = write(fs_img, &vd, blocksize);
		if (retval == -1)
		{
			perror("error writing BEA01 descriptor");
			exit(-1);
		}
	}

	memcpy(vd.stdIdent, STD_ID_NSR02, STD_ID_LEN);
	for (index = 0; index < 2048; index += blocksize)
	{
		retval = write(fs_img, &vd, blocksize);
		if (retval == -1)
		{
			perror("error writing NSR02 descriptor");
			exit(-1);
		}
	}

	memcpy(vd.stdIdent, STD_ID_TEA01, STD_ID_LEN);
	for (index = 0; index < 2048; index += blocksize)
	{
		retval = write(fs_img, &vd, blocksize);
		if (retval == -1)
		{
			perror("error writing TEA01 descriptor");
			exit(-1);
		}
	}
}

void
write_anchor(int loc, int start, int len, int rstart, int rlen)
{
	struct AnchorVolDescPtr *avdp;
	ssize_t retval;
	size_t totsize;

	totsize = sizeof(struct AnchorVolDescPtr);
	avdp = calloc(1, blocksize);

	avdp->mainVolDescSeqExt.extLocation = le32_to_cpu(start);
	avdp->mainVolDescSeqExt.extLength = le32_to_cpu(len * blocksize);
	avdp->reserveVolDescSeqExt.extLocation = le32_to_cpu(rstart);
	avdp->reserveVolDescSeqExt.extLength = le32_to_cpu(rlen * blocksize);
	avdp->descTag = query_tag(TID_ANCHOR_VOL_DESC_PTR, 2, 1, loc, avdp, totsize);

	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, avdp, blocksize);
	if (retval == -1) {
		perror("error writing Anchor descriptor");
		exit(-1);
	}
}

void
write_primaryvoldesc(int loc, int snum, timestamp crtime)
{
	struct PrimaryVolDesc *pvd;
	ssize_t retval;
	size_t totsize;

	totsize = sizeof(struct PrimaryVolDesc);
	pvd = calloc(1, totsize);

	pvd->volDescSeqNum = le32_to_cpu(snum);
	pvd->primaryVolDescNum = le32_to_cpu(0);
	memcpy(pvd->volIdent, vol_id, sizeof(vol_id));
	pvd->volSeqNum = le16_to_cpu(1);
	pvd->maxVolSeqNum = le16_to_cpu(1);
	pvd->interchangeLvl = le16_to_cpu(2);
	pvd->maxInterchangeLvl = le16_to_cpu(3);
	pvd->charSetList = le32_to_cpu(0x00000001);
	pvd->maxCharSetList = le32_to_cpu(0x00000001);
	/* first 8 chars == 32-bit time value */ /* next 8 imp use */
	memcpy(pvd->volSetIdent, set_id, sizeof(set_id));
	pvd->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(pvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	pvd->explanatoryCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(pvd->explanatoryCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	pvd->volAbstract.extLocation = le32_to_cpu(0);
	pvd->volAbstract.extLength = le32_to_cpu(0);
	pvd->volCopyright.extLocation = le32_to_cpu(0);
	pvd->volCopyright.extLength = le32_to_cpu(0);
/*
	pvd.appIdent // Application Identifier
*/
	pvd->recordingDateAndTime = crtime;
	pvd->impIdent.flags = 0;
	strcpy(pvd->impIdent.ident, UDF_ID_DEVELOPER);
	pvd->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	pvd->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
	pvd->predecessorVolDescSeqLocation = le32_to_cpu(19);
	pvd->flags = le16_to_cpu(0x0001);
	pvd->descTag = query_tag(TID_PRIMARY_VOL_DESC, 2, 1, loc, pvd, totsize);

	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, pvd, sizeof(struct PrimaryVolDesc));
	if (retval == -1) {
		perror("error writing Primary Volume descriptor");
		exit(-1);
	}
}

void
write_logicalvoldesc(int loc, int snum, int start, int len, int filesetpart, int filesetblock, int spartable, int sparnum)
{
	struct LogicalVolDesc *lvd;
#if 0
	struct GenericPartitionMap1 *gpm1;
#else
	struct SparablePartitionMap *spm;
#endif
	long_ad *fsd;
	ssize_t retval;
	size_t totsize;
#if 0
	Uint32 maplen = 6;
#else
	Uint32 maplen = 64;
#endif

	totsize = sizeof(struct LogicalVolDesc) + maplen;
	lvd = calloc(1, totsize);

#if 0
	gpm1 = (struct GenericPartitionMap1 *)&(lvd->partitionMaps[0]);
	/* type 1 only */ /* 2 2.2.8, 2.2.9 */
	gpm1->partitionMapType = 1;
	gpm1->partitionMapLength = maplen;
	gpm1->volSeqNum = le16_to_cpu(1);
	gpm1->partitionNum = le16_to_cpu(0);
#else
	spm = (struct SparablePartitionMap *)&(lvd->partitionMaps[0]);
	spm->partitionMapType = 2;
	spm->partitionMapLength = maplen;
	spm->partIdent.flags = 0;
	strncpy(spm->partIdent.ident, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE));
    ((Uint16 *)spm->partIdent.identSuffix)[0] = le16_to_cpu(0x0200);
    spm->partIdent.identSuffix[2] = UDF_OS_CLASS_UNIX;
    spm->partIdent.identSuffix[3] = UDF_OS_ID_LINUX;
	spm->volSeqNum = le16_to_cpu(1);
	spm->partitionNum = le16_to_cpu(0);
	spm->packetLength = le16_to_cpu(32);
	spm->numSparingTables = 1;
	spm->sizeSparingTable = sizeof(struct SparingTable) + sparnum * sizeof(SparingEntry);
	spm->locSparingTable[0] = spartable;
#endif

	lvd->volDescSeqNum = le32_to_cpu(snum);

	lvd->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(lvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);

	memcpy(&lvd->logicalVolIdent, lvol_id, sizeof(lvol_id));

	lvd->logicalBlockSize = le32_to_cpu(blocksize);

	lvd->domainIdent.flags = 0;
	strcpy(lvd->domainIdent.ident, UDF_ID_COMPLIANT);
	((Uint16 *)lvd->domainIdent.identSuffix)[0] = le16_to_cpu(0x0200);
	lvd->domainIdent.identSuffix[2] = 0x00;

	fsd = (long_ad *)lvd->logicalVolContentsUse;
	fsd->extLength = le32_to_cpu(blocksize);
	fsd->extLocation.logicalBlockNum = le32_to_cpu(filesetblock);
	fsd->extLocation.partitionReferenceNum = le16_to_cpu(filesetpart);

	lvd->mapTableLength = le32_to_cpu(maplen);
	lvd->numPartitionMaps = le32_to_cpu(1);

	lvd->impIdent.flags = 0;
	strcpy(lvd->impIdent.ident, UDF_ID_DEVELOPER);
	lvd->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	lvd->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	lvd->integritySeqExt.extLocation = le32_to_cpu(start);
	lvd->integritySeqExt.extLength = le32_to_cpu(len * blocksize);

	lvd->descTag = query_tag(TID_LOGICAL_VOL_DESC, 2, 1, loc, lvd, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, lvd, totsize);
	if (retval == -1) {
	        perror("error writing Logical Volume descriptor");
	        exit(-1);
	}
}

void write_logicalvolintdesc(int loc, timestamp crtime)
{
	int i;
	struct LogicalVolIntegrityDesc *lvid;
	struct LogicalVolIntegrityDescImpUse *lvidiu;
	ssize_t retval;
	size_t totsize;
	Uint32 npart = 1;

	totsize = sizeof(struct LogicalVolIntegrityDesc) + sizeof(Uint32) * npart + sizeof(Uint32) * npart + sizeof(struct LogicalVolIntegrityDescImpUse);
	lvid = calloc(1, totsize);

	for (i=0; i<npart; i++)
		lvid->freeSpaceTable[i] = le32_to_cpu(273388);
	for (i=0; i<npart; i++)
		lvid->sizeTable[i+npart] = le32_to_cpu(273408);
	lvidiu = (struct LogicalVolIntegrityDescImpUse *)&(lvid->impUse[npart*2*sizeof(Uint32)/sizeof(Uint8)]);

	lvidiu->impIdent.flags = 0;
	strcpy(lvidiu->impIdent.ident, UDF_ID_DEVELOPER);
	lvidiu->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	lvidiu->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	lvidiu->numFiles = le32_to_cpu(0);
	lvidiu->numDirs = le32_to_cpu(2);
	lvidiu->minUDFReadRev = le16_to_cpu(0x0150);
	lvidiu->minUDFWriteRev = le16_to_cpu(0x0150);
	lvidiu->maxUDFWriteRev = le16_to_cpu(0x0200);

	lvid->recordingDateAndTime = crtime;
	lvid->integrityType = le32_to_cpu(INTEGRITY_TYPE_CLOSE);
	lvid->nextIntegrityExt.extLocation = le32_to_cpu(0);
	lvid->nextIntegrityExt.extLength = le32_to_cpu(0);
	((Uint64 *)lvid->logicalVolContentsUse)[0] = le64_to_cpu(18); /* Max Unique ID */
	lvid->numOfPartitions = le32_to_cpu(npart);
	lvid->lengthOfImpUse = le32_to_cpu(sizeof(struct LogicalVolIntegrityDescImpUse));

	lvid->descTag = query_tag(TID_LOGICAL_VOL_INTEGRITY_DESC, 2, 1, loc, lvid, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, lvid, totsize);
	if (retval == -1) {
		perror("error writing Logical Volume Integrity descriptor");
		exit(-1);
	}
}

void write_partitionvoldesc(int loc, int snum, int start, int end, int usb)
{
	struct PartitionDesc *pd;
	struct PartitionHeaderDesc *phd;
	ssize_t retval;
	size_t totsize;

	totsize = sizeof(struct PartitionDesc);
	pd = calloc(1, totsize);

	pd->volDescSeqNum = le32_to_cpu(snum);
	pd->partitionFlags = le16_to_cpu(0x0001);
	pd->partitionNumber = le16_to_cpu(0);

	pd->partitionContents.flags = 0;
	strcpy(pd->partitionContents.ident, PARTITION_CONTENTS_NSR02);

	phd = (struct PartitionHeaderDesc *)&(pd->partitionContentsUse[0]);
	phd->unallocatedSpaceTable.extLength = le32_to_cpu(0);
	phd->unallocatedSpaceBitmap.extLength = le32_to_cpu(((end-start)/(blocksize*8) + 1) * blocksize | 0x40000000);
	phd->unallocatedSpaceBitmap.extPosition = le32_to_cpu(usb-start);
	phd->partitionIntegrityTable.extLength = le32_to_cpu(0);
	phd->freedSpaceTable.extLength = le32_to_cpu(0);
	phd->freedSpaceBitmap.extLength = le32_to_cpu(0);

	pd->accessType = le32_to_cpu(PARTITION_ACCESS_RW);
	pd->partitionStartingLocation = le32_to_cpu(start);
	pd->partitionLength = le32_to_cpu(end-start+1);

	pd->impIdent.flags = 0;
	strcpy(pd->impIdent.ident, UDF_ID_DEVELOPER);
	pd->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	pd->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
	
	pd->descTag = query_tag(TID_PARTITION_DESC, 2, 1, loc, pd, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, pd, totsize);
	if (retval == -1) {
		perror("error writing Partition descriptor");
		exit(-1);
	}
}

void write_unallocatedspacedesc(int loc, int snum, int start1, int end1, int start2, int end2)
{
	struct UnallocatedSpaceDesc *usd;
	extent_ad *ead;
	ssize_t retval;
	size_t totsize;
	Uint32 ndesc = 2;

	totsize = sizeof(struct UnallocatedSpaceDesc) + sizeof(extent_ad) * ndesc;
	usd = calloc(1, totsize);

	ead = &(usd->allocDescs[0]);
	ead->extLength = le32_to_cpu((1 + end1 - start1) * blocksize);
	ead->extLocation = le32_to_cpu(start1);

	ead = &(usd->allocDescs[1]);
	ead->extLength = le32_to_cpu((1 + end2 - start2) * blocksize);
	ead->extLocation = le32_to_cpu(start2);
 
	usd->volDescSeqNum = le32_to_cpu(snum);
	usd->numAllocDescs = le32_to_cpu(ndesc);

	usd->descTag = query_tag(TID_UNALLOC_SPACE_DESC, 2, 1, loc, usd, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, usd, totsize);
	if (retval == -1) {
		perror("error writing Unallocated Space descriptor");
		exit(-1);
	}
}

void write_impusevoldesc(int loc, int snum)
{
	struct ImpUseVolDesc *iuvd;
	struct ImpUseVolDescImpUse *iuvdiu;
	ssize_t retval;
	size_t totsize;
	struct ustr instr;

	totsize = sizeof(struct ImpUseVolDesc);
	iuvd = calloc(1, totsize);

	iuvdiu = (struct ImpUseVolDescImpUse *)&(iuvd->impUse[0]);

	iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);

	memcpy(iuvdiu->logicalVolIdent, lvol_id, sizeof(lvol_id));

	memset(&instr, 0, sizeof(struct ustr));
	sprintf(instr.u_name, "mkudf (Linux UDF tools) %d.%d (%d)",
		VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
	instr.u_len = strlen(instr.u_name) + 1;
	udf_UTF8toCS0(iuvdiu->LVInfo1, &instr, 36);

	memset(&instr, 0, sizeof(struct ustr));
	sprintf(instr.u_name, "Linux UDF %s", UDF_VERSION_NOTICE);
	instr.u_len = strlen(instr.u_name) + 1;
	udf_UTF8toCS0(iuvdiu->LVInfo2, &instr, 36);

	memset(&instr, 0, sizeof(struct ustr));
	strncpy(instr.u_name, "<linux_udf@hootie.lvld.hp.com>", 34);
	instr.u_len = strlen(instr.u_name) + 1;
	udf_UTF8toCS0(iuvdiu->LVInfo3, &instr, 36);

	iuvdiu->impIdent.flags = 0;
	strcpy(iuvdiu->impIdent.ident, UDF_ID_DEVELOPER);
	iuvdiu->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	iuvdiu->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	iuvd->volDescSeqNum = le32_to_cpu(snum);

	iuvd->impIdent.flags = 0;
	strcpy(iuvd->impIdent.ident, UDF_ID_LV_INFO);
	((Uint16 *)iuvd->impIdent.identSuffix)[0] = le16_to_cpu(0x0200);
	iuvd->impIdent.identSuffix[2] = UDF_OS_CLASS_UNIX;
	iuvd->impIdent.identSuffix[3] = UDF_OS_ID_LINUX;

	iuvd->descTag = query_tag(TID_IMP_USE_VOL_DESC, 2, 1, loc, iuvd, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, iuvd, totsize);
	if (retval == -1) {
		perror("error writing Imp Use Volume descriptor");
		exit(-1);
	}
}

void write_termvoldesc(int loc)
{
	struct TerminatingDesc *td;
	ssize_t retval;
	size_t totsize;

	totsize = sizeof(struct TerminatingDesc);
	td = calloc(1, totsize);
	
	td->descTag = query_tag(TID_TERMINATING_DESC, 2, 1, loc, td, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, td, totsize);
	if (retval == -1) {
		perror("error writing Terminating descriptor");
		exit(-1);
	}
}

void write_filesetdesc(int loc, int sloc, int snum, int rootpart, int rootblock, timestamp crtime)
{
	struct FileSetDesc *fsd;
	ssize_t retval;
	ssize_t totsize;

	totsize = sizeof(struct FileSetDesc);
	fsd = calloc(1, totsize);

	fsd->recordingDateAndTime = crtime;
	fsd->interchangeLvl = le16_to_cpu(3);
	fsd->maxInterchangeLvl = le16_to_cpu(3);
	fsd->charSetList = le32_to_cpu(0x00000001);
	fsd->maxCharSetList = le32_to_cpu(0x00000001);
	fsd->fileSetNum = 0;
	fsd->fileSetDescNum = 0;
	fsd->logicalVolIdentCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(fsd->logicalVolIdentCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	memcpy(&fsd->logicalVolIdent, lvol_id, sizeof(lvol_id));
	fsd->fileSetCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(fsd->fileSetCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	memcpy(&fsd->fileSetIdent, file_set_id, sizeof(file_set_id));
/*
	copyrightFileIdent[32]
	abstractFileIdent
*/
	fsd->rootDirectoryICB.extLength = le32_to_cpu(blocksize);
	fsd->rootDirectoryICB.extLocation.logicalBlockNum = le32_to_cpu(rootblock);
	fsd->rootDirectoryICB.extLocation.partitionReferenceNum = le16_to_cpu(rootpart);
	fsd->domainIdent.flags = 0;
	strcpy(fsd->domainIdent.ident, UDF_ID_COMPLIANT);
/*
	nextExt
	StreamDirectoryICB
*/
	
	fsd->descTag = query_tag(TID_FILE_SET_DESC, 2, snum, loc - sloc, fsd, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, fsd, totsize);
	free(fsd);
	if (retval == -1) {
		perror("error writing File Set descriptor");
		exit(-1);
	}
}

void write_spartable(int loc, int snum, int start, int num)
{
	struct SparingTable *st;
	ssize_t retval;
	size_t totsize;
	int i;

	totsize = sizeof(struct SparingTable) + num * sizeof(SparingEntry);
	st = calloc(1, totsize);
	st->sparingIdent.flags = 0;
	strncpy(st->sparingIdent.ident, UDF_ID_SPARING, strlen(UDF_ID_SPARING));
    ((Uint16 *)st->sparingIdent.identSuffix)[0] = le16_to_cpu(0x0200);
    st->sparingIdent.identSuffix[2] = UDF_OS_CLASS_UNIX;
    st->sparingIdent.identSuffix[3] = UDF_OS_ID_LINUX;
	st->reallocationTableLen = le16_to_cpu(32);
	st->sequenceNum = le32_to_cpu(0);
	for (i=0; i<num; i++)
	{
		st->mapEntry[i].origLocation = 0xFFFFFFFF;
		st->mapEntry[i].mappedLocation = start + (i*32);
	}

	st->descTag = query_tag(0, 2, snum, loc, st, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, st, totsize);
	free(st);
	if (retval == -1) {
		perror("error writing Sparing Table");
		exit(-1);
	}
}

void write_spacebitmapdesc(int loc, int sloc, int snum, int start, int end)
{
	struct SpaceBitmapDesc *sbd;
	ssize_t retval;
	size_t totsize;
	Uint32 nbytes = (end-start)/8+1;

	totsize = sizeof(struct SpaceBitmapDesc) + sizeof(Uint8) * nbytes;
	sbd = calloc(1, totsize);
	
	sbd->numOfBits = le32_to_cpu(end-start+1);
	sbd->numOfBytes = le32_to_cpu(nbytes);
	memset(sbd->bitmap, 0xFF, sizeof(Uint8) * nbytes);
	sbd->bitmap[0] = 0x00;
	sbd->bitmap[1] = 0x00;
	sbd->bitmap[2] = 0xfc;
    sbd->bitmap[4] = 0xfe;
    sbd->bitmap[8] = 0xfe;

	sbd->descTag = query_tag(TID_SPACE_BITMAP_DESC, 2, snum, loc - sloc, sbd, sizeof(tag));
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, sbd, totsize);
	free(sbd);
	if (retval == -1) {
		perror("error writing Space Bitmap descriptor");
		exit(-1);
	}
}

void write_fileentry1(int loc, int sloc, int snum, int part, int block, timestamp crtime)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	ssize_t retval;
	size_t totsize;
	Uint32 leattr = 0;
	Uint32 filelen2 = 11;
	Uint32 ladesc1 = compute_ident_length(sizeof(struct FileIdentDesc));
	Uint32 ladesc2 = compute_ident_length(sizeof(struct FileIdentDesc) + filelen2);

	totsize = sizeof(struct FileEntry) + ladesc1 + ladesc2;
	fe = calloc(1, totsize);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = le16_to_cpu(1);
	fid->fileCharacteristics = 0x0A; /* 0000 1010 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = le32_to_cpu(blocksize);
	fid->icb.extLocation.logicalBlockNum = le32_to_cpu(block);
	fid->icb.extLocation.partitionReferenceNum = le16_to_cpu(part);
	fid->lengthOfImpUse = le16_to_cpu(0);
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc1);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr + ladesc1]);
	fid->fileVersionNum = le16_to_cpu(1);
	fid->fileCharacteristics = 0x02; /* 0000 0010 */
	fid->lengthFileIdent = filelen2;
	fid->icb.extLength = le32_to_cpu(blocksize);
	fid->icb.extLocation.logicalBlockNum = le32_to_cpu(17);
	fid->icb.extLocation.partitionReferenceNum = le16_to_cpu(part);
	fid->lengthOfImpUse = le16_to_cpu(0);
	strcpy(&fid->fileIdent[0], " lost+found");
	fid->fileIdent[0] = 8;
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc2);

	fe->uid = le32_to_cpu(0);
	fe->gid = le32_to_cpu(0);
	fe->permissions = le32_to_cpu(PERM_U_DELETE|PERM_U_CHATTR|PERM_U_READ|PERM_U_WRITE|PERM_U_EXEC|PERM_G_READ|PERM_G_EXEC|PERM_O_READ|PERM_O_EXEC);
	fe->fileLinkCount = le16_to_cpu(2);
	fe->recordFormat = 0;
	fe->recordDisplayAttr = 0;
	fe->recordLength = le32_to_cpu(0);
	fe->informationLength = le64_to_cpu(ladesc1 + ladesc2 + leattr);
	fe->logicalBlocksRecorded = le64_to_cpu(1);
	fe->accessTime = crtime;
	fe->modificationTime = fe->accessTime;
	fe->attrTime = fe->accessTime;
	fe->checkpoint = 0;
	fe->extendedAttrICB.extLength = le32_to_cpu(0);
	fe->extendedAttrICB.extLocation.logicalBlockNum = le32_to_cpu(0);
	fe->extendedAttrICB.extLocation.partitionReferenceNum = le16_to_cpu(0);

	fe->impIdent.flags = 0;
	strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
	fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	fe->uniqueID = le64_to_cpu(16);
	fe->lengthExtendedAttr = le32_to_cpu(0);
	fe->lengthAllocDescs = le32_to_cpu(ladesc1 + ladesc2);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_DIRECTORY, 0, 0, ICB_FLAG_AD_IN_ICB);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, fe, totsize);
	if (retval == -1) {
		perror("error writing File Entry");
		exit(-1);
	}
}

void write_fileentry2(int loc, int sloc, int snum, int part, int block, timestamp crtime)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	ssize_t retval;
	size_t totsize;
	Uint32 leattr = 0;
	Uint32 ladesc1 = compute_ident_length(sizeof(struct FileIdentDesc));

	totsize = sizeof(struct FileEntry) + ladesc1;
	fe = calloc(1, totsize);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = le16_to_cpu(1);
	fid->fileCharacteristics = 0x0A; /* 0000 1010 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = le32_to_cpu(blocksize);
	fid->icb.extLocation.logicalBlockNum = le32_to_cpu(block);
	fid->icb.extLocation.partitionReferenceNum = le16_to_cpu(part);
	fid->lengthOfImpUse = le16_to_cpu(0);
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc1);

	fe->uid = le32_to_cpu(0);
	fe->gid = le32_to_cpu(0);
	fe->permissions = le32_to_cpu(PERM_U_DELETE|PERM_U_CHATTR|PERM_U_READ|PERM_U_WRITE|PERM_U_EXEC|PERM_G_READ|PERM_G_EXEC|PERM_O_READ|PERM_O_EXEC);
	fe->fileLinkCount = le16_to_cpu(1);
	fe->recordFormat = 0;
	fe->recordDisplayAttr = 0;
	fe->recordLength = le32_to_cpu(0);
	fe->informationLength = le64_to_cpu(ladesc1 + leattr);
	fe->logicalBlocksRecorded = le64_to_cpu(1);
	fe->accessTime = crtime;
	fe->modificationTime = fe->accessTime;
	fe->attrTime = fe->accessTime;
	fe->checkpoint = 0;
	fe->extendedAttrICB.extLength = le32_to_cpu(0);
	fe->extendedAttrICB.extLocation.logicalBlockNum = le32_to_cpu(0);
	fe->extendedAttrICB.extLocation.partitionReferenceNum = le16_to_cpu(0);

	fe->impIdent.flags = 0;
	strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
	fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	fe->uniqueID = le64_to_cpu(17);
	fe->lengthExtendedAttr = le32_to_cpu(0);
	fe->lengthAllocDescs = le32_to_cpu(ladesc1);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_DIRECTORY, 2, 0, ICB_FLAG_AD_IN_ICB);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, fe, totsize);
	if (retval == -1) {
		perror("error writing File Entry");
		exit(-1);
	}
}

timestamp query_timestamp(struct timeval *tv, struct timezone *tz)
{
	timestamp ret;
	struct tm *tm;

	tm = localtime(&tv->tv_sec);

	ret.typeAndTimezone = le16_to_cpu(((-tz->tz_minuteswest) & 0x0FFF) | 0x1000);
	ret.year = le16_to_cpu(1900 + tm->tm_year);
	ret.month = 1 + tm->tm_mon;
	ret.day = tm->tm_mday;
	ret.hour = tm->tm_hour;
	ret.minute = tm->tm_min;
	ret.second = tm->tm_sec;
	ret.centiseconds = tv->tv_usec / 10000;
	ret.hundredsOfMicroseconds = (tv->tv_usec - 
		ret.centiseconds * 10000) / 100;
	ret.microseconds = tv->tv_usec -
		ret.centiseconds * 10000 -
		ret.hundredsOfMicroseconds * 100;
	return ret;
}

tag query_tag(Uint16 Ident, Uint16 descVersion, Uint16 SerialNum,
              Uint32 Location, void *ptr, size_t len)
{
	tag ret;
	int i;

	ret.tagIdent = le16_to_cpu(Ident);
	ret.descVersion = le16_to_cpu(descVersion);
	ret.tagChecksum = 0;
	ret.reserved = 0;
	ret.tagSerialNum = le16_to_cpu(SerialNum);
	ret.descCRCLength = le16_to_cpu(len - sizeof(tag));
	ret.descCRC = le16_to_cpu(udf_crc((char *)ptr + sizeof(tag), le16_to_cpu(ret.descCRCLength), 0));
	ret.tagLocation = le32_to_cpu(Location);
	for (i=0; i<16; i++)
		if (i != 4)
			ret.tagChecksum += (Uint8)(((char *)&ret)[i]);

	return ret;
}

icbtag query_icbtag(Uint32 priorEntries, Uint16 strategyType,
                    Uint16 strategyParm, Uint16 numEntries, Uint8 fileType,
                    Uint32 parBlockNum, Uint16 parRefNum, Uint16 flags)
{
	icbtag ret;

	ret.priorRecordedNumDirectEntries = le32_to_cpu(priorEntries);
	ret.strategyType = le16_to_cpu(strategyType);
	ret.strategyParameter = le16_to_cpu(strategyParm);
	ret.numEntries = le16_to_cpu(numEntries);
	ret.reserved = 0;
	ret.fileType = fileType;
	ret.parentICBLocation.logicalBlockNum = le32_to_cpu(parBlockNum);
	ret.parentICBLocation.partitionReferenceNum = le16_to_cpu(parRefNum);
	ret.flags = le16_to_cpu(flags);

	return ret;
}

int compute_ident_length(int len)
{
	return len + (4 - (len % 4)) % 4;
}

int
main(int argc, char *argv[])
{
	struct timeval tv;
	struct timezone tz;
	timestamp crtime;

	if ( argc < 2 ) {
		usage();
		return 0;
	}
	parse_args(argc, argv);

	fs_img = open(argv[optind], O_WRONLY, 0660);
	if (fs_img == -1) {
		perror("error opening image file");
		return -1;
	}

	gettimeofday(&tv, &tz);
	crtime = query_timestamp(&tv, &tz);

	get_blocksize();
	get_blocks();
	write_system_area();
	write_vrs();
	write_anchor(256, 0x120, 16, 0x140, 16);
	write_anchor(lastblock-256, 0x120, 16, 0x140, 16);
	write_anchor(lastblock, 0x120, 16, 0x140, 16);
	write_primaryvoldesc(0x120, 1, crtime);
	write_primaryvoldesc(0x140, 1, crtime);
	write_logicalvoldesc(0x121, 2, 0x160, 2, 0, 32, 2432, 32);
	write_logicalvoldesc(0x141, 2, 0x160, 2, 0, 32, 2432, 32);
	write_logicalvolintdesc(0x160, crtime);
	write_termvoldesc(0x161);
	write_partitionvoldesc(0x122, 3, 2464, lastblock-288, 2464);
	write_partitionvoldesc(0x142, 3, 2464, lastblock-288, 2464);
	write_unallocatedspacedesc(0x123, 4, 32, 255, 384, 1407);
	write_unallocatedspacedesc(0x143, 4, 32, 255, 384, 1407);
	write_impusevoldesc(0x124, 5);
	write_impusevoldesc(0x144, 5);
	write_termvoldesc(0x125);
	write_termvoldesc(0x145);
	write_spartable(2432, 1, 1408, 32);
	write_spacebitmapdesc(2464, 2464, 1, 2464, lastblock-288);
	write_filesetdesc(2496, 2464, 1, 0, 2528-2464, crtime);
	write_fileentry1(2528, 2464, 1, 0, 2528-2464, crtime);
	write_fileentry2(2481, 2464, 1, 0, 2528-2464, crtime);
	close(fs_img);
	return 0;
}
