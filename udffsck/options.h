/*
 * Copyright (C) 2017 Vojtech Vladyka <vojtech.vladyka@gmail.com>
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

#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "config.h"

#include <getopt.h>
#include "utils.h"
#include "log.h"

void usage(void);
void parse_args(int, char *[], char **path, int *blocksize/*, struct cdrw_disc *, char **/);

extern int interactive;
extern int autofix;
extern verbosity_e verbosity;
extern int colored;
extern int fast_mode;

/*
 * Command line option token values.
 *      0x0000-0x00ff   Single characters
 *      0x1000-0x1fff   Long switches (no arg)
 *      0x2000-0x2fff   Long settings (arg required)
 */

#define OPT_HELP        0x1000

#endif /* _OPTIONS_H */
