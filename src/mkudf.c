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

#include <../src/udfdecl.h>
#include <asm/page.h>

/* These should be changed if you make any code changes */
#define VERSION_MAJOR	0
#define VERSION_MINOR	0
#define VERSION_BUILD	2

/*
 * Command line option token values.
 *	0x0000-0x00ff	Single characters
 *	0x1000-0x1fff	Long switches (no arg)
 *	0x2000-0x2fff	Long settings (arg required)
 */
#define OPT_HELP	0x1010
#define OPT_VERBOSE	0x1020
#define OPT_VERSION	0x1030
#define OPT_BLOCKS	0x2010
#define OPT_BLK_SIZE	0x2020
#define OPT_DIR		0x2030
#define OPT_LVOL_ID	0x2040
#define OPT_MEDIA	0x2050
#define OPT_OFFSET	0x2060
#define OPT_ABSTRACT	0x2070
#define OPT_COPYRIGHT	0x2080
#define OPT_VOL_ID	0x2090
#define OPT_VOL_NUM	0x20a0
#define OPT_SET_ID	0x20b0
#define OPT_SET_SIZE	0x20c0

/* Long command line options */
struct option long_options[] = {
	{ "help", no_argument, NULL, OPT_HELP },
	{ "verbose", no_argument, NULL, OPT_VERBOSE },
	{ "version", no_argument, NULL, OPT_VERSION },
	{ "blocks", required_argument, NULL, OPT_BLOCKS },
	{ "block-size", required_argument, NULL, OPT_BLK_SIZE },
	{ "directory", required_argument, NULL, OPT_DIR },
	{ "logical-volume-id", required_argument, NULL, OPT_LVOL_ID },
	{ "media-type", required_argument, NULL, OPT_MEDIA },
	{ "offset", required_argument, NULL, OPT_OFFSET },
	{ "volume-abstract", required_argument, NULL, OPT_ABSTRACT },
	{ "volume-copyright", required_argument, NULL, OPT_COPYRIGHT },
	{ "volume-id", required_argument, NULL, OPT_VOL_ID },
	{ "volume-number", required_argument, NULL, OPT_VOL_NUM },
	{ "volume-set-id", required_argument, NULL, OPT_SET_ID },
	{ "volume-set-size", required_argument, NULL, OPT_SET_SIZE }
};

/* Command line globals */
char *opt_filename;
unsigned opt_blocksize = 0U;
unsigned opt_blocks = 0U;
char *opt_dir = NULL;
unsigned opt_offset = 0U;
char *opt_abstract = NULL;
char *opt_copyright = NULL;
unsigned opt_set_size = 1U;

/* Generic globals */
int fs_img;
unsigned blocksize;
unsigned blocksize_bits;
unsigned blocks;
unsigned lastblock;
dstring vol_id[32];
dstring lvol_id[128];
dstring set_id[128];

void write_system_area(void);
void get_blocksize(void);
void get_blocks(void);
void write_anchor(int, int, int, int, int);
void write_primaryvoldesc(int, int);
void write_logicalvoldesc(int, int, int, int);
timestamp query_timestamp(struct timeval *, struct timezone *);
tag query_tag(Uint16, Uint16, Uint16, Uint32, void *, size_t);
icbtag query_icbtag(Uint32, Uint16, Uint16, Uint16, Uint8, Uint32, Uint16, Uint16);


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
		"\t--block-size=NUMBER\n"
		"\t--directory=DIRECTORY\n"
		"\t--logical-volume-id=STRING\n"
		"\t--media-type=ow|ro|rw|wo\n"
		"\t--offset=NUMBER\n"
		"\t--volume-abstract=FILE\n"
		"\t--volume-copyright=FILE\n"
		"\t--volume-id=STRING\n"
		"\t--volume-number=NUMBER\n"
		"\t--volume-set-id=STRING\n"
		"\t--volume-set-size=NUMBER\n"
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
		switch (retval) {
			case OPT_BLK_SIZE:
				opt_blocksize = strtoul(optarg, 0, 0);
				break;

			case OPT_DIR:
				opt_dir = optarg;
				break;

			case OPT_VOL_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 30);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(vol_id, &instr, 32);
				break;

			case OPT_LVOL_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 126);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(lvol_id, &instr, 128);
				break;

			case OPT_SET_ID:
				memset(&instr, 0, sizeof(struct ustr));
				strncpy(instr.u_name, optarg, 126);
				instr.u_len = strlen(instr.u_name) + 1;
				udf_UTF8toCS0(set_id, &instr, 128);
				break;

			case EOF:
				return;

			case OPT_HELP:
				usage();

			case OPT_BLOCKS:
				opt_blocks = strtoul(optarg, 0, 0);
				break;

			case OPT_VERSION:
			case 'V':
				fprintf(stderr, "mkudf (Linux UDF tools) "
					"%d.%d (%d)\n", VERSION_MAJOR,
					VERSION_MINOR, VERSION_BUILD);
				exit(0);

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
}

