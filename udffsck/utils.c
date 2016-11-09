#include "utils.h"


int64_t udf_lseek64(int fd, int64_t offset, int whence) {
#if defined(HAVE_LSEEK64)
	return lseek64(fd, offset, whence);
#elif defined(HAVE_LLSEEK)
	return llseek(fd, offset, whence);
#else
	return lseek(fd, offset, whence);
#endif
}
