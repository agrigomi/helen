#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdio.h>

#if _TRACE_
#define TRACE(...)      { fprintf_timestamp(stderr); fprintf(stderr, __VA_ARGS__); }
#define TRACEb(ptr, n)	fprintf_bytes(stderr, ptr, n)
#if _DEBUG_
#define TRACEfl(...)    fprintf(stderr, "[%s] [%d] ", __FILE__, __LINE__);\
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

#define LOG(...)	fprintf(stderr, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif
int fprintf_bytes(FILE *stream, void *ptr, int n);
void fprintf_timestamp(FILE *stream);
#ifdef __cplusplus
}
#endif

#endif
