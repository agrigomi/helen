#include <string.h>
#include <map>
#include <zlib.h>
#include "http.h"
#include "str.h"
#include "argv.h"
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
	int r = -1, n = 0;;

	for (; methods[n]; n++) {
		if (strcasecmp(method, methods[n]) == 0) {
			r = n;
			break;
		}
	}

	return r;
}

bool rt_allow_executables(void) {
	static bool allow_exec = argv_check(OPT_EXEC);

	return allow_exec;
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

_err_t rt_gzip_buffer(const unsigned char *src, long unsigned int sz_src,
		unsigned char *dst, long unsigned int *psz_dst) {
	_err_t r = E_FAIL;
	z_stream zs;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = sz_src;
	zs.next_in = (unsigned char *)src;
	zs.avail_out = *psz_dst;
	zs.next_out = dst;

	// hard to believe they don't have a macro for gzip encoding, "Add 16" is the best thing zlib can do:
	// "Add 16 to windowBits to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper"
	if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
		deflate(&zs, Z_FINISH);
		deflateEnd(&zs);
		*psz_dst = zs.total_out;
		r = E_OK;
	}

	return r;
}

_err_t rt_compress_buffer(const unsigned char *src, long unsigned int sz_src,
		unsigned char *dst, long unsigned int *psz_dst, char **pp_type) {
	_err_t r = E_FAIL;
	unsigned int enc = rt_select_encoding(NULL);

	if (enc & ENCODING_GZIP)
		r = rt_gzip_buffer(src, sz_src, dst, psz_dst);
	else if(enc & ENCODING_DEFLATE)
		r = rt_deflate_buffer(src, sz_src, dst, psz_dst);

	*pp_type = (char *)rt_encoding_bit_to_name(&enc);

	return r;
}

static _err_t do_z_deflate(z_stream *p_strm, int fd_in, int fd_out) {
	_err_t r = E_OK;
	_uchar_t inb[MAX_COMPRESSION_CHUNK];
	_uchar_t outb[MAX_COMPRESSION_CHUNK];
	int flush = Z_NO_FLUSH;

	do {
		if ((p_strm->avail_in = read(fd_in, inb, sizeof(inb))) < MAX_COMPRESSION_CHUNK)
			flush = Z_FINISH;

		p_strm->next_in = inb;

		do {
			p_strm->avail_out = MAX_COMPRESSION_CHUNK;
			p_strm->next_out = outb;

			int zr = deflate(p_strm, flush);

			if (zr == Z_STREAM_ERROR || zr == Z_ERRNO) {
				r = E_FAIL;
				flush = Z_FINISH;
				break;
			}

			if (p_strm->avail_out <= MAX_COMPRESSION_CHUNK)
				write(fd_out, outb, MAX_COMPRESSION_CHUNK - p_strm->avail_out);
			else {
				flush = Z_FINISH;
				break;
			}
		} while (p_strm->avail_out == 0);
	} while (flush != Z_FINISH);

	deflateEnd(p_strm);

	return r;
}

_err_t rt_deflate_stream(int fd_in, int fd_out) {
	_err_t r = E_FAIL;
	z_stream strm;

	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) == Z_OK)
		r = do_z_deflate(&strm, fd_in, fd_out);

	return r;
}

_err_t rt_gzip_stream(int fd_in, int fd_out) {
	_err_t r = E_FAIL;
	z_stream strm;

	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) == Z_OK)
		r = do_z_deflate(&strm, fd_in, fd_out);

	return r;
}

_err_t rt_compress_stream(int encoding, int fd_in, int fd_out, _cstr_t *str_encoding) {
	int r = E_FAIL;

	if (encoding & ENCODING_GZIP) {
		if ((r = rt_gzip_stream(fd_in, fd_out)) == E_OK)
			*str_encoding = STR_ENCODING_GZIP;
	} else if (encoding & ENCODING_DEFLATE) {
		if ((r = rt_deflate_stream(fd_in, fd_out)) == E_OK)
			*str_encoding = STR_ENCODING_DEFLATE;
	}

	return r;
}

typedef struct {
	_cstr_t 	name;
	unsigned int	bit;
}_enc_t;

static _enc_t _g_enc_[] = {
	{ STR_ENCODING_BR,		ENCODING_BR },
	{ STR_ENCODING_GZIP,		ENCODING_GZIP },
	{ STR_ENCODING_DEFLATE,		ENCODING_DEFLATE },
	{ NULL,				0 }
};

unsigned int rt_parse_encoding(_cstr_t str_alg) {
	unsigned int r = 0;

	if (str_alg) {
		int n = 0;
		_char_t lb[256];
		char *rest = NULL;
		char *token = NULL;

		strncpy(lb, str_alg, sizeof(lb) - 1);

		if ((token = strtok_r(lb, ",", &rest))) {
			_char_t str[64] = "";

			do {
				strncpy(str, token, sizeof(str) - 1);
				str_trim(str);
				n = 0;

				while (_g_enc_[n].name) {
					if (strcasecmp(_g_enc_[n].name, str) == 0) {
						r |= _g_enc_[n].bit;
						break;
					}
					n++;
				}
			} while ((token = strtok_r(NULL, ",", &rest)));
		}
	}

	return r;
}

_cstr_t rt_encoding_bit_to_name(unsigned int *encoding_bit) {
	_cstr_t r = NULL;
	int n = 0;

	while (_g_enc_[n].name) {
		if (*encoding_bit & _g_enc_[n].bit) {
			r = _g_enc_[n].name;
			*encoding_bit = _g_enc_[n].bit;
			break;
		}

		n++;
	}

	return r;
}

unsigned int rt_select_encoding(_cstr_t ext /* file extension */) {
	unsigned int r = 0;
	unsigned int ext_mask = 0;

	if (ext) {
		_cstr_t host = getenv(REQ_HOST);
		_vhost_t *p_host = cfg_get_vhost(host);
		_mapping_t *p_ext_map = cfg_get_ext_mapping(p_host->host, ext);

		if (p_ext_map) {
			_cstr_t acc_enc = getenv(REQ_ACCEPT_ENCODING);

			r = rt_parse_encoding(acc_enc);
			ext_mask = rt_parse_encoding(p_ext_map->ext._compression());
		}
	}

	return r & ext_mask & SUPPORTED_ENCODING;
}

void rt_sha1_string(_cstr_t data, _str_t out, int sz_out) {
	SHA1Context sha1_cxt;
	unsigned char sha1_result[SHA1HashSize];
	static const char *hex = "0123456789abcdef";
	int i = 0, j = 0;;

	SHA1Reset(&sha1_cxt);
	SHA1Input(&sha1_cxt, (unsigned char *)data, strlen(data));
	SHA1Result(&sha1_cxt, sha1_result);

	for (; i < SHA1HashSize && j < sz_out; i++, j += 2) {
		out[j] = hex[(sha1_result[i] >> 4) & 0x0f];
		out[j + 1] = hex[sha1_result[i] & 0x0f];
	}
}
