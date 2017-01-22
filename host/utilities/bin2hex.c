#include <stdio.h>

int main(int argc, char *argv[])
{
	FILE *handle;
	unsigned int c;
	unsigned long val;
	int i;
	
	handle = fopen(argv[1], "rb");
	if (handle == NULL) {
		fprintf(stderr, "Cannot open %s\n", argv[1]);
		return -1;
	}

	i = 0;
	while ((c = fgetc(handle)) != EOF) {
		if (i == 0) {
			val = 0;
		}
		val += ((c & 0xff) << (i << 3));
		if (i == 3) {
			printf("%08lx\n", val);
		}
		if (++i == 4) {
			i = 0;
		}
	}
	
	if (i != 0) {
		fprintf(stderr, "File not an integral number of 32 bit words\n");
		printf("%08lx\n", val);
		return -2;
	}
	return 0;
}

