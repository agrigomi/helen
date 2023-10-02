#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <map>
#include <vector>
#include "http.h"
#include "trace.h"
#include "fcfg.h"
#include "str.h"
#include "respawn.h"

static std::map<int, _cstr_t> _g_resp_text_ = {
	{ HTTPRC_CONTINUE,		"Continue" },
	{ HTTPRC_SWITCHING_PROTOCOL,	"Switching Protocols" },
	{ HTTPRC_OK,			"OK" },
	{ HTTPRC_CREATED,		"Created" },
	{ HTTPRC_ACCEPTED,		"Accepted" },
	{ HTTPRC_NON_AUTH,		"Non-Authoritative Information" },
	{ HTTPRC_NO_CONTENT,		"No Content" },
	{ HTTPRC_RESET_CONTENT,		"Reset Content" },
	{ HTTPRC_PART_CONTENT,		"Partial Content" },
	{ HTTPRC_MULTICHOICES,		"Multiple Choices" },
	{ HTTPRC_MOVED_PERMANENTLY,	"Moved Permanently" },
	{ HTTPRC_FOUND,			"Found" },
	{ HTTPRC_SEE_OTHER,		"See Other" },
	{ HTTPRC_NOT_MODIFIED,		"Not Modified" },
	{ HTTPRC_USE_PROXY,		"Use proxy" },
	{ HTTPRC_TEMP_REDIRECT,		"Temporary redirect" },
	{ HTTPRC_BAD_REQUEST,		"Bad Request" },
	{ HTTPRC_UNAUTHORIZED,		"Unauthorized" },
	{ HTTPRC_PAYMENT_REQUIRED,	"Payment Required" },
	{ HTTPRC_FORBIDDEN,		"Forbidden" },
	{ HTTPRC_NOT_FOUND,		"Not Found" },
	{ HTTPRC_METHOD_NOT_ALLOWED,	"Method Not Allowed" },
	{ HTTPRC_NOT_ACCEPTABLE,	"Not Acceptable" },
	{ HTTPRC_PROXY_AUTH_REQUIRED,	"Proxy Authentication Required" },
	{ HTTPRC_REQUEST_TIMEOUT,	"Request Time-out" },
	{ HTTPRC_CONFLICT,		"Conflict" },
	{ HTTPRC_GONE,			"Gone" },
	{ HTTPRC_LENGTH_REQUIRED,	"Length Required" },
	{ HTTPRC_PRECONDITION_FAILED,	"Precondition Failed" },
	{ HTTPRC_REQ_ENTITY_TOO_LARGE,	"Request Entity Too Large" },
	{ HTTPRC_REQ_URI_TOO_LARGE,	"Request-URI Too Large" },
	{ HTTPRC_UNSUPPORTED_MEDIA_TYPE,"Unsupported Media Type" },
	{ HTTPRC_EXPECTATION_FAILED,	"Expectation Failed" },
	{ HTTPRC_INTERNAL_SERVER_ERROR,	"Internal Server Error" },
	{ HTTPRC_NOT_IMPLEMENTED,	"Not Implemented" },
	{ HTTPRC_BAD_GATEWAY,		"Bad Gateway" },
	{ HTTPRC_SERVICE_UNAVAILABLE,	"Service Unavailable" },
	{ HTTPRC_GATEWAY_TIMEOUT,	"Gateway Time-out" },
	{ HTTPRC_VERSION_NOT_SUPPORTED,	"HTTP Version not supported" },
};

static std::map<int, _cstr_t> _g_resp_content_ = {
	{ HTTPRC_NOT_FOUND,		"<!DOCTYPE html><html><head></head><body><h1>Not found #404</h1></body></html>" },
	{ HTTPRC_INTERNAL_SERVER_ERROR,	"<!DOCTYPE html><html><head></head><body><h1>Internal server error #500</h1></body></html>" },
	{ HTTPRC_FORBIDDEN,		"<!DOCTYPE html><html><head></head><body><h1>Forbidden path #403</h1></body></html>" },
	{ HTTPRC_NOT_IMPLEMENTED,	"<!DOCTYPE html><html><head></head><body><h1>Not implemented #501</h1></body></html>" }
};

static const char *methods[] = { "GET", "HEAD", "POST",
				"PUT", "DELETE", "CONNECT",
				"OPTIONS", "TRACE", "PATCH",
				NULL };
