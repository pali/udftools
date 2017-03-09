#ifndef __UTILS_H__
#define __UTILS_H__

#include <ecma_167.h>
#include <libudffs.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int64_t udf_lseek64(int fd, int64_t offset, int whence);
int print_disc(struct udf_disc *disc);
int prompt(const char *format, ...);

void note(const char *format, ...);
void msg(const char *format, ...);
void imp(const char *format, ...);
void warn(const char *format, ...);
void err(const char *format, ...);
void fatal(const char *format, ...);

#endif //__UTILS_H__
