#include <stdio.h>
#include "config.h"
#include "http.h"
#include "argv.h"

static _argv_t args[] = {
	{ OPT_HELP,	OF_LONG,			NULL,		"Print this help" },
	{ OPT_SHELP,	0,				NULL,		"Print this help" },
	{ OPT_VER,	0,				NULL,		"Print version" },
	{ OPT_DIR,	OF_LONG | OF_VALUE,		NULL,		"Settings directory" },
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
	printf("Usage: http [options]\n");
}

int main(int argc, char *argv[]) {
	if(argc > 1 && argv_parse(argc, (_cstr_t *)argv, args)) {
		if(argv_check(OPT_SHELP) || argv_check(OPT_HELP))
			usage();
		if(argv_check(OPT_VER))
			printf("%s\n", VERSION);
	} else
		usage();

	return 0;
}
