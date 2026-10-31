#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
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

	fprintf(stream, "%s[", KNRM);
	fprintf(stream, "%s%02d%02d%02d %02d%02d%02d", KWHT,
		_tm->tm_year - 100, _tm->tm_mon + 1, _tm->tm_mday,
		_tm->tm_hour, _tm->tm_min, _tm->tm_sec);
	fprintf(stream, "%s] ", KNRM);
	fflush(stream);
}

void color_fprintf(FILE *stream, const char *color, const char *fmt, ...) {
	va_list va;
	char log_buffer[2048];
	int total_len = 0;

	/* 1. Generate timestamp string slice inside local stack memory */
	time_t t = time(NULL);
	struct tm *_tm = localtime(&t);

	total_len += snprintf(log_buffer + total_len, sizeof(log_buffer) - total_len,
		"%s[%s%02d%02d%02d %02d%02d%02d%s] ",
		KNRM, KWHT, _tm->tm_year - 100, _tm->tm_mon + 1, _tm->tm_mday,
		_tm->tm_hour, _tm->tm_min, _tm->tm_sec, KNRM);

	/* 2. Append the target layout ANSI escape color code */
	total_len += snprintf(log_buffer + total_len, sizeof(log_buffer) - total_len,
		"%s", color);

	/* 3. Append the actual structured log message arguments */
	va_start(va, fmt);
	total_len += vsnprintf(log_buffer + total_len, sizeof(log_buffer) - total_len,
		fmt, va);
	va_end(va);

	/* 4. Append the normal color reset token before pushing down the stream */
	total_len += snprintf(log_buffer + total_len, sizeof(log_buffer) - total_len,
		"%s", KNRM);

	/* 5. Trigger a single atomic low-level kernel write call */
	if (total_len > 0 && total_len < (int)sizeof(log_buffer)) {
		int fd = fileno(stream);
		if (fd != -1) {
			write(fd, log_buffer, (size_t)total_len);
		} else {
			/* Fallback to standard stream prints if fd extraction fails */
			fwrite(log_buffer, 1, (size_t)total_len, stream);
			fflush(stream);
		}
	}
}

