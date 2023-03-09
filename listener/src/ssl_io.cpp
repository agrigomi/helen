#include <malloc.h>
#include <unistd.h>
#include "config.h"
#include <openssl/ssl.h>
#include "cfg.h"
#include "respawn.h"

#define MAX_BUFFER_SIZE	64*1024
#define IO_RUNNING	(1<<0)
#define IO_STOPPED	(1<<1)

typedef struct {
	_proc_t	proc;	// context of child process
	_u8	*in;	// input buffer
	_u8	*out;	// output buffer
	SSL	*ssl;	// SSL connection context
	_u32	flags;
}_ssl_io_context_t;

void ssl_io(_listen_t *pl, SSL *cl_cxt) {
	_ssl_io_context_t io_cxt;

	io_cxt.in = (_u8 *)malloc(MAX_BUFFER_SIZE);
	io_cxt.out = (_u8 *)malloc(MAX_BUFFER_SIZE);
	io_cxt. ssl = cl_cxt;

	if(proc_exec_ve(&io_cxt.proc, pl->argv[0], pl->argv, pl->env) == 0) {
		pthread_create(&pl->thread, NULL, [](void *udata)->void* {
			_ssl_io_context_t *p_io_cxt = (_ssl_io_context_t *)udata;

			p_io_cxt->flags = IO_RUNNING;

			while(p_io_cxt->flags & IO_RUNNING) {
				//...
			}

			p_io_cxt->flags = IO_STOPPED;
			return NULL;
		}, &io_cxt);

		while(proc_status(&io_cxt.proc) == -1) {
			//...
		}

		proc_break(&io_cxt.proc);
		proc_wait(&io_cxt.proc);

		while(!(io_cxt.flags & IO_STOPPED))
			usleep(10000);
	}

	free(io_cxt.in);
	free(io_cxt.out);
}
