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
 *              linux_udf@hpesjro.fc.hp.com
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
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>

#include <udfdecl.h>
#include "mkudf.h"

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
write_vrs(write_func udf_write_data, mkudf_options *opt, int start)
{
	struct VolStructDesc vd;
	int blocksize_bits = opt->blocksize_bits;
	int blocksize = opt->blocksize;
	if (blocksize < sizeof(struct VolStructDesc))
		blocksize = sizeof(struct VolStructDesc);
	opt->blocksize_bits = 11;

	memset(&vd, 0, sizeof(struct VolStructDesc));

	/* We don't write ISO9660 data so don't write ISO9660 headers */
#if 0
	vd.structType = 0x01U;
	memcpy(vd.stdIdent, STD_ID_CD001, STD_ID_LEN);
	udf_write_data(opt, start >> opt->blocksize_bits, &vd, sizeof(struct VolStructDesc), "CD001 descriptor");
	start += blocksize;

	vd.structType = 0xFFU;
	memcpy(vd.stdIdent, STD_ID_CD001, STD_ID_LEN);
	udf_write_data(opt, start >> opt->blocksize_bits, &vd, sizeof(struct VolStructDesc), "CD001 descriptor");
 	start += blocksize;
#endif

	vd.structType = 0x00U;
	vd.structVersion = 0x01U;

	memcpy(vd.stdIdent, STD_ID_BEA01, STD_ID_LEN);
	udf_write_data(opt, start >> opt->blocksize_bits, &vd, sizeof(struct VolStructDesc), "BEA01 descriptor");
	start += blocksize;

	memcpy(vd.stdIdent, STD_ID_NSR02, STD_ID_LEN);
	udf_write_data(opt, start >> opt->blocksize_bits, &vd, sizeof(struct VolStructDesc), "NSR002 descriptor");
	start += blocksize;

	memcpy(vd.stdIdent, STD_ID_TEA01, STD_ID_LEN);
	udf_write_data(opt, start >> opt->blocksize_bits, &vd, sizeof(struct VolStructDesc), "TEA01 descriptor");
	start += blocksize;

	opt->blocksize_bits = blocksize_bits;
}

void
write_anchor(write_func udf_write_data, mkudf_options *opt, int loc, int start, int len, int rstart, int rlen)
{
	struct AnchorVolDescPtr *avdp;
	size_t totsize;

	totsize = sizeof(struct AnchorVolDescPtr);
	avdp = calloc(1, opt->blocksize);

	avdp->mainVolDescSeqExt.extLocation = le32_to_cpu(start);
	avdp->mainVolDescSeqExt.extLength = le32_to_cpu(len * opt->blocksize);
	avdp->reserveVolDescSeqExt.extLocation = le32_to_cpu(rstart);
	avdp->reserveVolDescSeqExt.extLength = le32_to_cpu(rlen * opt->blocksize);
	avdp->descTag = query_tag(TID_ANCHOR_VOL_DESC_PTR, 2, 1, loc, avdp, totsize);

	udf_write_data(opt, loc, avdp, opt->blocksize, "Anchor descriptor");
	free(avdp);
}

