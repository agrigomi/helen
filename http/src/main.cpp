#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include "config.h"
#include "http.h"
#include "argv.h"
#include "trace.h"
#include "sig.h"

static _argv_t args[] = {
	{ OPT_HELP,		OF_LONG,				NULL,				"Print this help" },
	{ OPT_SHELP,		0,					NULL,				"Print this help" },
	{ OPT_VER,		0,					NULL,				"Print version" },
	{ OPT_DIR,		OF_LONG | OF_VALUE | OF_PRESENT,	(_str_t)".",			"Settings directory" },
	{ OPT_LISTEN,		0,					NULL,				"Listen mode" },
	{ OPT_PORT,		OF_VALUE |OF_PRESENT,			(_str_t)"8080",			"Listen port (-" OPT_PORT "<port number>)" },
	{ OPT_LPORT,		OF_LONG | OF_VALUE | OF_PRESENT,	(_str_t)"8080",			"Listen port (--" OPT_LPORT "=<port number>)" },
	{ OPT_SSL_CERT,		OF_LONG | OF_VALUE,			NULL,				"SSL certificate file (PEM only)" },
	{ OPT_SSL_KEY,		OF_LONG | OF_VALUE,			NULL,				"SSL private key file (PEM only)" },
	{ OPT_SSL_METHOD,	OF_LONG | OF_VALUE,			NULL,				"SSL server method (SSLv23, TLSv1_2, DTLS, TLS)" },
	//...
	{ NULL,			0,					NULL,				NULL }
};

SSL_CTX *_g_ssl_context_ = NULL;
SSL	*_g_ssl_in_ = NULL;
SSL	*_g_ssl_out_ = NULL;

static void usage(void) {
	int n = 0;

	printf("options:\n");
	while (args[n].opt_name) {
		if (args[n].opt_flags & OF_LONG)
			printf("--%s:      \t%s\n", args[n].opt_name, args[n].opt_help);
		else
			printf("-%s:      \t%s\n", args[n].opt_name, args[n].opt_help);

		n++;
	}

	printf("Usage: http [options]\n");
}

static void setup_ssl_context(const char *method, const char *cert, const char *key) {
	const SSL_METHOD *ssl_method = ssl_select_method(method);

	if (ssl_method) {
		if ((_g_ssl_context_ = ssl_create_context(ssl_method))) {
			if (ssl_load_cert(_g_ssl_context_, cert)) {
				if (ssl_load_key(_g_ssl_context_, key)) {
					_g_ssl_in_ = SSL_new(_g_ssl_context_);

					SSL_set_fd(_g_ssl_in_, STDIN_FILENO);
					SSL_set_accept_state(_g_ssl_in_);

					if (SSL_accept(_g_ssl_in_) == 1)
						/* Because of one descriptor for stdin and stdout */
						_g_ssl_out_ = _g_ssl_in_;
					else {
						TRACE("http[%d] SSL handshake failed\n", getpid());
						SSL_CTX_free(_g_ssl_context_);
						SSL_free(_g_ssl_in_);
						_g_ssl_context_ = NULL;
						_g_ssl_in_ = NULL;
					}
				} else {
					TRACE("http[%d] Failed to load SSL key '%s'\n", getpid(), key);
					SSL_CTX_free(_g_ssl_context_);
					_g_ssl_context_ = NULL;
				}
			} else {
				TRACE("http[%d] Failed to load SSL certificate '%s'\n", getpid(), cert);
				SSL_CTX_free(_g_ssl_context_);
				_g_ssl_context_ = NULL;
			}
		} else {
			TRACE("http[%d] Unable to allocate SSL context\n", getpid());
		}
	} else {
		TRACE("http[%d] Unsupported SSL method '%s'\n", getpid(), method);
	}
}

int main(int argc, char *argv[]) {
	int r = 0;
	const char *ssl_method = NULL;
	const char *ssl_cert = NULL;
	const char *ssl_key = NULL;

	signal(SIGCHLD, [](__attribute__((unused)) int sig) {
		int stat;
		pid_t	pid;

		while (1) {
			if ((pid = wait3 (&stat, WNOHANG, (struct rusage *)NULL )) <= 0)
				break;
			TRACE("http[%d]: SIGCHLD: PID=%u, STATUS=%d\n", getpid(), pid, stat);
		}
	});
	signal(SIGSEGV, [](__attribute__((unused)) int sig) {
		TRACE("http: SIGSEGV\n");
		dump_stack();
		exit(0);
	});
	signal(SIGINT, [](__attribute__((unused)) int sig) {
		TRACE("http[%d]: SIGINT\n", getpid());
		exit(0);
	});
	signal(SIGKILL, [](__attribute__((unused)) int sig) {
		TRACE("http[%d]: SIGKILL\n", getpid());
		exit(0);
	});
	signal(SIGTERM, [](__attribute__((unused)) int sig) {
		TRACE("http[%d]: SIGTERM\n", getpid());
		exit(0);
	});
	signal(SIGPIPE, [](__attribute__((unused)) int sig) {
		TRACE("http[%d]: SIGPIPE\n", getpid());
	});

	if (argv_parse(argc, (_cstr_t *)argv, args)) {
		if (argv_check(OPT_SHELP) || argv_check(OPT_HELP))
			usage();
		if (argv_check(OPT_VER))
			printf("%s\n", VERSION);

		r = cfg_init();

		//...

		r = E_OK;
/*
		if (!(ssl_method = argv_value(OPT_SSL_METHOD)))
			ssl_method = getenv(OPT_SSL_METHOD);
		if (!(ssl_cert = argv_value(OPT_SSL_CERT)))
			ssl_cert = getenv(OPT_SSL_CERT);
		if (!(ssl_key = argv_value(OPT_SSL_KEY)))
			ssl_key = getenv(OPT_SSL_KEY);

		if (ssl_method && ssl_cert && ssl_key) {
			// setup SSL context
			TRACE("http[%d] Setup SSL method='%s'; cert='%s'; key='%s'\n", getpid(), ssl_method, ssl_cert, ssl_key);
			setup_ssl_context(ssl_method, ssl_cert, ssl_key);
		}

		//...

		if (_g_ssl_in_)
			SSL_free(_g_ssl_in_);
		if (_g_ssl_context_)
			SSL_CTX_free(_g_ssl_context_);
*/
	} else {
		usage();
		r = -1;
	}

	return r;
}
