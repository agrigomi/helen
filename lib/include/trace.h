#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdio.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#ifdef __cplusplus
extern "C" {
#endif
int fprintf_bytes(FILE *stream, void *ptr, int n);
void fprintf_timestamp(FILE *stream);
void color_fprintf(FILE *stream, const char *color, const char *fmt, ...);
#ifdef __cplusplus
}
#endif


#if _TRACE_
#define TRACE(...)      { fprintf_timestamp(stderr); color_fprintf(stderr, KWHT, __VA_ARGS__); }
#define TRACE_INF(...)      { fprintf_timestamp(stderr); color_fprintf(stderr, KNRM, __VA_ARGS__); }
#define TRACE_WRN(...)      { fprintf_timestamp(stderr); color_fprintf(stderr, KCYN, __VA_ARGS__); }
#define TRACE_ERR(...)      { fprintf_timestamp(stderr); color_fprintf(stderr, KRED, __VA_ARGS__); }
#define TRACEb(ptr, n)	fprintf_bytes(stderr, ptr, n)
#if _DEBUG_
#define TRACEfl(...)    fprintf(stderr, KYEL "[%s] [%d] ", __FILE__, __LINE__);\
                        fprintf(stderr, __VA_ARGS__);
#define TRACE1(msg, val) fprintf(stderr, "[%s] [%d] ", __FILE__, __LINE__);\
			fprintf(stderr, "%s %s\n", msg, val)
#else //_DEBUG_
#define TRACEfl(...)  fprintf(stderr, __VA_ARGS__);
#define TRACE1(msg, val) fprintf(stderr, "%s %s\n", msg, val)
#endif // _DEBUG_
#else
#define TRACE(...)
#define TRACEb(ptr, n)
#define TRACEfl(...)
#define TRACE1(msg, val)
#endif // _TRACE_

#define LOG(...) { fprintf_timestamp(stderr); color_fprintf(stderr, KWHT, __VA_ARGS__); }
#define LOG_INF(...) { fprintf_timestamp(stderr); color_fprintf(stderr, KNRM, __VA_ARGS__); }
#define LOG_ERR(...) { fprintf_timestamp(stderr); color_fprintf(stderr, KRED, __VA_ARGS__); }
#define LOG_WRN(...) { fprintf_timestamp(stderr); color_fprintf(stderr, KCYN, __VA_ARGS__); }

#endif