#define METHOD_GET	0
#define METHOD_HEAD	1
#define METHOD_POST	2
#define METHOD_PUT	3
#define METHOD_DELETE	4
#define METHOD_CONNECT	5
#define METHOD_OPTIONS	6
#define METHOD_TRACE	7
#define METHOD_PATCH	8

static char _g_resp_buffer_[256 * 1024];

typedef struct {
	unsigned long begin; // start offset in content
	unsigned long size; // size in bytes
	char	header[512]; // range header
} _range_t;

typedef std::vector<_range_t> _v_range_t;

#define RCT_NONE	0
#define RCT_STATIC	1
#define RCT_MAPPING	2

#define MAX_BOUNDARY	20

typedef struct {
	char		*s_method;
	int		i_method; // resolved method
	char		*url; // request URL
	char		*protocol;
	char		*path; // resolved path (real path)
	bool		b_st; // valid stat
	struct stat	st; // file status
	_vhost_t	*p_vhost;
	_v_range_t	*pv_ranges;
	char		boundary[MAX_BOUNDARY];
	int		rc; // response code
	int		rc_type; // response content type
	union {
		_cstr_t		static_text;
		_mapping_t	*p_mapping;
	};
	char		*header;
	int		sz_hbuffer;
} _resp_t;

typedef struct {
	_cstr_t		var;
	_cstr_t 	(*vcb)(_resp_t *);
} _hdr_t;

