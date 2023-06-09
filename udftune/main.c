/*
 * Copyright (C) 2017-2021  Pali Rohár <pali.rohar@gmail.com>
 * Copyright (C) 2023  Johannes Truschnigg <johannes@truschnigg.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/fs.h>
#include <sys/ioctl.h>

#include "libudffs.h"
#include "options.h"
#include "updatedisc.h"
#include "../udfinfo/readdisc.h"

int main(int argc, char *argv[])
{
	struct udf_disc disc;
	char *filename;
	struct partitionDesc *pd;
	struct domainIdentSuffix *dis;
	int fd;
	int flags;
	char buf[256];
	dstring new_vid[32];
	dstring new_fsid[32];
	int mark_ro = 0;
	int mark_rw = 0;
	int force = 0;
	int update = 0;
	int update_lvd = 0;
	int update_fsd = 0;
	int update_vid = 0;

	if (fcntl(0, F_GETFL) < 0 && open("/dev/null", O_RDONLY) < 0)
		_exit(1);
	if (fcntl(1, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);
	if (fcntl(2, F_GETFL) < 0 && open("/dev/null", O_WRONLY) < 0)
		_exit(1);

	appname = "udftune";

	memset(&disc, 0, sizeof(disc));

	disc.head = calloc(1, sizeof(struct udf_extent));
	if (!disc.head)
	{
		fprintf(stderr, "%s: Error: calloc failed: %s\n", appname, strerror(errno));
		exit(1);
	}

	disc.start_block = (uint32_t)-1;
	disc.flags = FLAG_LOCALE;
	disc.tail = disc.head;
	disc.head->space_type = USPACE;

	memset(new_vid, 0, sizeof(new_vid));
	memset(new_fsid, 0, sizeof(new_fsid));

	new_vid[0] = 0xFF;
	new_fsid[0] = 0xFF;

	parse_args(argc, argv, &disc, &filename, &force, &mark_ro, &mark_rw);

	if (mark_ro != 0 || mark_rw != 0)
	{
		update_lvd = 1;
		update_fsd = 1;
	}

	if (update_lvd || update_fsd)
		update = 1;

	if (update && !(disc.flags & FLAG_NO_WRITE))
		flags = O_RDWR | O_EXCL;
	else
		flags = O_RDONLY | O_EXCL;

	fd = open_existing_disc(&disc, filename, flags, update, buf);
	if (fd < 0)
		exit(1);

	if (disc.udf_pd[0])
		pd = disc.udf_pd[0];
	else if (disc.udf_pd[1])
		pd = disc.udf_pd[1];
	else
	{
		fprintf(stderr, "%s: Error: Both Main and Reserve Partition Descriptor are damaged\n", appname);
		exit(1);
	}

	if (!check_access_type(&disc, pd, (char *)appname, force, update_vid))
		exit(1);

	/* TODO: VAT mode */
	if ((le32_to_cpu(pd->accessType) == PD_ACCESS_TYPE_WRITE_ONCE && !force) || (disc.vat && (new_fsid[0] != 0xFF)))
	{
		fprintf(stderr, "%s: Error: Updating Virtual Allocation Table is not supported yet\n", appname);
		exit(1);
	}

	/* TODO: Pseudo OverWrite mode */
	if (le32_to_cpu(pd->accessType) == PD_ACCESS_TYPE_NONE && !force)
	{
		fprintf(stderr, "%s: Error: Updating pseudo-overwrite partition is not supported yet\n", appname);
		exit(1);
	}

	if (disc.udf_lvd[0] && update_lvd)
	{
		if (!check_wr_lvd(&disc, (char *)appname, force)) exit(1);
	}

	if (disc.udf_fsd && update_fsd)
	{
		if (!check_wr_fsd(&disc, (char *)appname, force)) exit(1);
	}

	if (update_lvd)
	{
		if (!verify_lvd(&disc, (char *)appname))
			exit(1);
	}

	if (update_fsd)
	{
		if (!verify_fsd(&disc, (char *)appname))
			exit(1);
	}

	if (mark_ro != 0)
	{
		printf("Setting FSD SOFT_WRITE_PROTECT Flag...\n");
		dis = (struct domainIdentSuffix *)disc.udf_fsd->domainIdent.identSuffix;
		dis->domainFlags |= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;

		printf("Setting LVD SOFT_WRITE_PROTECT Flag...\n");
		dis = (struct domainIdentSuffix *)disc.udf_lvd[0]->domainIdent.identSuffix;
		dis->domainFlags |= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;
		dis = (struct domainIdentSuffix *)disc.udf_lvd[1]->domainIdent.identSuffix;
		dis->domainFlags |= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;
	}

	if (mark_rw != 0)
	{
		printf("Removing FSD HARD_WRITE_PROTECT Flag...\n");
		dis = (struct domainIdentSuffix *)disc.udf_fsd->domainIdent.identSuffix;
		dis->domainFlags &= DOMAIN_FLAGS_HARD_WRITE_PROTECT;
		printf("Removing FSD SOFT_WRITE_PROTECT Flag...\n");
		dis = (struct domainIdentSuffix *)disc.udf_fsd->domainIdent.identSuffix;
		dis->domainFlags &= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;

		printf("Removing LVD HARD_WRITE_PROTECT Flag...\n");
		dis = (struct domainIdentSuffix *)disc.udf_lvd[0]->domainIdent.identSuffix;
		dis->domainFlags &= DOMAIN_FLAGS_HARD_WRITE_PROTECT;
		dis = (struct domainIdentSuffix *)disc.udf_lvd[1]->domainIdent.identSuffix;
		dis->domainFlags &= DOMAIN_FLAGS_HARD_WRITE_PROTECT;
		printf("Removing LVD SOFT_WRITE_PROTECT Flag...\n");
		dis = (struct domainIdentSuffix *)disc.udf_lvd[0]->domainIdent.identSuffix;
		dis->domainFlags &= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;
		dis = (struct domainIdentSuffix *)disc.udf_lvd[1]->domainIdent.identSuffix;
		dis->domainFlags &= DOMAIN_FLAGS_SOFT_WRITE_PROTECT;
	}

	if (update_lvd)
	{
		printf("Updating Main Logical Volume Descriptor...\n");
		update_desc(disc.udf_lvd[0], sizeof(*disc.udf_lvd[0]) + le32_to_cpu(disc.udf_lvd[0]->mapTableLength));
		write_desc(fd, &disc, MVDS, TAG_IDENT_LVD, disc.udf_lvd[0]);
	}

	if (update_fsd)
	{
		printf("Updating File Set Descriptor...\n");
		update_desc(disc.udf_fsd, sizeof(*disc.udf_fsd));
		write_desc(fd, &disc, PSPACE, TAG_IDENT_FSD, disc.udf_fsd);
	}

	if (update_lvd && disc.udf_lvd[1] != disc.udf_lvd[0])
	{
		printf("Updating Reserve Logical Volume Descriptor...\n");
		update_desc(disc.udf_lvd[1], sizeof(*disc.udf_lvd[1]) + le32_to_cpu(disc.udf_lvd[1]->mapTableLength));
		write_desc(fd, &disc, RVDS, TAG_IDENT_LVD, disc.udf_lvd[1]);
	}

	if (!sync_device(&disc, fd, (char *)appname, (char *)filename)) return 1;
	return 0;
}
