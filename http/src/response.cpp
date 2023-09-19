#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <map>
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

static int resolve_method(_cstr_t method) {
	int n = 0;

	for (; methods[n]; n++) {
		if (strcasecmp(method, methods[n]) == 0)
			break;
	}

	return n;
}

static void send_env_var(_cstr_t var, _cstr_t http_var) {
	_cstr_t val = getenv(var);

	if (val)
		io_fwrite("%s: %s\r\n", http_var, val);
}

static void send_header(int rc, _cstr_t doc = NULL, size_t size = 0, struct stat *p_stat = NULL) {
	_cstr_t protocol = getenv(REQ_PROTOCOL);
	_cstr_t text = _g_resp_text_[rc];
	char value[128];

	if (!protocol)
		protocol = "HTTP/1.1";

	if (!text)
		text = "Unknown";

	io_fwrite("%s %d %s\r\n", protocol, rc, text);

	if (p_stat) {
		tm *_tm = gmtime(&(p_stat->st_mtime));

		strftime(value, sizeof(value), "%a, %d %b %Y %H:%M:%S GMT", _tm);
		io_fwrite("%s: %s\r\n", RES_LAST_MODIFIED, value);
	}

	//if (doc) {
		//...
	//}
	//...

	send_env_var(RES_SERVER, "Server");

	if (size)
		io_fwrite("%s: %d\r\n", RES_CONTENT_LENGTH, size);
	else if (p_stat)
		io_fwrite("%s: %d\r\n", RES_CONTENT_LENGTH, p_stat->st_size);
}

static void send_eoh(void) {
	io_write("\r\n", 2);
}

static _err_t send_file_content(_cstr_t doc) {
	_err_t r = E_FAIL;
	int fd = open(doc, O_RDONLY);
	int nb = 0;

	if (fd > 0) {
		while ((nb = read(fd, _g_resp_buffer_, sizeof(_g_resp_buffer_))) > 0)
			io_write(_g_resp_buffer_, nb);

		close(fd);
		r = E_OK;
	}

	return r;
}

static _err_t send_directory_response(_vhost_t *p_vhost, _cstr_t method, _cstr_t url, _cstr_t dir, struct stat *p_stat);

