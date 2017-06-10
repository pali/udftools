/*
 * log.h
 *
 * Copyright (c) 2017    Vojtech Vladyka <vojtch.vladyka@gmail.com>
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
#ifndef __LOG_G__
#define __LOG_G__

#include "config.h"

#include <ecma_167.h>
#include <libudffs.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "udffsck.h"

typedef enum {
    NONE=0,
    WARN,
    MSG,
    DBG
} verbosity_e;

extern verbosity_e verbosity;

void dbg(const char *format, ...);
void dwarn(const char *format, ...);
void note(const char *format, ...);
void msg(const char *format, ...);
void imp(const char *format, ...);
void warn(const char *format, ...);
void err(const char *format, ...);
void fatal(const char *format, ...);

char * verbosity_level_str(verbosity_e lvl);


#endif //__LOG_H__
