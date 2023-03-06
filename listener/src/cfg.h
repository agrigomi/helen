#ifndef __CFG_H__
#define __CFG_H__

#include <pthread.h>
#include "err.h"
#include "dtype.h"

#define MAX_LISTEN_NAME	64
#define MAX_ARGV	256
#define MAX_ENV		256

#define LISTEN_RUNNING	(1 << 0)
#define LISTEN_STOPPED	(1 << 1)

typedef struct {
	_char_t name[MAX_LISTEN_NAME]; 	// process name
	_u32	port;			// listen port
	_cstr_t	argv[MAX_ARGV];		// process arguments
	_cstr_t	env[MAX_ENV];		// environment variables (optional)
	pthread_t thread;		// thread context
	int 	server_fd;		// server socket FD
	volatile _u32 flags;		// flags for internal use
}_listen_t;

/**
Loading configuration file (JSON)
*/
_err_t cfg_load(_cstr_t fname);

/**
Enumeration of configured listeners
*/
void cfg_enum_listen(void (*)(_listen_t *));

#endif