void get_blocks()
{
	blocks = lseek(fs_img, 0, SEEK_END) >> blocksize_bits;
	if (opt_blocks && opt_blocks < blocks)
		blocks = opt_blocks;
	lastblock = blocks-1;
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
	avdp = calloc(1, totsize);

	avdp->mainVolDescSeqExt.extLocation = htofsl(start);
	avdp->mainVolDescSeqExt.extLength = htofsl(len * blocksize);
	avdp->reserveVolDescSeqExt.extLocation = htofsl(rstart);
	avdp->reserveVolDescSeqExt.extLength = htofsl(rlen * blocksize);
	avdp->descTag = query_tag(TID_ANCHOR_VOL_DESC_PTR, 2, 1, loc, avdp, totsize);

	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, avdp, totsize);
	if (retval == -1) {
		perror("error writing Anchor descriptor");
		exit(-1);
	}
}

void
write_primaryvoldesc(int loc, int snum)
{
	struct PrimaryVolDesc *pvd;
	ssize_t retval;
	size_t totsize;
	struct timeval tv;
	struct timezone tz;

	totsize = sizeof(struct PrimaryVolDesc);
	pvd = calloc(1, totsize);

	pvd->volDescSeqNum = htofsl(snum);
	pvd->primaryVolDescNum = htofsl(0);
	memcpy(pvd->volIdent, vol_id, sizeof(vol_id));
	pvd->volSeqNum = htofss(1);
	pvd->maxVolSeqNum = htofss(1);
	pvd->interchangeLvl = htofss(2);
	pvd->maxInterchangeLvl = htofss(3);
	pvd->charSetList = htofsl(0x00000001);
	pvd->maxCharSetList = htofsl(0x00000001);
	/* first 8 chars == 32-bit time value */ /* next 8 imp use */
	memcpy(pvd->volSetIdent, set_id, sizeof(set_id));
	pvd->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(pvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	pvd->explanatoryCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(pvd->explanatoryCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	pvd->volAbstract.extLocation = htofsl(0);
	pvd->volAbstract.extLength = htofsl(0);
	pvd->volCopyrightNotice.extLocation = htofsl(0);
	pvd->volCopyrightNotice.extLength = htofsl(0);
/*
	pvd.appIdent // Application Identifier
*/
	gettimeofday(&tv, &tz);
	pvd->recordingDateAndTime = query_timestamp(&tv, &tz);
	pvd->impIdent.flags = 0;
	strcpy(pvd->impIdent.ident, UDF_ID_DEVELOPER);
	pvd->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	pvd->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
	pvd->predecessorVolDescSeqLocation = htofsl(19);
	pvd->flags = htofss(0x0001);
	pvd->descTag = query_tag(TID_PRIMARY_VOL_DESC, 2, 1, loc, pvd, totsize);

	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, pvd, sizeof(struct PrimaryVolDesc));
	if (retval == -1) {
		perror("error writing Primary Volume descriptor");
		exit(-1);
	}
}

void
write_logicalvoldesc(int loc, int snum, int start, int len)
{
	struct LogicalVolDesc *lvd;
	struct GenericPartitionMap1 *gpm1;
	long_ad *fsd;
	ssize_t retval;
	size_t totsize;
	Uint32 maplen;

	totsize = sizeof(struct LogicalVolDesc) + sizeof(struct GenericPartitionMap1);
	lvd = calloc(1, totsize);

	gpm1 = (struct GenericPartitionMap1 *)&(lvd->partitionMaps[0]);
	/* type 1 only */ /* 2 2.2.8, 2.2.9 */
	gpm1->partitionMapType = 1;
	gpm1->partitionMapLength = 6;
	maplen = gpm1->partitionMapLength;
	gpm1->volSeqNum = htofss(1);
	gpm1->partitionNum = htofss(0);

	lvd->volDescSeqNum = htofsl(snum);

	lvd->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(lvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);

	memcpy(&lvd->logicalVolIdent, lvol_id, sizeof(lvol_id));

	lvd->logicalBlockSize = htofsl(blocksize);

	lvd->domainIdent.flags = 0;
	strcpy(lvd->domainIdent.ident, UDF_ID_COMPLIANT);
	((Uint16 *)lvd->domainIdent.identSuffix)[0] = htofss(0x0200);
	lvd->domainIdent.identSuffix[2] = 0x00;

	fsd = (long_ad *)lvd->logicalVolContentsUse;
	fsd->extLength = htofsl(blocksize);
	fsd->extLocation.logicalBlockNum = htofsl(0);
	fsd->extLocation.partitionReferenceNum = htofss(0);

	lvd->mapTableLength = htofsl(maplen);
	lvd->numPartitionMaps = htofsl(1);

	lvd->impIdent.flags = 0;
	strcpy(lvd->impIdent.ident, UDF_ID_DEVELOPER);
	lvd->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	lvd->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	lvd->integritySeqExt.extLocation = htofsl(start);
	lvd->integritySeqExt.extLength = htofsl(len * blocksize);

	lvd->descTag = query_tag(TID_LOGICAL_VOL_DESC, 2, 1, loc, lvd, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, lvd, totsize);
	if (retval == -1) {
	        perror("error writing Logical Volume descriptor");
	        exit(-1);
	}
}

void write_logicalvolintdesc(int loc)
{
	int i;
	struct LogicalVolIntegrityDesc *lvid;
	struct LogicalVolIntegrityDescImpUse *lvidiu;
	ssize_t retval;
	size_t totsize;
	struct timeval tv;
	struct timezone tz;
	Uint32 npart = 1;

	totsize = sizeof(struct LogicalVolIntegrityDesc) + sizeof(Uint32) * npart + sizeof(Uint32) * npart + sizeof(struct LogicalVolIntegrityDescImpUse);
	lvid = calloc(1, totsize);

	for (i=0; i<npart; i++)
		lvid->freeSpaceTable[i] = htofsl(196);
	for (i=0; i<npart; i++)
		lvid->sizeTable[i+npart] = htofsl(201);
	lvidiu = (struct LogicalVolIntegrityDescImpUse *)&(lvid->impUse[npart*2*sizeof(Uint32)/sizeof(Uint8)]);

	lvidiu->impIdent.flags = 0;
	strcpy(lvidiu->impIdent.ident, UDF_ID_DEVELOPER);
	lvidiu->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	lvidiu->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	lvidiu->numFiles = htofsl(1);
	lvidiu->numDirs = htofsl(3);
	lvidiu->minUDFReadRev = htofss(0x0150);
	lvidiu->minUDFWriteRev = htofss(0x0150);
	lvidiu->maxUDFWriteRev = htofss(0x0200);

	gettimeofday(&tv, &tz);
	lvid->recordingDateAndTime = query_timestamp(&tv, &tz);
	lvid->integrityType = htofsl(INTEGRITY_TYPE_OPEN);
	lvid->nextIntegrityExt.extLocation = htofsl(0);
	lvid->nextIntegrityExt.extLength = htofsl(0);
	((Uint64 *)lvid->logicalVolContentsUse)[0] = htofsll(3); /* Max Unique ID */
	lvid->numOfPartitions = htofsl(npart);
	lvid->lengthOfImpUse = htofsl(sizeof(struct LogicalVolIntegrityDescImpUse));

	lvid->descTag = query_tag(TID_LOGICAL_VOL_INTEGRITY_DESC, 2, 1, loc, lvid, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, lvid, totsize);
	if (retval == -1) {
		perror("error writing Logical Volume Integrity descriptor");
		exit(-1);
	}
}

void write_partitionvoldesc(int loc, int snum)
{
	struct PartitionDesc *pd;
	struct PartitionHeaderDesc *phd;
	ssize_t retval;
	size_t totsize;

	totsize = sizeof(struct PartitionDesc);
	pd = calloc(1, totsize);

	pd->volDescSeqNum = htofsl(snum);
	pd->partitionFlags = htofss(0x0001);
	pd->partitionNumber = htofss(0);

	pd->partitionContents.flags = 0;
	strcpy(pd->partitionContents.ident, PARTITION_CONTENTS_NSR02);

	phd = (struct PartitionHeaderDesc *)&(pd->partitionContentsUse[0]);
	phd->unallocatedSpaceTable.extLength = htofsl(0);
	phd->unallocatedSpaceBitmap.extLength = htofsl(blocksize | 0x40000000);
	phd->unallocatedSpaceBitmap.extPosition = htofsl(1);
	phd->partitionIntegrityTable.extLength = htofsl(0);
	phd->freedSpaceTable.extLength = htofsl(0);
	phd->freedSpaceBitmap.extLength = htofsl(0);

	pd->accessType = htofsl(PARTITION_ACCESS_OW);
	pd->partitionStartingLocation = htofsl(55);
	pd->partitionLength = htofsl(201);

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

void write_unallocatedspacedesc(int loc, int snum, int start, int end)
{
	struct UnallocatedSpaceDesc *usd;
	extent_ad *ead;
	ssize_t retval;
	size_t totsize;
	Uint32 ndesc = 1;


	totsize = sizeof(struct UnallocatedSpaceDesc) + sizeof(extent_ad) * ndesc;
	usd = calloc(1, totsize);

	ead = (extent_ad *)&(usd->allocDescs[0]);
	ead->extLength = htofsl((1 + end - start) * blocksize);
	ead->extLocation = htofsl(start);
 
	usd->volDescSeqNum = htofsl(snum);
	usd->numAllocDescs = htofsl(ndesc);

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

	totsize = sizeof(struct ImpUseVolDesc);
	iuvd = calloc(1, totsize);

	iuvdiu = (struct ImpUseVolDescImpUse *)&(iuvd->impUse[0]);

	iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);

	memcpy(iuvdiu->logicalVolIdent, lvol_id, sizeof(lvol_id));

	iuvdiu->impIdent.flags = 0;
	strcpy(iuvdiu->impIdent.ident, UDF_ID_DEVELOPER);
	iuvdiu->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	iuvdiu->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	iuvd->volDescSeqNum = htofsl(snum);

	iuvd->impIdent.flags = 0;
	strcpy(iuvd->impIdent.ident, UDF_ID_LV_INFO);
	((Uint16 *)iuvd->impIdent.identSuffix)[0] = htofss(0x0200);
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

void write_filesetdesc(int loc, int sloc, int snum)
{
	struct FileSetDesc *fsd;
	ssize_t retval;
	ssize_t totsize;
	struct timeval tv;
	struct timezone tz;

	totsize = sizeof(struct FileSetDesc);
	fsd = calloc(1, totsize);

	gettimeofday(&tv, &tz);
	fsd->recordingDateAndTime = query_timestamp(&tv, &tz);
	fsd->interchangeLvl = htofss(3);
	fsd->maxInterchangeLvl = htofss(3);
	fsd->charSetList = htofsl(0x00000001);
	fsd->maxCharSetList = htofsl(0x00000001);
	fsd->fileSetNum = 0;
	fsd->fileSetDescNum = 0;
	fsd->logicalVolIdentCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(fsd->logicalVolIdentCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	memcpy(&fsd->logicalVolIdent, lvol_id, sizeof(lvol_id));
	fsd->fileSetCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(fsd->fileSetCharSet.charSetInfo, UDF_CHAR_SET_INFO);
/*
	filesetident[32]
	copyrightFileIdent[32]
	abstractFileIdent
*/
	fsd->rootDirectoryICB.extLength = htofsl(blocksize);
	fsd->rootDirectoryICB.extLocation.logicalBlockNum = htofsl(2);
	fsd->rootDirectoryICB.extLocation.partitionReferenceNum = htofss(0);
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

void write_spacebitmapdesc(int loc, int sloc, int snum)
{
	struct SpaceBitmapDesc *sbd;
	ssize_t retval;
	size_t totsize;
	Uint32 nbytes = 26;

	totsize = sizeof(struct SpaceBitmapDesc) + sizeof(Uint8) * nbytes;
	sbd = calloc(1, totsize);
	
	sbd->numOfBits = htofsl(201);
	sbd->numOfBytes = htofsl(nbytes);
	memset(sbd->bitmap, 0xFF, sizeof(Uint8) * nbytes);
	sbd->bitmap[0] = 0x80;
	sbd->bitmap[1] = 0xFE;

	sbd->descTag = query_tag(TID_SPACE_BITMAP_DESC, 2, snum, loc - sloc, sbd, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, sbd, totsize);
	free(sbd);
	if (retval == -1) {
		perror("error writing Space Bitmap descriptor");
		exit(-1);
	}
}

void write_fileentry1(int loc, int sloc, int snum)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	ssize_t retval;
	size_t totsize;
	struct timeval tv;
	struct timezone tz;
	Uint32 leattr = 0;
	Uint32 filelen2 = 11;
	Uint32 filelen3 = 5;
	Uint32 ladesc1 = compute_ident_length(sizeof(struct FileIdentDesc));
	Uint32 ladesc2 = compute_ident_length(sizeof(struct FileIdentDesc) + filelen2);
	Uint32 ladesc3 = compute_ident_length(sizeof(struct FileIdentDesc) + filelen3);

	totsize = sizeof(struct FileEntry) + ladesc1 + ladesc2 + ladesc3;
	fe = calloc(1, totsize);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = htofss(1);
	fid->fileCharacteristics = 0x0A; /* 0000 1010 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = htofsl(blocksize);
	fid->icb.extLocation.logicalBlockNum = htofsl(2);
	fid->icb.extLocation.partitionReferenceNum = htofss(0);
	fid->lengthOfImpUse = htofss(0);
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc1);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr + ladesc1]);
	fid->fileVersionNum = htofss(1);
	fid->fileCharacteristics = 0x02; /* 0000 0010 */
	fid->lengthFileIdent = filelen2;
	fid->icb.extLength = htofsl(blocksize);
	fid->icb.extLocation.logicalBlockNum = htofsl(3);
	fid->icb.extLocation.partitionReferenceNum = htofss(0);
	fid->lengthOfImpUse = htofss(0);
	strcpy(&fid->fileIdent[0], " lost+found");
	fid->fileIdent[0] = 8;
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc2);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr + ladesc1 + ladesc2]);
	fid->fileVersionNum = htofss(1);
	fid->fileCharacteristics = 0x02; /* 0000 0010 */
	fid->lengthFileIdent = filelen3;
	fid->icb.extLength = htofsl(blocksize);
	fid->icb.extLocation.logicalBlockNum = htofsl(4);
	fid->icb.extLocation.partitionReferenceNum = htofss(0);
	fid->lengthOfImpUse = htofss(0);
	strcpy(&fid->fileIdent[0], " test");
	fid->fileIdent[0] = 8;
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc3);

	fe->uid = htofsl(0);
	fe->gid = htofsl(0);
	fe->permissions = htofsl(PERM_U_DELETE|PERM_U_CHATTR|PERM_U_READ|PERM_U_WRITE|PERM_U_EXEC|PERM_G_READ|PERM_G_EXEC|PERM_O_READ|PERM_O_EXEC);
	fe->fileLinkCount = htofss(3);
	fe->recordFormat = 0;
	fe->recordDisplayAttr = 0;
	fe->recordLength = htofsl(0);
	fe->informationLength = htofsll(ladesc1 + ladesc2 + ladesc3 + leattr);
	fe->logicalBlocksRecorded = htofsll(1);
	gettimeofday(&tv, &tz);
	fe->accessTime = query_timestamp(&tv, &tz);
	fe->modificationTime = fe->accessTime;
	fe->attrTime = fe->accessTime;
	fe->checkpoint = 0;
	fe->extendedAttrICB.extLength = htofsl(0);
	fe->extendedAttrICB.extLocation.logicalBlockNum = htofsl(0);
	fe->extendedAttrICB.extLocation.partitionReferenceNum = htofss(0);

	fe->impIdent.flags = 0;
	strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
	fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	fe->uniqueID = htofsll(0);
	fe->lengthExtendedAttr = htofsl(0);
	fe->lengthAllocDescs = htofsl(ladesc1 + ladesc2 + ladesc3);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_DIRECTORY, 0, 0, ICB_FLAG_AD_IN_ICB);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, fe, totsize);
	if (retval == -1) {
		perror("error writing File Entry");
		exit(-1);
	}
}

