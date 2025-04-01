#include <unistd.h>
#include "http.h"
#include "trace.h"
#include "argv.h"

_err_t resp_exec_v(_cstr_t argv[],
		int (*out)(unsigned char *buf, unsigned int sz, void *udata),
		int (*in)(unsigned char *buf, unsigned int sz, void *udata),
		void *udata, int timeout_s) {
	_err_t r = E_FAIL;
	_proc_t proc;
	unsigned char bin[4096];
	unsigned char bout[16384];
	int tout = timeout_s * 1000000;

	signal(SIGCHLD, [](__attribute__((unused)) int sig) {});

	if ((r = proc_exec_v(&proc, argv[0], argv)) == E_OK) {
		 while (tout > 0 && proc_status(&proc) == -1) {
			int nin = (in) ? in(bin, sizeof(bin), udata) : 0;
			int nout = 0;

			usleep(10000);

			if (nin > 0) {
				proc_write(&proc, bin, nin);
				tout = timeout_s * 1000000;
			} else if (nin < 0) {
				TRACE("http[%d] Terminate '%s'\n", getpid(), argv[0]);
				proc_break(&proc);
			}

			if ((nout = verify_input(proc.PREAD_FD)) > 0) {
				nout = proc_read(&proc, bout, sizeof(bout));
				out(bout, nout, udata);
				tout = timeout_s * 1000000;
			}

			if (nin == 0 && nout == 0)
				tout -= 10000;
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
	if ((r = resp_exec_v((_cstr_t *)argv, out, in, udata, atoi(argv_value(OPT_TIMEOUT)))) != E_OK) {
		LOG("http[%d] Failed to execute '%s'\n", getpid(), cmd);
	}

	while (argv[i]) {
		free(argv[i]);
		i++;
	}

	return r;
}

_err_t resp_exec(_cstr_t cmd, _proc_t *proc) {
	_err_t r = E_FAIL;
	_str_t argv[256];
	int i = 0;

	memset(argv, 0, sizeof(argv));
	split_by_space(cmd, strlen(cmd), argv, 256);

	signal(SIGCHLD, [](__attribute__((unused)) int sig) {});
	TRACE("http[%d] Execute '%s'\n", getpid(), cmd);
	r = proc_exec_v(proc, argv[0], argv);

	while (argv[i]) {
		free(argv[i]);
		i++;
	}

	return r;
}

