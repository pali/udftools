#include <stdio.h>

int main(int argc, char **argv)
{
	char data[2048];
	FILE *fp, *fp2;
	int i;

	memset(data, 0xFF, 2048);

	if (argc < 3)
	{
		printf("Usage: %s <file1> <file2>\n", argv[0]);
		return 1;
	}
	fp = fopen(argv[1], "w+");
#if 0
	fp2 = fopen(argv[2], "w+");
#endif
#if 0
	fseek(fp, 201*2048, SEEK_SET);
	fwrite(data, 2048, 1, fp);
	fseek(fp, 0*2048, SEEK_SET);
	fwrite(data, 2048, 1, fp);
	for (i=200; i>85; i--)
	{
		fseek(fp, i*2048, SEEK_SET);
		fwrite(data, 2048, 1, fp);
	}
	ftruncate(fileno(fp), 400*2048);
	ftruncate(fileno(fp), 800*2048);
	fseek(fp, 799*2048, SEEK_SET);
	fwrite(data, 2048, 1, fp);
	fclose(fp);
#endif
#if 0
	fwrite(data, 2048, 1, fp);
	fwrite(data, 2048, 1, fp);
	fwrite(data, 2048, 1, fp);
	fwrite(data, 2048, 1, fp);
	fwrite(data, 2048, 1, fp);
	fwrite(data, 2048, 1, fp);
	fwrite(data, 2048, 1, fp);

	fwrite(data, 2048, 1, fp2);
	fclose(fp2);
	unlink(argv[2]);

	fseek(fp, 24*2048, SEEK_SET);
	fwrite(data, 2048, 1, fp);

	fseek(fp, 8*2048, SEEK_SET);
	fclose(fp);
#endif
	for (i=1; i<=200; i+=2)
	{
		fseek(fp, i*2048, SEEK_SET);
		fwrite(data, 2048, 1, fp);
	}
	for (i=0; i<200; i+=2)
	{
		fseek(fp, i*2048, SEEK_SET);
		fwrite(data, 2048, 1, fp);
	}
}