static int resolve_method(_cstr_t method) {
	int n = 0;

	for (; methods[n]; n++) {
		if (strcasecmp(method, methods[n]) == 0)
			break;
	}

	return n;
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

static char _g_vhdr_[4096];

static _hdr_t _g_hdef_[] = {
	{ RES_ACCEPT_RANGES,		[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						return "Bytes";
					}},
	{ RES_ALLOW,			[] (_resp_t *p) -> _cstr_t {
						_cstr_t r = NULL;

						if (p->rc == HTTPRC_METHOD_NOT_ALLOWED)
							r = ALLOW_METHOD;

						return r;
					}},
	{ RES_CONTENT_LENGTH,		[] (_resp_t *p) -> _cstr_t {
						if (p->b_st)
							snprintf(_g_vhdr_, sizeof(_g_vhdr_), "%lu", p->st.st_size);
						else if (p->rc_type == RCT_STATIC && p->static_text)
								snprintf(_g_vhdr_, sizeof(_g_vhdr_), "%lu", strlen(p->static_text));
						else if (p->rc_type == RCT_MAPPING && p->p_mapping) {
							if (p->p_mapping->_exec())
								_g_vhdr_[0] = 0;
							else {
								_g_vhdr_[0] = '0';
								_g_vhdr_[1] = 0;
							}
						} else
							_g_vhdr_[0] = 0;

						return _g_vhdr_;
					}},
	{ RES_CONTENT_TYPE,		[] (_resp_t *p) -> _cstr_t {
						_cstr_t r = NULL;

						if (p->rc_type == RCT_STATIC && p->static_text)
							r = "text/html";
						else {
							if (p->path && p->b_st) {
								mime_open();
								r = mime_resolve(p->path);
							}
						}

						return r;
					}},
	{ RES_CONTENT_ENCODING,		[] (_resp_t *p) -> _cstr_t {
						return "";
					}},
	{ RES_DATE,			[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						time_t _time = time(NULL);
						struct tm *_tm = gmtime(&_time);

						strftime(_g_vhdr_, sizeof(_g_vhdr_),
								 "%a, %d %b %Y %H:%M:%S GMT", _tm);
						return _g_vhdr_;
					}},
	{ RES_EXPIRES,			[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						time_t _time = time(NULL) + (72 * 60 * 60);
						struct tm *_tm = gmtime(&_time);

						strftime(_g_vhdr_, sizeof(_g_vhdr_),
								 "%a, %d %b %Y %H:%M:%S GMT", _tm);
						return _g_vhdr_;
					}},
	{ RES_LAST_MODIFIED,		[] (_resp_t *p) -> _cstr_t {
						if (p->b_st) {
							tm *_tm = gmtime(&(p->st.st_mtime));

							strftime(_g_vhdr_, sizeof(_g_vhdr_),
								 "%a, %d %b %Y %H:%M:%S GMT", _tm);
						} else
							_g_vhdr_[0] = 0;

						return _g_vhdr_;
					}},
	{ RES_CONNECTION,		[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						_cstr_t connection = getenv(REQ_CONNECTION);

						return (connection) ? connection : "Close";
					}},
	{ RES_SERVER,			[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						return SERVER_NAME;
					}},
	{ NULL,				NULL }
};

static _err_t send_header(_resp_t *p) {
	_err_t r = E_OK;
	_cstr_t protocol = (p->protocol) ? p->protocol : "HTTP/1.1";
	_cstr_t text = _g_resp_text_[p->rc];
	char *header_append = NULL;
	int i = 0, n = 0;

	// First response line
	i += snprintf(p->header + i, p->sz_hbuffer - i, "%s %d %s\r\n",
			protocol, p->rc, (text) ? text : "Unknown");

	if (p->i_method == METHOD_HEAD)
		goto _eoh_;

	if (p->rc_type == RCT_MAPPING && p->p_mapping)
		header_append = p->p_mapping->_header_append();

	while (_g_hdef_[n].var) {
		_cstr_t val = _g_hdef_[n].vcb(p);
		bool set = true;

		if (header_append && header_append[0])
			set = strcasestr(header_append, _g_hdef_[n].var) == NULL;

		if (set && val && val[0])
			i += snprintf(p->header + i, p->sz_hbuffer - 1, "%s: %s\r\n", _g_hdef_[n].var, val);

		n++;
	}

	// header append
	if (header_append)
		i += str_resolve(header_append, p->header + i, p->sz_hbuffer - i);

_eoh_:
	// EOH
	strncpy(p->header + i, "\r\n", p->sz_hbuffer - i);
	i += 2;

	io_write(p->header, i);

	return r;
}

static _err_t send_exec(_cstr_t cmd) {
	_err_t r = E_FAIL;
	_str_t argv[256];
	_proc_t proc;
	int i = 0;

	memset(argv, 0, sizeof(argv));
	split_by_space(cmd, strlen(cmd), argv, 256);

	signal(SIGCHLD, [](__attribute__((unused)) int sig) {});

	TRACE("http[%d] Execute '%s'\n", getpid(), cmd);
	if (proc_exec_v(&proc, argv[0], argv) == 0) {
		int nb = 0;

		do {
			if ((nb = io_verify_input()) > 0) {
				nb = io_read(_g_resp_buffer_, sizeof(_g_resp_buffer_));
				proc_write(&proc, _g_resp_buffer_, nb);
			}

			ioctl(proc.PREAD_FD, FIONREAD, &nb);
			if (nb > 0) {
				nb = proc_read(&proc, _g_resp_buffer_, sizeof(_g_resp_buffer_));
				io_write(_g_resp_buffer_, nb);
			}
		} while (proc_status(&proc) == -1);

		r = E_DONE;
	}

	while (argv[i]) {
		free(argv[i]);
		i++;
	}

	return r;
}

static _err_t send_file_content(_resp_t *p) {
	_err_t r = E_FAIL;
	int fd = (p->path) ? open(p->path, O_RDONLY) : -1;
	int nb = 0;

	if (fd > 0) {
		while ((nb = read(fd, _g_resp_buffer_, sizeof(_g_resp_buffer_))) > 0)
			io_write(_g_resp_buffer_, nb);

		close(fd);
		r = E_OK;
	}

	return r;
}

static _err_t send_response(_resp_t *p) {
	_err_t r = E_FAIL;
	bool header = true;
	char *proc = NULL;
	bool exec = false;
	char path[MAX_PATH];

	if (p->i_method == METHOD_HEAD)
		goto _send_header_;

	// For mapping
	if (p->rc_type == RCT_MAPPING && p->p_mapping) {
		switch (p->p_mapping->type) {
			case MAPPING_TYPE_URL:
				proc = p->p_mapping->url._proc();
				header = p->p_mapping->url.header;
				exec = p->p_mapping->url.exec;
				break;
			case MAPPING_TYPE_ERR:
				proc = p->p_mapping->err._proc();
				header = p->p_mapping->err.header;
				exec = p->p_mapping->err.exec;
				break;
		}

		if (proc[0]) {
			str_resolve(proc, path, sizeof(path));

			if (exec) {
				if (header)
					send_header(p);

				r = send_exec(path);
			} else {
				p->path = path;

				if ((p->b_st = (stat(p->path, &(p->st)) == 0)))
					goto _send_file_;
				else {
					if (p->rc >= HTTPRC_BAD_REQUEST && (p->static_text = _g_resp_content_[p->rc])) {
						p->rc_type = RCT_STATIC;
						goto _send_static_text_;
					} else
						goto _send_header_;
				}
			}
		} else
			goto _send_header_;
	} else if (p->rc_type == RCT_STATIC && p->static_text) {
_send_static_text_:
		int l = strlen(p->static_text);

		if (l) {
			if ((r = send_header(p)) >= E_OK)
				io_write(p->static_text, l);
		}
	} else if (p->b_st) {
_send_file_:
		if (header)
			r = send_header(p);

		if (p->path)
			r = send_file_content(p);
	} else {
_send_header_:
		if (header)
			r = send_header(p);
	}

	return r;
}

_err_t send_error_response(_vhost_t *p_vhost, int rc) {
	_err_t r = E_FAIL;

	if (rc >= HTTPRC_BAD_REQUEST) {
		_resp_t resp;
		_mapping_t *p_err_map = (p_vhost) ? cfg_get_err_mapping(p_vhost->host, rc) : NULL;
		_cstr_t content = NULL;

		memset(&resp, 0, sizeof(_resp_t));
		resp.rc = rc;
		resp.p_vhost = p_vhost;
		resp.s_method = getenv(REQ_METHOD);
		resp.i_method = (resp.s_method) ? resolve_method(resp.s_method) : 0;
		resp.url = getenv(REQ_URL);
		resp.protocol = getenv(REQ_PROTOCOL);
		resp.header = _g_resp_buffer_;
		resp.sz_hbuffer = sizeof(_g_resp_buffer_);

		if (p_err_map) {
			resp.rc_type = RCT_MAPPING;
			resp.p_mapping = p_err_map;
		} else if ((content = _g_resp_content_[rc])) {
			resp.rc_type = RCT_STATIC;
			resp.static_text = content;
		}

		if ((r = send_response(&resp)) == E_OK)
			r = E_DONE;
	}

	return r;
}

_err_t res_processing(void) {
	_err_t r = E_FAIL;
	_cstr_t host = getenv(REQ_HOST);
	_vhost_t *p_vhost = cfg_get_vhost(host);

	if (p_vhost) {
		_resp_t resp;
		char doc_path[MAX_PATH+1];
		char resolved_path[PATH_MAX];

		memset(&resp, 0, sizeof(_resp_t));
		_g_resp_buffer_[0] = 0;
		setenv(DOC_ROOT, p_vhost->root, 1);
		cfg_load_mapping(p_vhost);

		resp.p_vhost = p_vhost;
		resp.url = getenv(REQ_URL);
		resp.s_method = getenv(REQ_METHOD);
		resp.protocol = getenv(REQ_PROTOCOL);
		resp.header = _g_resp_buffer_;
		resp.sz_hbuffer = sizeof(_g_resp_buffer_);

		if (resp.s_method && resp.url && resp.protocol) {
			resp.i_method = (resp.s_method) ? resolve_method(resp.s_method) : 0;
			resp.protocol = getenv(REQ_PROTOCOL);

			if (resp.i_method == METHOD_GET || resp.i_method == METHOD_POST || resp.i_method == METHOD_HEAD) {
				if ((resp.p_mapping = cfg_get_url_mapping(p_vhost->host, resp.s_method, resp.url))) {
					resp.rc_type = RCT_MAPPING;
					resp.rc = (resp.p_mapping->url.resp_code) ? resp.p_mapping->url.resp_code : HTTPRC_OK;
				} else {
					snprintf(doc_path, sizeof(doc_path), "%s%s", p_vhost->root, resp.url);

					if ((resp.path = realpath(doc_path, resolved_path))) {
						if (memcmp(resp.path, p_vhost->root, strlen(p_vhost->root)) == 0) {
							if ((resp.b_st = (stat(resp.path, &resp.st) == 0)))
								resp.rc = HTTPRC_OK;
							else
								resp.rc = HTTPRC_NOT_FOUND;
						} else {
							resp.rc = HTTPRC_FORBIDDEN;
							resp.path = NULL;
						}
					} else
						resp.rc = HTTPRC_NOT_FOUND;
				}
			} else
				resp.rc = HTTPRC_METHOD_NOT_ALLOWED;
		} else
			resp.rc = HTTPRC_BAD_REQUEST;

		if (resp.rc >= HTTPRC_BAD_REQUEST) {
			TRACE("http[%d]: %s '%s'\n", getpid(), _g_resp_text_[resp.rc], resp.url);
			if ((resp.p_mapping = cfg_get_err_mapping(p_vhost->host, resp.rc)))
				resp.rc_type = RCT_MAPPING;
			else if ((resp.static_text = _g_resp_content_[resp.rc]))
				resp.rc_type = RCT_STATIC;
		}

		r = send_response(&resp);
	}

	return r;
}
