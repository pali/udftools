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

#ifndef OPTIONS_H
#define OPTIONS_H

struct udf_disc;

void parse_args(int, char *[], struct udf_disc *, char **, int *, int *, int *);

/*
 * Command line option token values.
 *      0x0000-0x00ff   Single characters
 *      0x1000-0x1fff   Long switches (no arg)
 *      0x2000-0x2fff   Long settings (arg required)
 */

#define OPT_HELP	0x1000
#define OPT_FORCE	0x1001
#define OPT_NO_WRITE	0x1002
#define OPT_MARK_RO	0x1003
#define OPT_MARK_RW	0x1004
#define OPT_BLK_SIZE	0x2000
#define OPT_VAT_BLOCK	0x2001
#define OPT_START_BLOCK	0x2008
#define OPT_LAST_BLOCK	0x2009


#endif /* OPTIONS_H */
