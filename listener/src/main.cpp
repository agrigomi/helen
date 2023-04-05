#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "config.h"
#include "argv.h"
#include "sig.h"
#include "trace.h"
#include "cfg.h"
#include "api_ssl.h"

static _argv_t args[] = {
	{ OPT_SHELP,	0,				NULL,		"Print this help" },
	{ OPT_HELP,	OF_LONG,			NULL,		"Print this help" },
	{ OPT_SVERSION,	0,				NULL,		"Print version" },
	{ OPT_CFG,	OF_LONG|OF_VALUE,		NULL,		"Run with configuration in JSON format (--" OPT_CFG "=<filename>.json)" },
	//...
	{ NULL,		0,				NULL,		NULL }
};

bool _g_fork_ = false;

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
	printf("\nUsage: hl [options]\n");
}

int main(int argc, char *argv[]) {
	int r = 0;

	signal(SIGCHLD, [](__attribute__((unused)) int sig) {
		int stat;
		pid_t	pid;

		while (1) {
			if ((pid = wait3 (&stat, WNOHANG, (struct rusage *)NULL )) <= 0)
				break;
			TRACE("hl[%d]: SIGCHLD: PID=%u, STATUS=%d\n", getpid(), pid, stat);
		}
	});
	signal(SIGSEGV, [](__attribute__((unused)) int sig) {
		TRACE("hl: SIGSEGV\n");
		dump_stack();
		exit(0);
	});
	signal(SIGINT, [](__attribute__((unused)) int sig) {
		TRACE("hl[%d]: SIGINT\n", getpid());
		cfg_stop();
		exit(0);
	});
	signal(SIGKILL, [](__attribute__((unused)) int sig) {
		TRACE("hl[%d]: SIGKILL\n", getpid());
		cfg_stop();
		exit(0);
	});
	signal(SIGTERM, [](__attribute__((unused)) int sig) {
		TRACE("hl[%d]: SIGTERM\n", getpid());
		cfg_stop();
		exit(0);
	});
	signal(SIGPIPE, [](__attribute__((unused)) int sig) {
		TRACE("hl[%d]: SIGPIPE\n", getpid());
	});

	if (argv_parse(argc, (_cstr_t *)argv, args)) {
		if (argv_check(OPT_SVERSION))
			printf("%s\n", VERSION);
		if (argv_check(OPT_SHELP) || argv_check(OPT_HELP))
			usage();

		_cstr_t cfg_file = argv_value(OPT_CFG);

		if (cfg_file) {
			ssl_init();
			cfg_load(cfg_file);
			cfg_start();

			while (1)
				usleep(1000000);
		}
	} else
		usage();

	return r;
}