void
write_primaryvoldesc(write_func udf_write_data, mkudf_options *opt, int loc, int snum, timestamp crtime)
{
	struct PrimaryVolDesc *pvd;
	size_t totsize;

	totsize = sizeof(struct PrimaryVolDesc);
	pvd = calloc(1, totsize);

	pvd->volDescSeqNum = le32_to_cpu(snum);
	pvd->primaryVolDescNum = le32_to_cpu(0);
	memcpy(pvd->volIdent, opt->vol_id, sizeof(opt->vol_id));
	pvd->volSeqNum = le16_to_cpu(1);
	pvd->maxVolSeqNum = le16_to_cpu(1);
	pvd->interchangeLvl = le16_to_cpu(2);
	pvd->maxInterchangeLvl = le16_to_cpu(3);
	pvd->charSetList = le32_to_cpu(0x00000001);
	pvd->maxCharSetList = le32_to_cpu(0x00000001);
	/* first 8 chars == 32-bit time value */ /* next 8 imp use */
	memcpy(pvd->volSetIdent, opt->set_id, sizeof(opt->set_id));
	pvd->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(pvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	pvd->explanatoryCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(pvd->explanatoryCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	pvd->volAbstract.extLocation = le32_to_cpu(0);
	pvd->volAbstract.extLength = le32_to_cpu(0);
	pvd->volCopyright.extLocation = le32_to_cpu(0);
	pvd->volCopyright.extLength = le32_to_cpu(0);
	memset(&pvd->appIdent, 0x00, sizeof(EntityID));
	pvd->recordingDateAndTime = crtime;
	pvd->impIdent.flags = 0;
	strcpy(pvd->impIdent.ident, UDF_ID_DEVELOPER);
	pvd->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	pvd->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
	pvd->predecessorVolDescSeqLocation = le32_to_cpu(19);
	pvd->flags = le16_to_cpu(0x0001);
	pvd->descTag = query_tag(TID_PRIMARY_VOL_DESC, 2, 1, loc, pvd, totsize);

	udf_write_data(opt, loc, pvd, sizeof(struct PrimaryVolDesc), "Primary Volume descriptor");
	free(pvd);
}

void
write_logicalvoldesc(write_func udf_write_data, mkudf_options *opt, int loc, int snum, int start, int len, int filesetpart, int filesetblock, int spartable1, int spartable2, int sparnum)
{
	struct LogicalVolDesc *lvd;
	struct GenericPartitionMap1 *gpm1;
	struct SparablePartitionMap *spm;
	long_ad *fsd;
	size_t totsize;
	Uint8 maplen = 0;
	Uint32 totmaplen;

	if (opt->partition == PT_NORMAL)
		maplen = 6;
	else if (opt->partition == PT_SPARING)
		maplen = 64;

	totsize = sizeof(struct LogicalVolDesc) + maplen;
	lvd = calloc(1, totsize);

	if (opt->partition == PT_NORMAL)
	{
		gpm1 = (struct GenericPartitionMap1 *)&(lvd->partitionMaps[0]);
		/* type 1 only */ /* 2 2.2.8, 2.2.9 */
		gpm1->partitionMapType = 1;
		gpm1->partitionMapLength = maplen;
		gpm1->volSeqNum = le16_to_cpu(1);
		gpm1->partitionNum = le16_to_cpu(0);
	}
	else
	{
		spm = (struct SparablePartitionMap *)&(lvd->partitionMaps[0]);
		spm->partitionMapType = 2;
		spm->partitionMapLength = maplen;
		spm->partIdent.flags = 0;
		strncpy(spm->partIdent.ident, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE));
		((Uint16 *)spm->partIdent.identSuffix)[0] = le16_to_cpu(0x0150);
		spm->partIdent.identSuffix[2] = UDF_OS_CLASS_UNIX;
		spm->partIdent.identSuffix[3] = UDF_OS_ID_LINUX;
		spm->volSeqNum = le16_to_cpu(1);
		spm->partitionNum = le16_to_cpu(0);
		spm->packetLength = le16_to_cpu(32);
		spm->numSparingTables = 2;
		spm->sizeSparingTable = sizeof(struct SparingTable) + sparnum * sizeof(SparingEntry);
		spm->locSparingTable[0] = spartable1;
		spm->locSparingTable[1] = spartable2;
	}

	totmaplen = maplen;

	lvd->volDescSeqNum = le32_to_cpu(snum);

	lvd->descCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(lvd->descCharSet.charSetInfo, UDF_CHAR_SET_INFO);

	memcpy(&lvd->logicalVolIdent, opt->lvol_id, sizeof(opt->lvol_id));

	lvd->logicalBlockSize = le32_to_cpu(opt->blocksize);

	lvd->domainIdent.flags = 0;
	strcpy(lvd->domainIdent.ident, UDF_ID_COMPLIANT);
	((Uint16 *)lvd->domainIdent.identSuffix)[0] = le16_to_cpu(0x0150);
	lvd->domainIdent.identSuffix[2] = 0x00;

	fsd = (long_ad *)lvd->logicalVolContentsUse;
	fsd->extLength = le32_to_cpu(opt->blocksize);
	fsd->extLocation.logicalBlockNum = le32_to_cpu(filesetblock);
	fsd->extLocation.partitionReferenceNum = le16_to_cpu(filesetpart);

	lvd->mapTableLength = le32_to_cpu(totmaplen);
	lvd->numPartitionMaps = le32_to_cpu(1);

	lvd->impIdent.flags = 0;
	strcpy(lvd->impIdent.ident, UDF_ID_DEVELOPER);
	lvd->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	lvd->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	lvd->integritySeqExt.extLocation = le32_to_cpu(start);
	lvd->integritySeqExt.extLength = le32_to_cpu(len * opt->blocksize);

	lvd->descTag = query_tag(TID_LOGICAL_VOL_DESC, 2, 1, loc, lvd, totsize);
	udf_write_data(opt, loc, lvd, totsize, "Logical Volume descriptor");
	free(lvd);
}

void write_logicalvolintdesc(write_func udf_write_data, mkudf_options *opt, int loc, timestamp crtime, int part0start)
{
	int i;
	struct LogicalVolIntegrityDesc *lvid;
	struct LogicalVolIntegrityDescImpUse *lvidiu;
	size_t totsize;
	Uint32 npart = 1;
	Uint32 spaceused = ((sizeof(struct SpaceBitmapDesc) +
		(((opt->blocks-1)-288-part0start)/8 + 1) * sizeof(Uint8) + opt->blocksize - 1) >>
		opt->blocksize_bits) + 3;

	totsize = sizeof(struct LogicalVolIntegrityDesc) + sizeof(Uint32) * npart + sizeof(Uint32) * npart + sizeof(struct LogicalVolIntegrityDescImpUse);
	lvid = calloc(1, totsize);

	for (i=0; i<npart; i++)
		lvid->freeSpaceTable[i] = le32_to_cpu(1+(opt->blocks-1)-288-part0start-spaceused);
	for (i=0; i<npart; i++)
		lvid->sizeTable[i+npart] = le32_to_cpu(1+(opt->blocks-1)-288-part0start);
	lvidiu = (struct LogicalVolIntegrityDescImpUse *)&(lvid->impUse[npart*2*sizeof(Uint32)/sizeof(Uint8)]);

	lvidiu->impIdent.flags = 0;
	strcpy(lvidiu->impIdent.ident, UDF_ID_DEVELOPER);
	lvidiu->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	lvidiu->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	lvidiu->numFiles = le32_to_cpu(0);
	lvidiu->numDirs = le32_to_cpu(2);
	lvidiu->minUDFReadRev = le16_to_cpu(0x0150);
	lvidiu->minUDFWriteRev = le16_to_cpu(0x0150);
	lvidiu->maxUDFWriteRev = le16_to_cpu(0x0150);

	lvid->recordingDateAndTime = crtime;
	lvid->integrityType = le32_to_cpu(INTEGRITY_TYPE_CLOSE);
	lvid->nextIntegrityExt.extLocation = le32_to_cpu(0);
	lvid->nextIntegrityExt.extLength = le32_to_cpu(0);
	((Uint64 *)lvid->logicalVolContentsUse)[0] = le64_to_cpu(17); /* Max Unique ID */
	lvid->numOfPartitions = le32_to_cpu(npart);
	lvid->lengthOfImpUse = le32_to_cpu(sizeof(struct LogicalVolIntegrityDescImpUse));

	lvid->descTag = query_tag(TID_LOGICAL_VOL_INTEGRITY_DESC, 2, 1, loc, lvid, totsize);
	udf_write_data(opt, loc, lvid, totsize, "Logical Volume Integrity descriptor");
	free(lvid);
}

void write_partitionvoldesc(write_func udf_write_data, mkudf_options *opt, int loc, int snum, int start, int end, int usb)
{
	struct PartitionDesc *pd;
	struct PartitionHeaderDesc *phd;
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
	phd->unallocatedSpaceBitmap.extLength = le32_to_cpu(
		((((sizeof(struct SpaceBitmapDesc)+end-start) >> (opt->blocksize_bits + 3)) + 1)
		<< opt->blocksize_bits) | 0x40000000);
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
	udf_write_data(opt, loc, pd, totsize, "Partition descriptor");
	free(pd);
}

void write_unallocatedspacedesc(write_func udf_write_data, mkudf_options *opt, int loc, int snum, int start1, int end1, int start2, int end2)
{
	struct UnallocatedSpaceDesc *usd;
	extent_ad *ead;
	size_t totsize;
	Uint32 ndesc = 2;

	totsize = sizeof(struct UnallocatedSpaceDesc) + sizeof(extent_ad) * ndesc;
	usd = calloc(1, totsize);

	ead = &(usd->allocDescs[0]);
	ead->extLength = le32_to_cpu((1 + end1 - start1) * opt->blocksize);
	ead->extLocation = le32_to_cpu(start1);

	ead = &(usd->allocDescs[1]);
	ead->extLength = le32_to_cpu((1 + end2 - start2) * opt->blocksize);
	ead->extLocation = le32_to_cpu(start2);
 
	usd->volDescSeqNum = le32_to_cpu(snum);
	usd->numAllocDescs = le32_to_cpu(ndesc);

	usd->descTag = query_tag(TID_UNALLOC_SPACE_DESC, 2, 1, loc, usd, totsize);
	udf_write_data(opt, loc, usd, totsize, "Unallocated Space descriptor");
	free(usd);
}

void write_impusevoldesc(write_func udf_write_data, mkudf_options *opt, int loc, int snum)
{
	struct ImpUseVolDesc *iuvd;
	struct ImpUseVolDescImpUse *iuvdiu;
	size_t totsize;
	struct ustr instr;

	totsize = sizeof(struct ImpUseVolDesc);
	iuvd = calloc(1, totsize);

	iuvdiu = (struct ImpUseVolDescImpUse *)&(iuvd->impUse[0]);

	iuvdiu->LVICharset.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(iuvdiu->LVICharset.charSetInfo, UDF_CHAR_SET_INFO);

	memcpy(iuvdiu->logicalVolIdent, opt->lvol_id, sizeof(opt->lvol_id));

	memset(&instr, 0, sizeof(struct ustr));
	snprintf(instr.u_name, 34, "mkudf (Linux UDF tools) %d.%d (%d)",
		VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
	instr.u_len = strlen(instr.u_name) + 1;
	udf_UTF8toCS0(iuvdiu->LVInfo1, &instr, 36);

	memset(&instr, 0, sizeof(struct ustr));
	snprintf(instr.u_name, 34, "Linux UDF %s (%s)",
		UDFFS_VERSION, UDFFS_DATE);
	instr.u_len = strlen(instr.u_name) + 1;
	udf_UTF8toCS0(iuvdiu->LVInfo2, &instr, 36);

	memset(&instr, 0, sizeof(struct ustr));
	snprintf(instr.u_name, 34, "%s",
		EMAIL_STRING);
	instr.u_len = strlen(instr.u_name) + 1;
	udf_UTF8toCS0(iuvdiu->LVInfo3, &instr, 36);

	iuvdiu->impIdent.flags = 0;
	strcpy(iuvdiu->impIdent.ident, UDF_ID_DEVELOPER);
	iuvdiu->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
	iuvdiu->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;

	iuvd->volDescSeqNum = le32_to_cpu(snum);

	iuvd->impIdent.flags = 0;
	strcpy(iuvd->impIdent.ident, UDF_ID_LV_INFO);
	((Uint16 *)iuvd->impIdent.identSuffix)[0] = le16_to_cpu(0x0150);
	iuvd->impIdent.identSuffix[2] = UDF_OS_CLASS_UNIX;
	iuvd->impIdent.identSuffix[3] = UDF_OS_ID_LINUX;

	iuvd->descTag = query_tag(TID_IMP_USE_VOL_DESC, 2, 1, loc, iuvd, totsize);
	udf_write_data(opt, loc, iuvd, totsize, "Imp Use Volume descriptor");
	free(iuvd);
}

void write_termvoldesc(write_func udf_write_data, mkudf_options *opt, int loc)
{
	struct TerminatingDesc *td;
	size_t totsize;

	totsize = sizeof(struct TerminatingDesc);
	td = calloc(1, totsize);
	
	td->descTag = query_tag(TID_TERMINATING_DESC, 2, 1, loc, td, totsize);
	udf_write_data(opt, loc, td, totsize, "Terminating descriptor");
	free(td);
}

void write_filesetdesc(write_func udf_write_data, mkudf_options *opt, int loc, int sloc, int snum, int rootpart, int rootblock, timestamp crtime)
{
	struct FileSetDesc *fsd;
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
	memcpy(&fsd->logicalVolIdent, opt->lvol_id, sizeof(opt->lvol_id));
	fsd->fileSetCharSet.charSetType = UDF_CHAR_SET_TYPE;
	strcpy(fsd->fileSetCharSet.charSetInfo, UDF_CHAR_SET_INFO);
	memcpy(&fsd->fileSetIdent, opt->file_set_id, sizeof(opt->file_set_id));
/*
	copyrightFileIdent[32]
	abstractFileIdent
*/
	fsd->rootDirectoryICB.extLength = le32_to_cpu(opt->blocksize);
	fsd->rootDirectoryICB.extLocation.logicalBlockNum = le32_to_cpu(rootblock);
	fsd->rootDirectoryICB.extLocation.partitionReferenceNum = le16_to_cpu(rootpart);
	fsd->domainIdent.flags = 0;
	strcpy(fsd->domainIdent.ident, UDF_ID_COMPLIANT);
	((Uint16 *)fsd->domainIdent.identSuffix)[0] = le16_to_cpu(0x0150);
	fsd->domainIdent.identSuffix[2] = 0x00;
/*
	nextExt
	StreamDirectoryICB
*/
	
	fsd->descTag = query_tag(TID_FILE_SET_DESC, 2, snum, loc - sloc, fsd, totsize);
	udf_write_data(opt, loc, fsd, totsize, "File Set descriptor");
	free(fsd);
}

void write_spartable(write_func udf_write_data, mkudf_options *opt, int loc, int snum, int start, int num)
{
	struct SparingTable *st;
	size_t totsize;
	int i;

	totsize = sizeof(struct SparingTable) + num * sizeof(SparingEntry);
	st = calloc(1, totsize);
	st->sparingIdent.flags = 0;
	strncpy(st->sparingIdent.ident, UDF_ID_SPARING, strlen(UDF_ID_SPARING));
	((Uint16 *)st->sparingIdent.identSuffix)[0] = le16_to_cpu(0x0150);
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
	udf_write_data(opt, loc, st, totsize, "Sparing Table");
	free(st);
}

void write_spacebitmapdesc(write_func udf_write_data, mkudf_options *opt, int loc, int sloc, int snum, int start, int end, int fileset)
{
	struct SpaceBitmapDesc *sbd;
	size_t totsize;
	int i;
	Uint32 nbytes = (end-start)/8+1;

	totsize = sizeof(struct SpaceBitmapDesc) + sizeof(Uint8) * nbytes;
	sbd = calloc(1, totsize);
	
	sbd->numOfBits = le32_to_cpu(end-start+1);
	sbd->numOfBytes = le32_to_cpu(nbytes);
	memset(sbd->bitmap, 0xFF, sizeof(Uint8) * nbytes);
	for (i=0; i<(totsize + opt->blocksize - 1) >> opt->blocksize_bits; i++)
		sbd->bitmap[i/8] &= ~(1 << i%8);
	sbd->bitmap[fileset/8] &= ~(1 << (fileset%8));
	sbd->bitmap[(fileset+32)/8] &= ~(1 << ((fileset+32)%8));
	sbd->bitmap[(fileset+33)/8] &= ~(1 << ((fileset+33)%8));
	if ((end-start+1)%8)
		sbd->bitmap[nbytes-1] = 0x0FF >> (8-((end-start+1)%8));

	sbd->descTag = query_tag(TID_SPACE_BITMAP_DESC, 2, snum, loc - sloc, sbd, sizeof(tag));
	udf_write_data(opt, loc, sbd, totsize, "Space Bitmap descriptor");
	free(sbd);
}

void write_fileentry1(write_func udf_write_data, mkudf_options *opt, int loc, int sloc, int snum, int part, int block, timestamp crtime)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	size_t totsize;
	Uint32 leattr = 0;
#if 1
	Uint32 filelen2 = 11;
#else
	char foo[] = {	0x10,\
					0x65, 0xE5, 0x67, 0x2C, 0x8A, 0x9E, 0x30, 0xC7,\
					0x30, 0xA3, 0x30, 0xEC, 0x30, 0xEC, 0x30, 0xAF,\
					0x30, 0xAF };
	Uint32 filelen2 = sizeof(foo);
#endif
	Uint32 ladesc1 = compute_ident_length(sizeof(struct FileIdentDesc));
	Uint32 ladesc2 = compute_ident_length(sizeof(struct FileIdentDesc) + filelen2);

	totsize = sizeof(struct FileEntry) + ladesc1 + ladesc2;
	fe = calloc(1, totsize);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = le16_to_cpu(1);
	fid->fileCharacteristics = 0x0A; /* 0000 1010 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = le32_to_cpu(opt->blocksize);
	fid->icb.extLocation.logicalBlockNum = cpu_to_le32(block);
	fid->icb.extLocation.partitionReferenceNum = cpu_to_le16(part);
	*(Uint32 *)((struct ADImpUse *)fid->icb.impUse)->impUse = cpu_to_le32(0);
	fid->lengthOfImpUse = le16_to_cpu(0);
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc1);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr + ladesc1]);
	fid->fileVersionNum = le16_to_cpu(1);
	fid->fileCharacteristics = 0x02; /* 0000 0010 */
	fid->lengthFileIdent = filelen2;
	fid->icb.extLength = le32_to_cpu(opt->blocksize);
	fid->icb.extLocation.logicalBlockNum = le32_to_cpu(block+1);
	fid->icb.extLocation.partitionReferenceNum = le16_to_cpu(part);
	*(Uint32 *)((struct ADImpUse *)fid->icb.impUse)->impUse = cpu_to_le32(0);
	fid->lengthOfImpUse = le16_to_cpu(0);
#if 1
	strcpy(&fid->fileIdent[0], " lost+found");
	fid->fileIdent[0] = 8;
#else
	printf("len=%d\n", sizeof(foo));
	memcpy(&fid->fileIdent, foo, sizeof(foo));
#endif
	fid->descTag = query_tag(TID_FILE_IDENT_DESC, 2, snum, loc - sloc, fid, ladesc2);

	fe->uid = le32_to_cpu(0);
	fe->gid = le32_to_cpu(0);
	fe->permissions = le32_to_cpu(PERM_U_DELETE|PERM_U_CHATTR|PERM_U_READ|PERM_U_WRITE|PERM_U_EXEC|PERM_G_READ|PERM_G_EXEC|PERM_O_READ|PERM_O_EXEC);
	fe->fileLinkCount = le16_to_cpu(2);
	fe->recordFormat = 0;
	fe->recordDisplayAttr = 0;
	fe->recordLength = le32_to_cpu(0);
	fe->informationLength = le64_to_cpu(ladesc1 + ladesc2 + leattr);
	fe->logicalBlocksRecorded = le64_to_cpu(0);
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

	fe->uniqueID = le64_to_cpu(0);
	fe->lengthExtendedAttr = le32_to_cpu(0);
	fe->lengthAllocDescs = le32_to_cpu(ladesc1 + ladesc2);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_DIRECTORY, 0, 0, ICB_FLAG_AD_IN_ICB);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	udf_write_data(opt, loc, fe, totsize, "File Entry");
	free(fe);
}

