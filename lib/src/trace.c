#include <stdio.h>

int fprintf_bytes(FILE *stream, void *ptr, int n) {
	int i = 0;
	int r = 0;

	for(; i < n; i++) {
		if(i == (n - 1))
			r += fprintf(stream, "%02x\n", *((unsigned char *)ptr + i));
		else
			r += fprintf(stream, "%02x", *((unsigned char *)ptr + i));
	}

	return r;
}
