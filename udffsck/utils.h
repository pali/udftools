/*
 * utils.h
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
#ifndef __UTILS_H__
#define __UTILS_H__

#include "config.h"

#include <ecma_167.h>
#include <libudffs.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "udffsck.h"

int print_disc(struct udf_disc *disc);
int prompt(const char *format, ...);

void print_metadata_sequence(vds_sequence_t *seq);

#if MEMTRACE

void *custom_malloc(size_t size, char * file, int line);
void custom_free(void *ptr, char * file, int line);
int custom_munmap(void *addr, size_t length, char * file, int line);
void *custom_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset, char * file, int line);

#define malloc(size) custom_malloc(size, __FILE__, __LINE__)
#define free(ptr) custom_free(ptr, __FILE__, __LINE__)
#define mmap(addr, length, prot, flags, fd, offset) custom_mmap(addr, length, prot, flags, fd, offset, __FILE__, __LINE__)
#define munmap(addr, length) custom_munmap(addr, length, __FILE__, __LINE__)

#endif

#endif //__UTILS_H__