void write_fileentry2(write_func udf_write_data, mkudf_options *opt, int loc, int sloc, int snum, int part, int block, timestamp crtime)
{
	struct FileEntry *fe;
	struct FileIdentDesc *fid;
	size_t totsize;
	Uint32 leattr = 0;
	Uint32 ladesc1 = compute_ident_length(sizeof(struct FileIdentDesc));

	totsize = sizeof(struct FileEntry) + ladesc1;
	fe = calloc(1, totsize);

	fid = (struct FileIdentDesc *)&(fe->allocDescs[leattr]);
	fid->fileVersionNum = le16_to_cpu(1);
	fid->fileCharacteristics = 0x0A; /* 0000 1010 */
	fid->lengthFileIdent = 0;
	fid->icb.extLength = le32_to_cpu(opt->blocksize);
	fid->icb.extLocation.logicalBlockNum = le32_to_cpu(block);
	fid->icb.extLocation.partitionReferenceNum = le16_to_cpu(part);
	*(Uint32 *)((struct ADImpUse *)fid->icb.impUse)->impUse = cpu_to_le32(0);
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
	fe->logicalBlocksRecorded = le64_to_cpu(0);
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
	fe->lengthAllocDescs = le32_to_cpu(ladesc1);

	fe->icbTag = query_icbtag(0, 4, 0, 1, FILE_TYPE_DIRECTORY, 2, 0, ICB_FLAG_AD_IN_ICB);
	fe->descTag = query_tag(TID_FILE_ENTRY, 2, snum, loc - sloc, fe, totsize);
	udf_write_data(opt, loc, fe, totsize, "File Entry");
	free(fe);
}

