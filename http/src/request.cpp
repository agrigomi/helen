#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "http.h"
#include "trace.h"
#include "str.h"
#include "url-codec.h"
#include "argv.h"

void set_env_var(char *vstr, const char *div) {
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

void req_decode_url(_cstr_t url) {
	char *scheme= NULL;
	char *urn = NULL;
	char decoded_url[2048];
	char *s = decoded_url;
	char *uri = NULL;
	char *domain = NULL;
	char *port = NULL;
	char *authority = NULL;

	memset(decoded_url, 0, sizeof(decoded_url));
	UrlDecode(url, strlen(url), decoded_url, sizeof(decoded_url) - 1);

	// set whole URL
	setenv(REQ_URL, decoded_url, 1);

	// scheme
	str_div_s(decoded_url, "://", &scheme, &urn);
	if (urn) {
		if ((urn - s) <= 8) {
			s = urn;
			setenv(REQ_SCHEME, scheme, 1);
			setenv(REQ_URN, urn, 1);
		} else {
			*(urn - 3) = ':'; //restore
			urn = s;
			setenv(REQ_SCHEME, SCHEME_FILE, 1);
		}
	} else {
		s = urn = scheme;
		setenv(REQ_SCHEME, SCHEME_FILE, 1);
		setenv(REQ_URN, urn, 1);
	}

	// URI
	if ((uri = strstr(s, "/"))) {
		char *path = NULL;
		char *parameters = NULL;

		setenv(REQ_URI, uri, 1);
		str_div_s(uri, "?", &path, &parameters);
		setenv(REQ_PATH, path, 1);

		if (parameters) {
			char *anchor = NULL;

			str_div_s(parameters, "#", &parameters, &anchor);
			if (anchor)
				setenv(REQ_ANCHOR, anchor, 1);

			if (strlen(parameters)) {
				str_split(parameters, "&", [] (int __attribute__((unused)) idx,
						char *str,
						void __attribute__((unused)) *udata) -> int {
					set_env_var(str, "=");
					return 0;
				}, NULL);
			}
		}

		*uri = 0;
		authority = urn;

		if (strlen(authority)) {
			setenv(REQ_AUTHORITY, authority, 1);
			str_div_s(authority, ":", &domain, &port);
			setenv(REQ_DOMAIN, domain, 1);

			if (port)
				setenv(REQ_PORT, port, 1);
		}
	} else
		setenv(REQ_DOMAIN, s, 1);
}

_err_t decode_request(char *request) {
	_err_t r = E_FAIL;
	char *rest = NULL;
	char *token = NULL;

	if ((token = strtok_r(request, " ", &rest))) {
		setenv(REQ_METHOD, token, 1);

		if ((token = strtok_r(NULL, " ", &rest))) {
			// URL decode
			req_decode_url(token);

			if ((token = strtok_r(NULL, " ", &rest))) {
				setenv(REQ_PROTOCOL, token, 1);
				r = E_OK;
			}
		}
	}

	return r;
}

_err_t req_receive(int timeout, int *req_len) {
	_err_t r = E_FAIL;
	char line[2048];
	int rl = 0; // request length

	*req_len = 0;
	// Set default response header fields
	hdr_init();

	if (io_wait_input(timeout) > 0) {
		// read request line
		if ((rl = io_read_line(line, sizeof(line))) > 0) {
			TRACE("http[%d] %s\n", getpid(), line);
			// parse request
			if ((r = decode_request(line)) == E_OK) {
				_cstr_t scheme = getenv(REQ_SCHEME);

				if (!scheme)
					scheme = SCHEME_FILE;

				if (strcmp(scheme, SCHEME_FILE) == 0) {
					int hl = 0; // header line length

					// read header lines
					while ((hl = io_read_line(line, sizeof(line))) > 0) {
						rl += hl;
						set_env_var(line, ":");
					}

					*req_len = rl;
					setenv(RES_ENV_SERVER, SERVER_NAME, 1);
					setenv(RES_ENV_ALLOW, ALLOW_METHOD, 1);
				} else if (strcmp(scheme, SCHEME_HTTP) == 0 && argv_check(OPT_PROXY))
					r = proxy_http();
				else if (strcmp(scheme, SCHEME_HTTPS) == 0 && argv_check(OPT_PROXY))
					r = proxy_https();
				else {
					while (io_read_line(line, sizeof(line)) > 0) {
						TRACE("%s\n", line);
					}

					r = send_error_response(NULL, HTTPRC_SERVICE_UNAVAILABLE);
				}
			} else {
				TRACE("http[%d] Invalid request\n", getpid());
				send_error_response(NULL, HTTPRC_BAD_REQUEST);
				r = E_DONE;
			}
		} else if(rl == 0) {
			// empty request
			r = E_OK;
			TRACE("http[%d] Empty request\n", getpid());
		} else {
			TRACE("http[%d] Disconnect (remote close)\n", getpid());
		}
	} else {
		send_error_response(NULL, HTTPRC_REQUEST_TIMEOUT);
		TRACE("http[%d] Request timed out\n", getpid());
	}

	return r;
}
