#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/udf_167.h>

#if 0
                0000000000111111111122222222223333333333
                0123456789012345678901234567890123456789
#endif
char bfree[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ<>?|"
#if 0
                4444444444555555555566666666667777777777
                0123456789012345678901234567890123456789
#endif
               ")!@#$%^&*(abcdefghijklmnopqrstuvwxyz,./\\"
#if 0
                88888888 8899999   9   9
                01234567 8901234   5   6
#endif
               ";'[]-=:\"{}_+`~\003\002\001";

static unsigned char lookup[16] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

int main(int argc, char **argv)
{
	int fd;
	int sec;
	int retval;
	unsigned long fp;
	int blocks;
	char *sector;
	int bytes;

	if (argc < 3)
	{
		printf("usage: dump <device> <sector> <blocks>\n");
		return -1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
	{
		perror(argv[1]);
		return -1;
	}
	sec = strtoul(argv[2], NULL, 0);
	blocks = strtoul(argv[3], NULL, 0);

	fp = (sec << 11L) + sizeof(struct SpaceBitmapDesc);
	retval = lseek(fd, fp, SEEK_SET);
	if (retval < 0)
	{
		perror(argv[1]);
		return -1;
	}
	else
		printf("%d: seek to %ld ok, retval %u\n", sec, fp, retval);

	bytes = (blocks-1)/8 + 1;
	sector = malloc(bytes);
	retval = read(fd, sector, bytes);

	printf("        00000000011111111222222223333333334444444455555555\n"
           "        01234678902345689012456780123467890234568901245678\n"
           "        02468024680246802468024680246802468024680246802468\n");
	if (retval > 0)
	{
		int i, j, cnt;
		for (i=0; i<bytes; i+=12)
		{
			int acc = 0;
			if (!(i % 600))
			{
				printf("\n");
				printf("%6d: ", i);
			}
			cnt = bytes-i >= 12 ? 12 : bytes-i;
			for (j=0; j<cnt; j++)
			{
				acc += lookup[ sector[i+j] & 0x0f ];
				acc += lookup[ (sector[i+j] >> 4) & 0x0f ];
			}

			printf("%c", bfree[acc]);
		}
		printf("\n");
	}
	else
	{
		printf("**** errno=%d ****\n", errno);
	}
	close(fd);
	return 0;
}
