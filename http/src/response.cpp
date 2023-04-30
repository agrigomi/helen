#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include "http.h"
#include "trace.h"

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
	{ HTTPRC_FORBIDDEN,		"<!DOCTYPE html><html><head></head><body><h1>Forbidden path #403</h1></body></html>" }
};

static const char *methods[] = { "GET", "HEAD", "POST",
				"PUT", "DELETE", "CONNECT",
				"OPTIONS", "TRACE", NULL };
#define METHOD_GET	0
#define METHOD_HEAD	1
#define METHOD_POST	2
#define METHOD_PUT	3
#define METHOD_DELETE	4
#define METHOD_CONNECT	5
#define METHOD_OPTIONS	6
#define METHOD_TRACE	7

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

static void send_header(int rc, _cstr_t doc = NULL, size_t size = 0) {
	_cstr_t protocol = getenv(REQ_PROTOCOL);
	_cstr_t text = _g_resp_text_[rc];

	if (!protocol)
		protocol = "HTTP/1.1";

	if (!text)
		text = "Unknown";

	io_fwrite("%s %d %s\r\n", protocol, rc, text);

	//...

	send_env_var(RES_SERVER, "Server");
	if (size)
		io_fwrite("%s: %d\r\n", "Content-Length", size);
}

static void send_eoh(void) {
	io_write("\r\n", 2);
}

static _err_t send_file(_cstr_t doc, struct stat *pst) {
	_err_t r = E_FAIL;
	int fd = open(doc, O_RDONLY);
	int nb = 0;

	if (fd > 0) {
		// ??? ...
		send_header(HTTPRC_OK, doc, pst->st_size);
		send_eoh();

		while ((nb = read(fd, _g_resp_buffer_, sizeof(_g_resp_buffer_))) > 0)
			io_write(_g_resp_buffer_, nb);

		// ... ????

		close(fd);
		r = E_OK;
	}

	return r;
}

_err_t send_error_response(_vhost_t *p_vhost, int rc) {
	_err_t r = E_OK;
	_mapping_t *p_err_map = (p_vhost) ? cfg_get_err_mapping(p_vhost->host, rc) : NULL;

	if (p_err_map) {
		//...
	} else {
		_cstr_t content = _g_resp_content_[rc];
		int len = (content) ? strlen(content) : 0;

		send_header(rc, NULL, len);
		send_eoh();

		if (content)
			io_write(content, len);
		else
			r = E_FAIL;
	}

	return r;
}

static _err_t send_mapped_response(_vhost_t *p_vhost, _mapping_t *p_url_map) {
	_err_t r = E_OK;

	//...

	return r;
}

static _err_t send_directory_response(_vhost_t *p_vhost, _cstr_t doc) {
	_err_t r = E_OK;

	//...

	return r;
}


static _err_t send_document_response(_vhost_t *p_vhost, _cstr_t doc, struct stat *p_st) {
	_err_t r = E_FAIL;

	switch (p_st->st_mode & S_IFMT) {
		case S_IFDIR:
			r = send_directory_response(p_vhost, doc);
			break;
		case S_IFLNK:
		case S_IFREG:
			if ((r = send_file(doc, p_st)) != E_OK)
				r = send_error_response(p_vhost, HTTPRC_INTERNAL_SERVER_ERROR);
			break;
		default:
			r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
	}

	return r;
}

static _err_t send_response(_vhost_t *p_vhost, _cstr_t doc, _cstr_t url) {
	_err_t r = E_FAIL;
	_cstr_t method = getenv(REQ_METHOD);

	if (method) {
		_mapping_t *p_url_map = cfg_get_url_mapping(p_vhost->host, method, url);

		if (p_url_map)
			r = send_mapped_response(p_vhost, p_url_map);
		else {
			struct stat st;

			if (stat(doc, &st) == 0) {
				switch (resolve_method(method)) {
					case METHOD_GET:
					case METHOD_POST:
						r = send_document_response(p_vhost, doc, &st);
						break;
					case METHOD_HEAD:
						send_header(HTTPRC_OK, doc, st.st_size);
						send_eoh();
						r = E_OK;
						break;
					default:
						r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
				}
			} else
				r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
		}
	} else
		r = send_error_response(p_vhost, HTTPRC_BAD_REQUEST);

	return r;
}

_err_t res_processing(void) {
	_err_t r = E_FAIL;
	_cstr_t host = getenv(REQ_HOST);
	_vhost_t *p_vhost = cfg_get_vhost(host);

	if (p_vhost) {
		_cstr_t url = getenv(REQ_URL);

		if (url) {
			char doc_path[MAX_PATH+1];
			char resolved_path[PATH_MAX];

			snprintf(doc_path, sizeof(doc_path), "%s%s", p_vhost->root, url);

			char *_url = realpath(doc_path, resolved_path);

			if (_url) {
				if (memcmp(p_vhost->root, _url, strlen(p_vhost->root)) == 0) {
					setenv(DOC_ROOT, p_vhost->root, 1);
					cfg_load_mapping(p_vhost);

					r = send_response(p_vhost, _url, url);
				} else {
					TRACE("http[%d] Trying to access outside of root path '%s'\n", getpid(), _url);
					send_error_response(p_vhost, HTTPRC_FORBIDDEN);
				}
			} else {
				r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
				TRACE("http[%d]: Not found '%s'\n", getpid(), doc_path);
			}
		}
	} else
		send_error_response(p_vhost, HTTPRC_INTERNAL_SERVER_ERROR);

	return r;
}

