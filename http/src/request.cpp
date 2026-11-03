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

static bool is_line_empty(const char *line) {
	bool r = false;

	if (*line == '\r' || *line == '\n')
		r = true;

	return r;
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

static _err_t receive_request(char *buffer, int sz, int *req_len) {
	_err_t r = E_FAIL;
	int bidx = 0; /* index in buffer */
	int sz_line = 0;

	memset(buffer, 0 , sz);

	while ((sz_line = io_read_line((buffer + bidx), sz - bidx)) > 0) {
		if (is_line_empty(buffer + bidx)) {
			*req_len = bidx;
			r = E_OK;
			break;
		}

		bidx += sz_line;
	}

	return r;
}

static char *enum_lines(char *buf, int sz, int *lsz, char **next) {
	char *r = NULL;
	char *p, *end;

	*lsz = 0;
	*next = NULL;

	if (buf) {
		end = buf + sz;

		/* skip leading control chars, stop at NUL */
		p = buf;
		while (p < end && *p && *p < 0x20)
			p++;

		/* a printable char here starts the line */
		if (p < end && *p) {
			r = p;
			while (p < end && *p >= 0x20)
				p++;

			*lsz = (int)(p - r);

			/* terminate the line and skip the separator run up to the next line */
			if (p < end && *p) {
				*p++ = 0;
				while (p < end - 1 && *p && *p < 0x20)
					p++;

				if (*p)
					*next = p;
			}
		}
	}

	return r;
}

_err_t req_receive(int timeout, int *req_len) {
	_err_t r = E_FAIL;
	char req_buffer[2048];

	*req_len = 0;
	// Set default response header fields
	hdr_init();
	// set default protocol version
	setenv(RES_PROTOCOL, STR_HTTP_1, 1);

	if (io_wait_input(timeout) > 0) {
		if ((r = receive_request(req_buffer, sizeof(req_buffer), req_len)) == E_OK) {
			char *line = NULL;
			char *next = NULL;
			int line_sz = 0;

			TRACE("<< %s\n", req_buffer);

			// Read the first line (HTTP request)
			if ((line = enum_lines(req_buffer, *req_len, &line_sz, &next))) {
				if ((r = decode_request(line)) == E_OK) {
					_cstr_t scheme = getenv(REQ_SCHEME);
					bool proxy = argv_check(OPT_PROXY);

					if (!scheme) // default scheme is FILE
						scheme = SCHEME_FILE;

					if (strcmp(scheme, SCHEME_FILE) == 0) {
						while ((line = next)) {
							// parse header
							if ((line = enum_lines(line, *req_len - line_sz, &line_sz, &next)))
								set_env_var(line, ":");
						}

						setenv(RES_ENV_SERVER, SERVER_NAME, 1);
						setenv(RES_ENV_ALLOW, ALLOW_METHOD, 1);
					} else if (strcmp(scheme, SCHEME_HTTP) == 0 && proxy)
						r = proxy_http(next, strlen(next));
					else if (strcmp(scheme, SCHEME_HTTPS) == 0 && proxy)
						r = proxy_https(next, strlen(next));
					else {
						LOG_WRN("http: PROXY Service unavailable. IP='%s'\n", io_get_peer_ip());
						send_error_response(NULL, HTTPRC_SERVICE_UNAVAILABLE);
						r = E_DONE;
					}
				} else {
					LOG_ERR("http: Invalid request. IP='%s' REQUEST='%s'\n", io_get_peer_ip(), line);
					send_error_response(NULL, HTTPRC_BAD_REQUEST);
					r = E_DONE;
				}
			} else {
				// empty request
				r = E_OK;
				TRACE_WRN("http: Empty request\n");
			}
		} else {
			TRACE("http: Disconnect (remote close)\n");
		}
	} else {
		send_error_response(NULL, HTTPRC_REQUEST_TIMEOUT);
		TRACE_WRN("http: Request timed out\n");
		r = E_TIMEOUT;
	}

	return r;
}
