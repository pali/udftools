#include <stdio.h>

int main(int argc, char **argv)
{
	char data[2048];
	FILE *fp;
	int i;

	memset(data, 0xFF, 2048);

	if (argc < 0)
	{
		printf("Usage: %s <file>\n", argv[0]);
		return 1;
	}
	fp = fopen(argv[1], "w");
	fseek(fp, 201*2048, SEEK_SET);
	fwrite(data, 2048, 1, fp);
	fseek(fp, 0*2048, SEEK_SET);
	fwrite(data, 2048, 1, fp);
	for (i=200; i>85; i--)
	{
		fseek(fp, i*2048, SEEK_SET);
		fwrite(data, 2048, 1, fp);
	}
	fclose(fp);
}
