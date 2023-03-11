#include <malloc.h>
#include <unistd.h>
#include "config.h"
#include <openssl/ssl.h>
#include "cfg.h"
#include "respawn.h"
#include "trace.h"

#define MAX_BUFFER_SIZE	1024*1024
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
	io_cxt.ssl = cl_cxt;

	if(proc_exec_ve(&io_cxt.proc, pl->argv[0], pl->argv, pl->env) == 0) {
		pthread_create(&pl->thread, NULL, [](void *udata)->void* {
			_ssl_io_context_t *p_io_cxt = (_ssl_io_context_t *)udata;

			p_io_cxt->flags = IO_RUNNING;

			while((p_io_cxt->flags & IO_RUNNING) && proc_status(&p_io_cxt->proc) == -1) {
				int nout = 0;

				if((nout = proc_read(&p_io_cxt->proc, p_io_cxt->out, MAX_BUFFER_SIZE)) > 0)
					SSL_write(p_io_cxt->ssl, p_io_cxt->out, nout);
				else
					break;
			}

			p_io_cxt->flags = IO_STOPPED;
			return NULL;
		}, &io_cxt);

		pthread_setname_np(pl->thread, "SSL tunnel");
		while(!(io_cxt.flags & IO_RUNNING))
			usleep(10000);

		while((io_cxt.flags & IO_RUNNING) && proc_status(&io_cxt.proc) == -1) {
			int nin = 0;

			if((nin = SSL_read(io_cxt.ssl, io_cxt.in, MAX_BUFFER_SIZE)) > 0)
				proc_write(&io_cxt.proc, io_cxt.in, nin);
			else
				break;
		}

		_ulong err = ERR_get_error();
		if(err) {
			_char_t error_string[2048] = "";

			ERR_error_string_n(err, error_string, sizeof(error_string));
			TRACE("hl[%d]: SSL %s\n", getpid(), error_string);
		}

		proc_break(&io_cxt.proc);
		usleep(100000);
		if(proc_status(&io_cxt.proc) == (_s8)-1)
			proc_kill(&io_cxt.proc);
		while(io_cxt.flags & IO_RUNNING)
			usleep(10000);
	}

	free(io_cxt.in);
	free(io_cxt.out);
}
