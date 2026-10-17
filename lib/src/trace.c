#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "trace.h"

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

void fprintf_timestamp(FILE *stream) {
	time_t t = time(NULL);
	struct tm *_tm = localtime(&t);

	color_fprintf(stream, KNRM, "[");
	color_fprintf(stream, KWHT, "%02d%02d%02d %02d%02d%02d",
		_tm->tm_year - 100, _tm->tm_mon + 1, _tm->tm_mday,
		_tm->tm_hour, _tm->tm_min, _tm->tm_sec);
	color_fprintf(stream, KNRM, "] ");
}

void color_fprintf(FILE *stream, const char *color, const char *fmt, ...) {
	va_list va;

	va_start(va, fmt);

	fprintf(stream, color);
	vfprintf(stream, fmt, va);

	va_end(va);
}
