#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "http.h"
#include "trace.h"
#include "str.h"

static void set_env_var(char *vstr, const char *div) {
	char *rest = NULL;
	char *token = NULL;

	if ((token = strtok_r(vstr, div, &rest))) {
		if (div[0] == ':') {
			char lb[256] = "REQ_";
			size_t i = 0, j = 4;

			for (; token[i] && j < sizeof(lb); i++, j++) {
				if (token[i] == '-')
					lb[j] = '_';
				else
					lb[j] = (char)toupper(token[i]);
			}

			lb[j] = 0;
			str_trim_left(rest);
			setenv(lb, rest, 1);
		} else
			setenv(token, rest, 1);
	}
}

static void decode_url(char *url) {
	char *rest = NULL;
	char *token = NULL;

	if ((token = strtok_r(url, "?", &rest))) {
		setenv(REQ_URL, token, 1);

		while ((token = strtok_r(NULL, "&", &rest)))
			set_env_var(token, "=");
	}
}

static _err_t decode_request(char *request) {
	_err_t r = E_FAIL;
	char *rest = NULL;
	char *token = NULL;

	if ((token = strtok_r(request, " ", &rest))) {
		setenv(REQ_METHOD, token, 1);

		if ((token = strtok_r(NULL, " ", &rest))) {
			// URL decode
			decode_url(token);

			if ((token = strtok_r(NULL, " ", &rest))) {
				setenv(REQ_PROTOCOL, token, 1);
				r = E_OK;
			}
		}
	}

	return r;
}

_err_t req_receive(int timeout) {
	_err_t r = E_FAIL;
	char line[2048];

	// read request line
	if (io_read_line(line, sizeof(line), timeout) > 0) {
		// parse request
		if (decode_request(line) == E_OK) {
			// read header lines
			while ((r = io_read_line(line, sizeof(line), timeout)) > 0)
				set_env_var(line, ":");

			setenv(RES_SERVER, SERVER_NAME, 1);
		}
	}

	return r;
}
