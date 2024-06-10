#include <unistd.h>
#include "http.h"
#include "respawn.h"
#include "trace.h"
#include "argv.h"

_err_t resp_exec_v(_cstr_t argv[],
		int (*out)(unsigned char *buf, unsigned int sz, void *udata),
		int (*in)(unsigned char *buf, unsigned int sz, void *udata),
		void *udata) {
	_err_t r = E_FAIL;
	_proc_t proc;
	unsigned char bin[4096];
	unsigned char bout[16384];

	signal(SIGCHLD, [](__attribute__((unused)) int sig) {});

	if ((r = proc_exec_v(&proc, argv[0], argv)) == E_OK) {
		while (true) {
			int nin = (in) ? in(bin, sizeof(bin), udata) : 0;
			int nout = 0;

			if (nin)
				proc_write(&proc, bin, nin);

			while ((nout = proc_read(&proc, bout, sizeof(bout))) > 0)
				out(bout, nout, udata);

			if ((r = proc_status(&proc)) != -1)
				break;
		}
	}

	return r;
}

static void split_by_space(_cstr_t str, _u32 str_size, _str_t dst_arr[], _u32 arr_size) {
	_str_t p_str = (_str_t)malloc(str_size + 1);

	if (p_str) {
		_str_t rest = NULL;
		_str_t token;
		_u32 i = 0, l = 0;

		memset(p_str, 0, str_size + 1);
		strncpy(p_str, str, str_size);

		for (token = strtok_r(p_str, " ", &rest); token != NULL; token = strtok_r(NULL, " ", &rest)) {
			l = strlen(token) + 1;
			if ((dst_arr[i] = (_str_t)malloc(l))) {
				strcpy(dst_arr[i], token);
				i++;
				if (i >= arr_size)
					break;
			} else {
				TRACEfl("Unable to allocate memory !\n");
			}
		}

		free(p_str);
	}
}

_err_t resp_exec(_cstr_t cmd,
		int (*out)(unsigned char *buf, unsigned int sz, void *udata),
		int (*in)(unsigned char *buf, unsigned int sz, void *udata),
		void *udata) {
	_err_t r = E_FAIL;
	_str_t argv[256];
	int i = 0;

	memset(argv, 0, sizeof(argv));
	split_by_space(cmd, strlen(cmd), argv, 256);

	TRACE("http[%d] Execute '%s'\n", getpid(), cmd);
	r = resp_exec_v((_cstr_t *)argv, out, in, udata);

	while (argv[i]) {
		free(argv[i]);
		i++;
	}

	return r;
}

