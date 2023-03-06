#ifndef __ARGV_H__
#define __ARGV_H__

#include "dtype.h"

// option flags
#define OF_VALUE	(1<<0)	// value required
#define OF_LONG		(1<<1)	// flag for long option (lead by --)
#define OF_PRESENT	(1<<2)	// flag for presence by default

typedef struct {
	_cstr_t	opt_name;
	_u32	opt_flags;
	_str_t	opt_value; // default value
	_cstr_t	opt_help;
}_argv_t;

#ifdef __cplusplus
extern "C" {
#endif
_bool argv_parse(_s32 argc, _cstr_t argv[], _argv_t *opt_map);
_bool argv_check(_cstr_t opt);
_cstr_t argv_value(_cstr_t opt);
_cstr_t argv_get(_u32 idx);
_s32 argv_read_stdin(_uchar_t *buffer, _u32 sz_buffer, _u8 timeout_sec);
#ifdef __cplusplus
}
#endif


#endif
