/*
 * options.h
 *
 * Copyright (c) 2001-2002  Ben Fennema
 * Copyright (c) 2014-2017  Pali Rohár <pali.rohar@gmail.com>
 * All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _OPTIONS_H
#define _OPTIONS_H 1

void usage(void);
void parse_args(int, char *[], struct udf_disc *, char **, int *, int *, int *);

/*
 * Command line option token values.
 *      0x0000-0x00ff   Single characters
 *      0x1000-0x1fff   Long switches (no arg)
 *      0x2000-0x2fff   Long settings (arg required)
 */

#define OPT_HELP	0x1000
#define OPT_NO_EFE	0x1001
#define OPT_LOCALE	0x1002
#define OPT_UNICODE8	0x1003
#define OPT_UNICODE16	0x1004
#define OPT_UTF8	0x1005
#define OPT_MEDIA_TYPE	0x1006
#define OPT_CLOSED	0x1007
#define OPT_VAT		0x1008
#define OPT_NEW_FILE	0x1009
#define OPT_NO_WRITE	0x1010

#define OPT_BLK_SIZE	0x2000
#define OPT_UDF_REV	0x2001
#define OPT_LVID	0x2002
#define OPT_VID		0x2003
#define OPT_VSID	0x2004
#define OPT_FSID	0x2005
#define OPT_STRATEGY	0x2006
#define OPT_SPARTABLE	0x2007
#define OPT_PACKETLEN	0x2008
#define OPT_SPARSPACE	0x2009
#define OPT_SPACE	0x200A
#define OPT_AD		0x200B
#define OPT_LABEL	0x200C
#define OPT_UUID	0x200D
#define OPT_FULLVSID	0x200E
#define OPT_UID		0x200F
#define OPT_GID		0x2010
#define OPT_MODE	0x2011
#define OPT_BOOTAREA	0x2012

#endif /* _OPTIONS_H */