void write_fileentry2(int loc, int sloc, int snum)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	ssize_t retval;
	size_t totsize;
	struct timeval tv;
	struct timezone tz;
	Uint32 leattr = 0;
	Uint32 filelen2 = 5;
	Uint32 ladesc1 = compute_ident_length(sizeof(struct FileIdentDesc));
	Uint32 ladesc2 = compute_ident_length(sizeof(struct FileIdentDesc) + filelen2);

	totsize = sizeof(struct FileEntry) + ladesc1 + ladesc2;
	fe = calloc(1, totsize);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = htofss(1);
	fid->fileCharacteristics = 0x0A; /* 0000 1010 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = htofsl(blocksize);
	fid->icb.extLocation.logicalBlockNum = htofsl(2);
	fid->icb.extLocation.partitionReferenceNum = htofss(0);
	fid->lengthOfImpUse = htofss(0);
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc1);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr + ladesc1]);
	fid->fileVersionNum = htofss(1);
	fid->fileCharacteristics = 0x00; /* 0000 0000 */
	fid->lengthFileIdent = filelen2;
	fid->icb.extLength = htofsl(blocksize);
	fid->icb.extLocation.logicalBlockNum = htofsl(5);
	fid->icb.extLocation.partitionReferenceNum = htofss(0);
	fid->lengthOfImpUse = htofss(0);
	strcpy(&fid->fileIdent[0], " temp");
	fid->fileIdent[0] = 8;
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc2);

	fe->uid = htofsl(0);
	fe->gid = htofsl(0);
	fe->permissions = htofsl(PERM_U_DELETE|PERM_U_CHATTR|PERM_U_READ|PERM_U_WRITE|PERM_U_EXEC|PERM_G_READ|PERM_G_EXEC|PERM_O_READ|PERM_O_EXEC);
	fe->fileLinkCount = htofss(1);
	fe->recordFormat = 0;
	fe->recordDisplayAttr = 0;
	fe->recordLength = htofsl(0);
	fe->informationLength = htofsll(ladesc1 + ladesc2 + leattr);
	fe->logicalBlocksRecorded = htofsll(1);
	gettimeofday(&tv, &tz);
	fe->accessTime = query_timestamp(&tv, &tz);
	fe->modificationTime = fe->accessTime;
	fe->attrTime = fe->accessTime;
	fe->checkpoint = 0;
	fe->extendedAttrICB.extLength = htofsl(0);
	fe->extendedAttrICB.extLocation.logicalBlockNum = htofsl(0);
	fe->extendedAttrICB.extLocation.partitionReferenceNum = htofss(0);

	fe->impIdent.flags = 0;
	strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
	fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	fe->uniqueID = htofsll(1);
	fe->lengthExtendedAttr = htofsl(0);
	fe->lengthAllocDescs = htofsl(ladesc1 + ladesc2);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_DIRECTORY, 2, 0, ICB_FLAG_AD_IN_ICB);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, fe, totsize);
	if (retval == -1) {
		perror("error writing File Entry");
		exit(-1);
	}
}

