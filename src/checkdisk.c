#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#define RETRY_COUNT		2

int main(int argc, char **argv)
{
	int fd;
	int start, stop;
	ssize_t retval;
	char buf[2048];
	int i;
	int retry = 0;
	int last = 0;

	if (argc < 3)
	{
		printf("usage: checkdisk <device> <start> <stop>\n");
		return -1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
	{
		perror(argv[1]);
		return -1;
	}
	start = strtoul(argv[2], NULL, 0);
	stop = strtoul(argv[3], NULL, 0);

    retval = lseek(fd, start << 11L, SEEK_SET);

	for (i=start; i<stop; i++)
	{
		retval = read(fd, buf, 2048);
		if (retval != 2048)
		{
			if (retry == RETRY_COUNT)
			{
				printf("Read: %d, Failure = %d\n", i, retval);
				lseek(fd, 2048, SEEK_CUR);
				retry = 0;
				last = i;
			}
			else if (last == i-1)
			{
				retry ++;
				i --;
			}
			else
			{
				lseek(fd, -2048, SEEK_CUR);
				retry ++;
				i -= 2;
			}
		}
	}

	close(fd);
	return 0;
}
