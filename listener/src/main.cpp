#include <stdio.h>
#include "config.h"
#include <sys/wait.h>
#include "config.h"
#include "argv.h"
#include "sig.h"
#include "trace.h"

#define OPT_SHELP	"h"
#define OPT_HELP	"help"
#define OPT_SVERSION	"v"
#define OPT_CFG		"config"

static _argv_t args[] = {
	{ OPT_SHELP,	0,				NULL,		"Print this help" },
	{ OPT_HELP,	OF_LONG,			NULL,		"Print this help" },
	{ OPT_SVERSION,	0,				NULL,		"Print version" },
	{ OPT_CFG,	OF_LONG|OF_VALUE,		NULL,		"Run with configuration in JSON format (--" OPT_CFG "=<filename.json>)" },
	//...
	{ NULL,		0,				NULL,		NULL }
};

static void usage(void) {
	int n = 0;

	printf("options:\n");
	while(args[n].opt_name) {
		if(args[n].opt_flags & OF_LONG)
			printf("--%s:      \t%s\n", args[n].opt_name, args[n].opt_help);
		else
			printf("-%s:      \t%s\n", args[n].opt_name, args[n].opt_help);

		n++;
	}
	printf("Usage: hl [options]\n");
}

int main(int argc, char *argv[]) {
	int r = 0;

	handle(SIGCHLD, [](__attribute__((unused)) int signum,
			siginfo_t *siginfo,
			__attribute__((unused)) void *arg) {
		int status = -1;

		waitpid(siginfo->si_pid, &status, WNOHANG);
		TRACE("SIGCHLD: PID=%u, STATUS=%d\n", siginfo->si_pid, status);
	});
	signal(SIGSEGV, [](__attribute__((unused)) int sig) {
		TRACE("SIGSEGV\n");
		dump_stack();
	});

	if(argv_parse(argc, (_cstr_t *)argv, args)) {
		if(argv_check(OPT_SHELP) || argv_check(OPT_HELP))
			usage();
		if(argv_check(OPT_SVERSION))
			printf("%s\n", VERSION);

		//...
	} else
		usage();

	return r;
}