static _err_t send_file(_cstr_t url, _cstr_t doc, struct stat *pst) {
	/* extract directory from URL and verify for mapping
	??? ... */
	send_header(HTTPRC_OK, doc, pst->st_size, pst);
	send_eoh();
	return send_file_content(doc);
	// ... ????
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

static _err_t send_exec(_str_t cmd) {
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

static _err_t send_mapped_err(_mapping_err_t *p_err_map) {
	_err_t r = E_FAIL;
	char *header_append = p_err_map->_header_append();
	char *proc = p_err_map->_proc();
	int append_len = 0;

	if (p_err_map->header) {
		if (header_append[0])
			append_len = str_resolve(header_append, _g_resp_buffer_, sizeof(_g_resp_buffer_));

		send_header(p_err_map->code);
		if (append_len)
			io_write(_g_resp_buffer_, append_len);
	}

	if (proc[0]) {
		str_resolve(proc, _g_resp_buffer_, sizeof(_g_resp_buffer_));

		if (p_err_map->exec) {
			if (p_err_map->header)
				send_eoh();
			r = send_exec(_g_resp_buffer_);
		} else {
			struct stat st;

			if (stat(_g_resp_buffer_, &st) == 0) {
				if (p_err_map->header) {
					char value[128];
					tm *_tm = gmtime(&(st.st_mtime));

					io_fwrite("%s: %d\r\n", RES_CONTENT_LENGTH, st.st_size);
					strftime(value, sizeof(value), "%a, %d %b %Y %H:%M:%S GMT", _tm);
					io_fwrite("%s: %s\r\n", RES_LAST_MODIFIED, value);
					send_eoh();
				}

				r = send_file_content(_g_resp_buffer_);
			}
		}
	}

	return r;
}

_err_t send_error_response(_vhost_t *p_vhost, int rc) {
	_err_t r = E_FAIL;
	_mapping_t *p_err_map = (p_vhost) ? cfg_get_err_mapping(p_vhost->host, rc) : NULL;

	if (p_err_map)
		r = send_mapped_err(&(p_err_map->err));
	else {
		_cstr_t content = _g_resp_content_[rc];
		int len = (content) ? strlen(content) : 0;

		send_header(rc, NULL, len);
		send_eoh();

		if (content) {
			io_write(content, len);
			r = E_OK;
		}
	}

	return r;
}

static _err_t send_mapped_response(_mapping_url_t *p_url_map) {
	_err_t r = E_FAIL;
	char *header_append = p_url_map->_header_append();
	char *proc = p_url_map->_proc();
	int append_len = 0;

	if (p_url_map->header) {
		if (header_append && header_append[0])
			append_len = str_resolve(header_append, _g_resp_buffer_, sizeof(_g_resp_buffer_));

		send_header((p_url_map->resp_code) ? p_url_map->resp_code : 200);
		if (append_len)
			io_write(_g_resp_buffer_, append_len);
	}

	if (proc[0]) {
		str_resolve(proc, _g_resp_buffer_, sizeof(_g_resp_buffer_));

		if (p_url_map->exec) {
			if (p_url_map->header)
				send_eoh();
			r = send_exec(_g_resp_buffer_);
		} else {
			struct stat st;

			if (stat(_g_resp_buffer_, &st) == 0) {
				if (p_url_map->header) {
					char value[128];
					tm *_tm = gmtime(&(st.st_mtime));

					io_fwrite("%s: %d\r\n", RES_CONTENT_LENGTH, st.st_size);
					strftime(value, sizeof(value), "%a, %d %b %Y %H:%M:%S GMT", _tm);
					io_fwrite("%s: %s\r\n", RES_LAST_MODIFIED, value);
					send_eoh();
				}

				r = send_file_content(_g_resp_buffer_);
			}
		}
	}

	return r;
}

static _err_t send_directory_response(_vhost_t *p_vhost, _cstr_t method, _cstr_t url, _cstr_t dir, struct stat *p_stat) {
	_err_t r = E_FAIL;

	cfg_load_mapping(p_vhost);

	_mapping_t *p_url_map = cfg_get_url_mapping(p_vhost->host, method, url);
	if (p_url_map)
		r = send_mapped_response(&(p_url_map->url));

	return r;
}

static _err_t send_unresolved_path(_vhost_t *p_vhost, _cstr_t method, _cstr_t url, _cstr_t req_doc, _cstr_t err_path) {
	_err_t r = E_FAIL;

	cfg_load_mapping(p_vhost);

	_mapping_t *p_url_map = cfg_get_url_mapping(p_vhost->host, method, url);

	if (p_url_map)
		r = send_mapped_response(&(p_url_map->url));
	else {
		r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
		TRACE("http[%d]: Not found (error path) '%s'\n", getpid(), err_path);
	}

	return r;
}

static _err_t send_document_response(_vhost_t *p_vhost, _cstr_t method, _cstr_t url, _cstr_t doc, struct stat *p_st) {
	_err_t r = E_FAIL;

	switch (p_st->st_mode & S_IFMT) {
		case S_IFDIR:
			r = send_directory_response(p_vhost, method, url, doc, p_st);
			break;
		case S_IFLNK:
		case S_IFREG:
			r = send_file(url, doc, p_st);
			break;
		default:
			r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
	}

	return r;
}

static _err_t send_response(_vhost_t *p_vhost, int method, _cstr_t str_method, _cstr_t url, _cstr_t doc) {
	_err_t r = E_FAIL;
	struct stat st;

	if (stat(doc, &st) == 0) {
		switch (method) {
			case METHOD_GET:
			case METHOD_POST:
				r = send_document_response(p_vhost, str_method, url, doc, &st);
				break;
			case METHOD_HEAD:
				send_header(HTTPRC_OK, doc, st.st_size, &st);
				send_eoh();
				r = E_OK;
				break;
			default:
				r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
		}
	} else {
		/* extract directory from URL and verify for directory mapping */
		/* ... */

		r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
		TRACE("http[%d]: Not found '%s'\n", getpid(), doc);
	}

	return r;
}

static _err_t connect_to_url(_vhost_t *p_vhost, _cstr_t str_method, _cstr_t url) {
	return send_error_response(p_vhost, HTTPRC_NOT_IMPLEMENTED);
}

_err_t res_processing(void) {
	_err_t r = E_FAIL;
	_cstr_t host = getenv(REQ_HOST);
	_vhost_t *p_vhost = cfg_get_vhost(host);

	if (p_vhost) {
		_cstr_t url = getenv(REQ_URL);
		_cstr_t method = getenv(REQ_METHOD);

		setenv(DOC_ROOT, p_vhost->root, 1);

		if (url && method) {
			int m = resolve_method(method);

			switch (m) {
				case METHOD_GET:
				case METHOD_POST:
				case METHOD_HEAD: {
					char doc_path[MAX_PATH+1];
					char resolved_path[PATH_MAX];

					snprintf(doc_path, sizeof(doc_path), "%s%s", p_vhost->root, url);

					char *_url = realpath(doc_path, resolved_path);

					if (_url) {
						if (memcmp(p_vhost->root, _url, strlen(p_vhost->root)) == 0)
							r = send_response(p_vhost, m, method, url, _url);
						else {
							TRACE("http[%d] Trying to access outside of root path '%s'\n", getpid(), _url);
							send_error_response(p_vhost, HTTPRC_FORBIDDEN);
						}
					} else
						r = send_unresolved_path(p_vhost, method, url, doc_path, resolved_path);
				} break;
				case METHOD_CONNECT:
					r = connect_to_url(p_vhost, method, url);
					break;
				default:
					r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
			}
		} else
			r = send_error_response(p_vhost, HTTPRC_BAD_REQUEST);
	} else
		send_error_response(p_vhost, HTTPRC_INTERNAL_SERVER_ERROR);

	return r;
}

