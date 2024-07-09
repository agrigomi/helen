#include <string.h>
#include <map>
#include <zlib.h>
#include "http.h"
#include "str.h"

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
	{ HTTPRC_RANGE_NOT_SATISFIABLE,	"Range Not Satisfiable" },
	{ HTTPRC_EXPECTATION_FAILED,	"Expectation Failed" },
	{ HTTPRC_UPGRADE_REQUIRED,	"Upgrade required" },
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
	{ HTTPRC_NOT_IMPLEMENTED,	"<!DOCTYPE html><html><head></head><body><h1>Not implemented #501</h1></body></html>" },
	{ HTTPRC_RANGE_NOT_SATISFIABLE,	"<!DOCTYPE html><html><head></head><body><h1>Range Not Satisfiable #416</h1></body></html>" }
};

static const char *methods[] = { "GET", "HEAD", "POST",
				"PUT", "DELETE", "CONNECT",
				"OPTIONS", "TRACE", "PATCH",
				NULL };

_cstr_t rt_resp_text(int rc) {
	return _g_resp_text_[rc];
}

_cstr_t rt_static_content(int rc) {
	return _g_resp_content_[rc];
}

int rt_resolve_method(_cstr_t method) {
	int n = 0;

	for (; methods[n]; n++) {
		if (strcasecmp(method, methods[n]) == 0)
			break;
	}

	return n;
}

_cstr_t rt_file_ext(_cstr_t path) {
	size_t l = strlen(path);

	while (l && path[l] != '.')
		l--;

	return path + l;
}

_err_t rt_deflate_buffer(const unsigned char *src, long unsigned int sz_src,
		unsigned char *dst, long unsigned int *psz_dst) {
	return compress(dst, psz_dst, src, sz_src);
}

unsigned int rt_parse_encoding(_cstr_t str_alg) {
	unsigned int r = 0;
	typedef struct {
		_cstr_t 	name;
		unsigned int	bit;
	}_enc_t;
	static _enc_t enc[] = {
		{ "br",		ENCODING_BR },
		{ "gzip",	ENCODING_GZIP },
		{ "deflate",	ENCODING_DEFLATE },
		{ NULL,		0 }
	};
	int n = 0;
	_char_t lb[256];
	char *rest = NULL;
	char *token = NULL;

	strncpy(lb, str_alg, sizeof(lb));

	if ((token = strtok_r(lb, ",", &rest))) {
		_char_t str[64] = "";

		do {
			strncpy(str, token, sizeof(str));
			str_trim(str);
			n = 0;

			while (enc[n].name) {
				if (strcasecmp(enc[n].name, str) == 0) {
					r |= enc[n].bit;
					break;
				}
				n++;
			}
		} while ((token = strtok_r(NULL, ",", &rest)));
	}

	return r;
}


