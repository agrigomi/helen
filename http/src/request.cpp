#include <string.h>
#include <stdlib.h>
#include "http.h"
#include "trace.h"

static _err_t decode_request(char *request) {
	_err_t r = E_FAIL;
	char *rest = NULL;
	char *token = NULL;

	if ((token = strtok_r(request, " ", &rest))) {
		setenv(REQ_METHOD, token, 1);

		if ((token = strtok_r(NULL, " ", &rest))) {
			// URL decode
			//...

			setenv(REQ_URL, token, 1); //???

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
	char line[4096];

	// read request line
	if (io_read_line(line, sizeof(line), timeout) > 0) {
		// parse request
		if (decode_request(line) == E_OK) {
			// read header lines
			while ((r = io_read_line(line, sizeof(line), timeout)) > 0) {
				//...
			}
		}
	}

	return r;
}