timestamp query_timestamp(struct timeval *tv, struct timezone *tz)
{
	timestamp ret;
	struct tm *tm;

	tm = localtime(&tv->tv_sec);

	ret.typeAndTimezone = le16_to_cpu(((-tz->tz_minuteswest) & 0x0FFF) | 0x1000);

	printf("%ld (%s)/%d\n", tm->tm_gmtoff, tm->tm_zone, tz->tz_minuteswest);
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
mkudf(write_func udf_write_data, mkudf_options *opt)
{
	Uint32 pvd, pvd_len, rvd, rvd_len, snum, lvd, lvd_len;
	Uint32 filesetpart, filesetblock, part0start;
	Uint32 sparstart = 0, spartable1 = 0, spartable2 = 0, sparnum = 0;
	Uint32 min_blocks = 0;
	struct timeval tv;
	struct timezone tz;
	timestamp crtime;

	gettimeofday(&tv, &tz);
	crtime = query_timestamp(&tv, &tz);

	pvd = 0x120;
	pvd_len = 0x10;
	rvd = 0x140;
	rvd_len = 0x10;
	snum = 0;
	lvd = 0x160;
	lvd_len = 2;

	filesetpart = 0;
	if (opt->partition == PT_SPARING)
	{
		if (opt->blocks % 32)
		{
			printf("mkudf: spared partitions must be a multiple of 32 blocks in size.\n");
			return -2;
		}
		part0start = 2464;
		sparstart = 1408;
		sparnum = 32;
		spartable1 = 2432;
		spartable2 = opt->blocks-256;
		min_blocks = 2818;
	}
	else if (opt->partition == PT_VAT)
	{
		part0start = 384;
		min_blocks = 738;
	}
	else
	{
		part0start = 1408;
		min_blocks = 1762;
	}

	if (opt->blocks < min_blocks)
	{
		printf("mkudf: min blocks=%d\n", min_blocks);
		return -1;
	}

	filesetblock =
		(sizeof(struct SpaceBitmapDesc) +
		(((opt->blocks-1)-288-part0start)/8 + 1) * sizeof(Uint8) + opt->blocksize - 1) >>
			opt->blocksize_bits;

	if (opt->partition == PT_SPARING || opt->partition == PT_NORMAL)
		filesetblock = ((filesetblock + 31) / 32) * 32;

	write_vrs(udf_write_data, opt, 32768 );
	write_anchor(udf_write_data, opt, 256, pvd, pvd_len, rvd, rvd_len);

	write_primaryvoldesc(udf_write_data, opt, pvd, ++snum, crtime);
	write_logicalvoldesc(udf_write_data, opt, pvd+1, ++snum, lvd, lvd_len, filesetpart,
		filesetblock, spartable1, spartable2, sparnum);
	write_partitionvoldesc(udf_write_data, opt, pvd+2, ++snum, part0start, (opt->blocks-1)-288, part0start);
	if (opt->partition == PT_SPARING || opt->partition == PT_NORMAL)
		write_unallocatedspacedesc(udf_write_data, opt, pvd+3, ++snum, 64, 255, 384, 1407);
	else
		write_unallocatedspacedesc(udf_write_data, opt, pvd+3, ++snum, 64, 255, 0, 0);
	write_impusevoldesc(udf_write_data, opt, pvd+4, ++snum);
	write_termvoldesc(udf_write_data, opt, pvd+5);

	write_primaryvoldesc(udf_write_data, opt, rvd, snum, crtime);
	write_logicalvoldesc(udf_write_data, opt, rvd+1, snum, lvd, lvd_len, filesetpart,
		filesetblock, spartable1, spartable2, sparnum);
	write_partitionvoldesc(udf_write_data, opt, rvd+2, snum, part0start, (opt->blocks-1)-288, part0start);
	if (opt->partition == PT_SPARING || opt->partition == PT_NORMAL)
		write_unallocatedspacedesc(udf_write_data, opt, rvd+3, snum, 64, 255, 384, 1407);
	else
		write_unallocatedspacedesc(udf_write_data, opt, rvd+3, snum, 64, 255, 0, 0);
	write_impusevoldesc(udf_write_data, opt, rvd+4, snum);
	write_termvoldesc(udf_write_data, opt, rvd+5);

	write_logicalvolintdesc(udf_write_data, opt, lvd, crtime, part0start);
	write_termvoldesc(udf_write_data, opt, lvd+1);

	if (opt->partition == PT_SPARING)
		write_spartable(udf_write_data, opt, spartable1, 1, sparstart, sparnum);
	write_spacebitmapdesc(udf_write_data, opt, part0start, part0start, 1, part0start, (opt->blocks-1)-288, filesetblock);
	write_filesetdesc(udf_write_data, opt, part0start+filesetblock, part0start, 1, 0, filesetblock+32, crtime);
	write_fileentry1(udf_write_data, opt, part0start+filesetblock+32, part0start, 1, 0, filesetblock+32, crtime);
	write_fileentry2(udf_write_data, opt, part0start+filesetblock+33, part0start, 1, 0, filesetblock+32, crtime);
	write_anchor(udf_write_data, opt, (opt->blocks-1)-256, pvd, pvd_len, rvd, rvd_len);
	if (opt->partition == PT_SPARING)
		write_spartable(udf_write_data, opt, spartable2, 1, sparstart, sparnum);
	write_anchor(udf_write_data, opt, (opt->blocks-1), pvd, pvd_len, rvd, rvd_len);
	return 0;
}
