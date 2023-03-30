#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "http.h"
#include "argv.h"
#include "trace.h"

static _argv_t args[] = {
	{ OPT_HELP,		OF_LONG,				NULL,				"Print this help" },
	{ OPT_SHELP,		0,					NULL,				"Print this help" },
	{ OPT_VER,		0,					NULL,				"Print version" },
	{ OPT_DIR,		OF_LONG | OF_VALUE | OF_PRESENT,	(_str_t)"./.config/helen",	"Settings directory" },
	{ OPT_SSL_CERT,		OF_LONG | OF_VALUE,			NULL,				"SSL certificate file (PEM only)" },
	{ OPT_SSL_KEY,		OF_LONG | OF_VALUE,			NULL,				"SSL private key file (PEM only)" },
	{ OPT_SSL_METHOD,	OF_LONG | OF_VALUE | OF_PRESENT,	NULL,				"SSL server method (SSLv23, TLSv1_2, DTLS, TLS)" },
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

					/* Because of one descriptor for stdin and stdout */
					_g_ssl_out_ = _g_ssl_in_;
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
	const char *ssl_method = NULL;
	const char *ssl_cert = NULL;
	const char *ssl_key = NULL;

	if (argv_parse(argc, (_cstr_t *)argv, args)) {
		if (argv_check(OPT_SHELP) || argv_check(OPT_HELP))
			usage();
		if (argv_check(OPT_VER))
			printf("%s\n", VERSION);

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
	} else
		usage();

	return 0;
}
