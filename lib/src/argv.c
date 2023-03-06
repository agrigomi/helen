#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "argv.h"

#define MAX_ARGV		128
#define MAX_OPTION_NAME		128

static _argv_t	*gp_argv_map = NULL;
static _s32	g_argc = 0;
static _cstr_t	*g_argv = NULL;

static _argv_t *find_option(_cstr_t opt_name) {
	_argv_t *r = 0;
	_u32 n = 0;

	while(gp_argv_map[n].opt_name) {
		if(strcmp((_str_t)opt_name, (_str_t)gp_argv_map[n].opt_name) == 0) {
			r = &gp_argv_map[n];
			break;
		}
		n++;
	}

	return r;
}

static _bool find_value(_cstr_t val) {
	_bool r = _false;
	_u32 n = 0;

	while(gp_argv_map[n].opt_name) {
		if(gp_argv_map[n].opt_value) {
			if(strcmp(val, gp_argv_map[n].opt_value) == 0) {
				r = _true;
				break;
			}
		}
		n++;
	}

	return r;
}

_bool argv_parse(_s32 argc, _cstr_t argv[], _argv_t *opt_map) {
	_bool r = _true;
	_s32 i = 0;

	g_argv = argv;
	g_argc = argc;
	gp_argv_map = opt_map;

	for(; i < argc; i++) {
		if(argv[i][0] == '-') {
			_argv_t *p_opt = 0;
			_char_t opt_name[MAX_OPTION_NAME]="";

			if(argv[i][1] == '-') {
				// long option
				_u32 j = 2, n = 0;

				while(argv[i][j] != 0 && argv[i][j] != '=' && argv[i][j] != ':') {
					opt_name[n] = argv[i][j];
					n++;
					j++;
				}

				opt_name[n] = 0;

				if((p_opt = find_option(opt_name))) {
					if(p_opt->opt_flags & OF_LONG) {
						p_opt->opt_flags |= OF_PRESENT;
						if(p_opt->opt_flags & OF_VALUE) {
							if(argv[i][j] == '=' || argv[i][j] == ':')
								p_opt->opt_value = (_str_t)&argv[i][j+1];
							else {
								if(argv[i][j] == 0 && i < argc - 1) {
									p_opt->opt_value = (_str_t)argv[i+1];
									i++;
								}
							}
						}
					}
					r = _true;
				} else {
					r = _false;
					break;
				}
			} else {
				// short options
				_u32 j = 1;
				while(argv[i][j]) {
					opt_name[0] = argv[i][j];
					opt_name[1] = 0;

					if((p_opt = find_option(opt_name))) {
						p_opt->opt_flags |= OF_PRESENT;
						if(p_opt->opt_flags & OF_VALUE) {
							if(argv[i][j+1] != 0) {
								p_opt->opt_value = (_str_t)&argv[i][j+1];
								break;
							} else {
								if(i < argc - 1) {
									i++;
									p_opt->opt_value = (_str_t)argv[i];
								}
							}
						}
						r = _true;
					} else {
						r = _false;
						break;
					}

					j++;
				}
			}
		}
	}

	return r;
}

_bool argv_check(_cstr_t opt) {
	_bool r = _false;
	_argv_t *p_opt = find_option(opt);

	if(p_opt) {
		if(p_opt->opt_flags & OF_PRESENT)
			r = _true;
	}

	return r;
}

_cstr_t argv_value(_cstr_t opt) {
	_cstr_t r = NULL;
	_argv_t *p_opt = find_option(opt);

	if(p_opt) {
		if(p_opt->opt_flags & OF_PRESENT)
			r = p_opt->opt_value;

		// check for environment variable
		if(r && r[0] == '$')
			r = getenv(&r[1]);
	}

	return r;
}

_cstr_t argv_get(_u32 idx) {
	_cstr_t r = NULL;
	_u32 arg_idx = 0;
	_s32 i = 0;

	for(; i < g_argc; i++) {
		_cstr_t arg = g_argv[i];

		if(arg[0] != '-') {
			if(find_value(arg) == _false) {
				if(arg_idx == idx) {
					r = arg;
					break;
				}

				arg_idx++;
			}
		}
	}

	// check for environment variable
	if(r && r[0] == '$')
		r = getenv(&r[1]);

	return r;
}

_s32 argv_read_stdin(_uchar_t *buffer, _u32 sz_buffer, _u8 timeout_sec) {
	_s32 r = _false;

	if(timeout_sec) {
		fd_set selectset;
		struct timeval timeout = {timeout_sec, 0};

		FD_ZERO(&selectset);
		FD_SET(0, &selectset);
		r =  select(1, &selectset, NULL, NULL, &timeout);
		if(r > 0)
			// stdin has data, read it
			r = (_s32)read(0, buffer, sz_buffer);
	} else
		r = (_s32)read(0, buffer, sz_buffer);

	return r;
}
