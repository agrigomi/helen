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
	{ OPT_SHELP,		0,					NULL,				"Print this help" },
	{ OPT_VER,		0,					NULL,				"Print version" },
	{ OPT_EXEC,		0,					NULL,				"Run executable files in documents root" },
	{ OPT_LISTEN,		0,					NULL,				"Listen mode" },
	{ OPT_PORT,		OF_VALUE |OF_PRESENT,			(_str_t)"8080",			"Listen port (-" OPT_PORT "<port number>)" },
	{ OPT_HELP,		OF_LONG,				NULL,				"Print this help" },
	{ OPT_DIR,		OF_LONG | OF_VALUE | OF_PRESENT,	(_str_t)".",			"Settings directory (--" OPT_DIR "=<patth>)" },
	{ OPT_TIMEOUT,		OF_LONG | OF_VALUE | OF_PRESENT,	(_str_t)"30",			"Socket timeout in seconds (--" OPT_TIMEOUT "=<sec.>)" },
	{ OPT_SSL_CERT,		OF_LONG | OF_VALUE,			NULL,				"SSL certificate file (PEM only)" },
	{ OPT_SSL_KEY,		OF_LONG | OF_VALUE,			NULL,				"SSL private key file (PEM only)" },
	{ OPT_SSL_METHOD,	OF_LONG | OF_VALUE,			NULL,				"SSL server method (SSLv23, TLSv1_2, DTLS, TLS)" },
	{ OPT_PROXY,		OF_LONG,				NULL,				"Enable proxy (method CONNECT)" },
	//...
	{ NULL,			0,					NULL,				NULL }
};

static void usage(void) {
	int n = 0;

	printf("options:\n");
	while (args[n].opt_name) {
		if (args[n].opt_flags & OF_LONG)
			printf("--%-10s  \t %s\n", args[n].opt_name, args[n].opt_help);
		else
			printf("-%-10s   \t %s\n", args[n].opt_name, args[n].opt_help);

		n++;
	}

	printf("Usage: http [options]\n");
}

int main(int argc, char *argv[]) {
	int r = 0;

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
		cfg_uninit();
		exit(0);
	});
	signal(SIGKILL, [](__attribute__((unused)) int sig) {
		TRACE("http[%d]: SIGKILL\n", getpid());
		cfg_uninit();
		exit(0);
	});
	signal(SIGTERM, [](__attribute__((unused)) int sig) {
		TRACE("http[%d]: SIGTERM\n", getpid());
		cfg_uninit();
		exit(0);
	});
	signal(SIGPIPE, [](__attribute__((unused)) int sig) {
		TRACE("http[%d]: SIGPIPE\n", getpid());
	});

	if (argv_parse(argc, (_cstr_t *)argv, args)) {
		if (argv_check(OPT_SHELP) || argv_check(OPT_HELP))
			usage();
		else if (argv_check(OPT_VER))
			printf("%s\n", VERSION);
		else {
			if ((r = cfg_init() == E_OK)) {
				r = io_start();
				cfg_uninit();
			}
		}
	} else {
		usage();
		r = -1;
	}

	return r;
}
