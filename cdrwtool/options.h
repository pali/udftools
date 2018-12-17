/*
 * options.h
 *
 * Copyright (c) 2002       Ben Fennema
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

#include <getopt.h>

void usage(void);
void parse_args(int, char *[], struct cdrw_disc *, const char **);

/*
 * Command line option token values.
 *      0x0000-0x00ff   Single characters
 *      0x1000-0x1fff   Long switches (no arg)
 *      0x2000-0x2fff   Long settings (arg required)
 */

#define OPT_HELP        0x1000

#endif /* _OPTIONS_H */
