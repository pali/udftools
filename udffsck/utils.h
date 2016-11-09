#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/types.h>
#include <unistd.h>

int64_t udf_lseek64(int fd, int64_t offset, int whence);

#endif //__UTILS_H__