void write_fileentry3(int loc, int sloc, int snum)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	ssize_t retval;
	size_t totsize;
	struct timeval tv;
	struct timezone tz;
	Uint32 leattr = 0;
	Uint32 ladesc1 = compute_ident_length(sizeof(struct FileIdentDesc));

	totsize = sizeof(struct FileEntry) + ladesc1;
	fe = calloc(1, totsize);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = htofss(1);
	fid->fileCharacteristics = 0x0A; /* 0000 1010 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = htofsl(blocksize);
	fid->icb.extLocation.logicalBlockNum = htofsl(2);
	fid->icb.extLocation.partitionReferenceNum = htofss(0);
	fid->lengthOfImpUse = htofss(0);
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc1);

	fe->uid = htofsl(0);
	fe->gid = htofsl(0);
	fe->permissions = htofsl(PERM_U_DELETE|PERM_U_CHATTR|PERM_U_READ|PERM_U_WRITE|PERM_U_EXEC|PERM_G_READ|PERM_G_EXEC|PERM_O_READ|PERM_O_EXEC);
	fe->fileLinkCount = htofss(1);
	fe->recordFormat = 0;
	fe->recordDisplayAttr = 0;
	fe->recordLength = htofsl(0);
	fe->informationLength = htofsll(ladesc1 + leattr);
	fe->logicalBlocksRecorded = htofsll(1);
	gettimeofday(&tv, &tz);
	fe->accessTime = query_timestamp(&tv, &tz);
	fe->modificationTime = fe->accessTime;
	fe->attrTime = fe->accessTime;
	fe->checkpoint = 0;
	fe->extendedAttrICB.extLength = htofsl(0);
	fe->extendedAttrICB.extLocation.logicalBlockNum = htofsl(0);
	fe->extendedAttrICB.extLocation.partitionReferenceNum = htofss(0);

	fe->impIdent.flags = 0;
	strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
	fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	fe->uniqueID = htofsll(2);
	fe->lengthExtendedAttr = htofsl(0);
	fe->lengthAllocDescs = htofsl(ladesc1);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_DIRECTORY, 2, 0, ICB_FLAG_AD_IN_ICB);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, fe, totsize);
	if (retval == -1) {
		perror("error writing File Entry");
		exit(-1);
	}
}

void write_fileentry4(int loc, int sloc, int snum)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	short_ad *sad;
	long_ad *lad;
	ssize_t retval;
	size_t totsize;
	struct timeval tv;
	struct timezone tz;
	Uint32 leattr = 0;
#if 0
	Uint32 ladesc1 = compute_ident_length(sizeof(long_ad));
#else
	Uint32 ladesc1 = compute_ident_length(sizeof(short_ad));
	Uint32 ladesc2 = compute_ident_length(sizeof(short_ad));
#endif

	totsize = sizeof(struct FileEntry) + ladesc1 + ladesc2;
	fe = calloc(1, totsize);

#if 0
	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = htofss(1);
	fid->fileCharacteristics = 0x08; /* 0000 1000 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = htofsl(blocksize);
	fid->icb.extLocation.logicalBlockNum = htofsl(5);
	fid->icb.extLocation.partitionReferenceNum = htofss(0);
	fid->lengthOfImpUse = htofss(0);
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc1)
#endif
#if 1
	sad = (short_ad *)(&(fe->allocDescs[leattr]));
	sad->extLength = htofsl(blocksize);
	sad->extPosition = htofsl(6);
	sad = (short_ad *)(&(fe->allocDescs[leattr + ladesc1]));
	sad->extLength = htofsl(blocksize/2);
	sad->extPosition = htofsl(8);
#else
	lad = (long_ad *)(&(fe->allocDescs[leattr]));
	lad->extLength = htofsl(blocksize);
	lad->extLocation.logicalBlockNum = htofsl(5);
	lad->extLocation.partitionReferenceNum = htofss(0);
#endif

	fe->uid = htofsl(0);
	fe->gid = htofsl(0);
	fe->permissions = htofsl(PERM_U_DELETE|PERM_U_CHATTR|PERM_U_READ|PERM_U_WRITE|PERM_G_READ);
	fe->fileLinkCount = htofss(1);
	fe->recordFormat = 0;
	fe->recordDisplayAttr = 0;
	fe->recordLength = htofsl(0);
	fe->informationLength = htofsll(blocksize + blocksize/2);
	fe->logicalBlocksRecorded = htofsll(2);
	gettimeofday(&tv, &tz);
	fe->accessTime = query_timestamp(&tv, &tz);
	fe->modificationTime = fe->accessTime;
	fe->attrTime = fe->accessTime;
	fe->checkpoint = 0;
	fe->extendedAttrICB.extLength = htofsl(0);
	fe->extendedAttrICB.extLocation.logicalBlockNum = htofsl(0);
	fe->extendedAttrICB.extLocation.partitionReferenceNum = htofss(0);

	fe->impIdent.flags = 0;
	strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
	fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	fe->uniqueID = htofsll(3);
	fe->lengthExtendedAttr = htofsl(0);
	fe->lengthAllocDescs = htofsl(ladesc1 + ladesc2);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_REGULAR, 4, 0, ICB_FLAG_AD_SHORT);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, fe, totsize);
	if (retval == -1) {
		perror("error writing File Entry");
		exit(-1);
	}
}

void write_data(int loc, char *data)
{
	ssize_t retval;

	lseek(fs_img, loc << blocksize_bits, SEEK_SET);
	retval = write(fs_img, data, strlen(data));
        if (retval == -1) {
                perror("error writing data");
                exit(-1);
	}
}

timestamp query_timestamp(struct timeval *tv, struct timezone *tz)
{
	timestamp ret;
	struct tm *tm;

	tm = localtime(&tv->tv_sec);

	ret.typeAndTimezone = htofss(((-tz->tz_minuteswest) & 0x0FFF) | 0x1000);
	ret.year = htofss(1900 + tm->tm_year);
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

	ret.tagIdent = htofss(Ident);
	ret.descVersion = htofss(descVersion);
	ret.tagChecksum = 0;
	ret.reserved = 0;
	ret.tagSerialNum = htofss(SerialNum);
	ret.descCRCLength = htofss(len - sizeof(tag));
	ret.descCRC = htofss(udf_crc((char *)ptr + sizeof(tag), fstohs(ret.descCRCLength)));
	ret.tagLocation = htofsl(Location);
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

	ret.priorRecordedNumDirectEntries = htofsl(priorEntries);
	ret.strategyType = htofss(strategyType);
	ret.strategyParameter = htofss(strategyParm);
	ret.numEntries = htofss(numEntries);
	ret.reserved = 0;
	ret.fileType = fileType;
	ret.parentICBLocation.logicalBlockNum = htofsl(parBlockNum);
	ret.parentICBLocation.partitionReferenceNum = htofss(parRefNum);
	ret.flags = htofss(flags);

	return ret;
}

int compute_ident_length(int len)
{
	return len + (4 - (len % 4)) % 4;
}

int
main(int argc, char *argv[])
{
	parse_args(argc, argv);

	fs_img = open(argv[optind], O_WRONLY);
	if (fs_img == -1) {
		perror("error opening image file");
		return -1;
	}

	get_blocksize();
	get_blocks();
	write_system_area();
	write_vrs();
	write_anchor(256, 21, 16, 37, 16);
#if 0
	write_anchor(lastblock-256, 19, 16, 35, 16);
#endif
	write_anchor(lastblock, 21, 16, 36, 16);
	write_primaryvoldesc(21, 1);
	write_primaryvoldesc(37, 1);
	write_logicalvoldesc(22, 2, 53, 2);
	write_logicalvoldesc(38, 2, 53, 2);
	write_logicalvolintdesc(53);
	write_termvoldesc(54);
	write_partitionvoldesc(23, 3);
	write_partitionvoldesc(39, 3);
	write_unallocatedspacedesc(24, 4, 257, lastblock-1);
	write_unallocatedspacedesc(40, 4, 257, lastblock-1);
	write_impusevoldesc(25, 5);
	write_impusevoldesc(41, 5);
	write_termvoldesc(26);
	write_termvoldesc(42);
	write_filesetdesc(55, 55, 1);
	write_spacebitmapdesc(56, 55, 1);
	write_fileentry1(57, 55, 1);
	write_fileentry2(58, 55, 1);
	write_fileentry3(59, 55, 1);
	write_fileentry4(60, 55, 1);
	write_data(61,
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"01234567"
);
	write_data(62, "----> BROKE! <----");
	write_data(63,
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$"
"01234567"
);
	close(fs_img);
	return 0;
}
