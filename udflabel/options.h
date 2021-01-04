/*
 * Copyright (C) 2017-2021  Pali Rohár <pali.rohar@gmail.com>
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

#ifndef OPTIONS_H
#define OPTIONS_H

struct udf_disc;

void parse_args(int, char *[], struct udf_disc *, char **, int *, dstring *, dstring *, dstring *, dstring *, char *, dstring *, dstring *, dstring *, dstring *, char *, char *);

/*
 * Command line option token values.
 *      0x0000-0x00ff   Single characters
 *      0x1000-0x1fff   Long switches (no arg)
 *      0x2000-0x2fff   Long settings (arg required)
 */

#define OPT_HELP	0x1000
#define OPT_LOCALE	0x1001
#define OPT_UNICODE8	0x1002
#define OPT_UNICODE16	0x1003
#define OPT_UTF8	0x1004
#define OPT_FORCE	0x1005
#define OPT_NO_WRITE	0x1006

#define OPT_BLK_SIZE	0x2000
#define OPT_VAT_BLOCK	0x2001
#define OPT_UUID	0x2002
#define OPT_LVID	0x2003
#define OPT_VID		0x2004
#define OPT_VSID	0x2005
#define OPT_FSID	0x2006
#define OPT_FULLVSID	0x2007
#define OPT_START_BLOCK	0x2008
#define OPT_LAST_BLOCK	0x2009
#define OPT_OWNER	0x200A
#define OPT_ORG		0x200B
#define OPT_CONTACT	0x200C
#define OPT_APPID	0x200D
#define OPT_IMPID	0x200E

#endif /* OPTIONS_H */
