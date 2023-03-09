#ifndef __CFG_H__
#define __CFG_H__

#include <pthread.h>
#include "err.h"
#include "dtype.h"
#include "api_ssl.h"

#define MAX_LISTEN_NAME	64
#define MAX_ARGV	256
#define MAX_ENV		256
#define MAX_PATH	256
#define MAX_SSL_METHOD	64

#define LISTEN_RUNNING	(1 << 0)
#define LISTEN_STOPPED	(1 << 1)

typedef struct {
	_char_t name[MAX_LISTEN_NAME]; 		// process name
	_u32	port;				// listen port
	_str_t	argv[MAX_ARGV];			// process arguments
	_str_t	env[MAX_ENV];			// environment variables (optional)
	pthread_t thread;			// thread context
	int 	server_fd;			// server socket FD
	volatile _u32 flags;			// flags for internal use
	bool	ssl_enable;			// enable / disable SSL
	_char_t	ssl_method[MAX_SSL_METHOD];	// SSL method name
	_char_t ssl_cert[MAX_PATH];		// Path to SSL certificate file
	_char_t ssl_key[MAX_PATH];		// Path to SSL key file
	SSL_CTX	*ssl_context;			// Pointer to SSL server context
	_u32	ssl_buffer_size;		// Buffer size for SSL IO
}_listen_t;

/**
Loading configuration file (JSON)
*/
_err_t cfg_load(_cstr_t fname);

/**
Enumeration of configured listeners
*/
void cfg_enum_listen(void (*)(_listen_t *));

/**
Start listen threads
*/
void cfg_start(void);

/**
Stop listen threads
*/
void cfg_stop(void);


#endif

