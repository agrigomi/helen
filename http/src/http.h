#ifndef __HTTP_H__
#define __HTTP_H__

#include <string.h>
#include <vector>
#include "config.h"
#include "api_ssl.h"
#include "sha1.h"
#include "respawn.h"
#include "err.h"

// command line options
#define OPT_HELP		"help"
#define OPT_SHELP		"h"
#define OPT_VER			"v"
#define OPT_DIR			"dir"
#define OPT_LISTEN		"l"
#define OPT_PORT		"p"
#define OPT_EXEC		"x"
#define OPT_INTERFACE		"if"
#define OPT_TIMEOUT		"timeout"
#define OPT_SSL_CERT		"ssl-cert"
#define OPT_SSL_KEY		"ssl-key"
#define OPT_SSL_METHOD		"ssl-method"
#define OPT_PROXY		"proxy"
#define OPT_CACHE		"cache"

#define SERVER_NAME		"Helen"
#define ALLOW_METHOD		"GET, POST, HEAD"

#define SCHEME_FILE		"file"
#define SCHEME_HTTP		"http"
#define SCHEME_HTTPS		"https"


// Common environment variables
#define DOC_ROOT		"DOC_ROOT"

// HTTP request environment variables
#define REQ_METHOD		"REQ_METHOD"
#define REQ_URL			"REQ_URL"
#define REQ_SCHEME		"REQ_SCHEME"
#define REQ_AUTHORITY		"REQ_AUTHORITY"
#define REQ_DOMAIN		"REQ_DOMAIN"
#define REQ_PORT		"REQ_PORT"
#define REQ_URI			"REQ_URI"
#define REQ_URN			"REQ_URN"
#define REQ_PATH		"REQ_PATH"
#define REQ_PARAMETERS		"REQ_PARAMETERS"
#define REQ_ANCHOR		"REQ_ANCHOR"
#define REQ_PROTOCOL		"REQ_PROTOCOL"
#define REQ_UPGRADE		"REQ_UPGRADE"
#define REQ_CONNECTION		"REQ_CONNECTION"
#define REQ_PROXY_CONNECTION	"REQ_PROXY_CONNECTION"
#define REQ_ACCEPT		"REQ_ACCEPT"
#define REQ_ACCEPT_ENCODING	"REQ_ACCEPT_ENCODING"
#define REQ_HOST		"REQ_HOST"
#define REQ_USER_AGENT		"REQ_USER_AGENT"
#define REQ_ACCEPT_LANGUAGE	"REQ_ACCEPT_LANGUAGE"
#define REQ_REFERER		"REQ_REFERER"
#define REQ_PRAGMA		"REQ_PRAGMA"
#define REQ_CACHE_CONTROL	"REQ_CACHE_CONTROL"
#define REQ_RANGE		"REQ_RANGE"
#define REQ_IF_RANGE		"REQ_IF_RANGE"
#define REQ_DIR			"REQ_DIR"
#define REQ_FILE		"REQ_FILE"
#define REQ_MISSING		"REQ_MISSING"

// HTTP response environment variables
#define RES_ENV_SERVER		"RES_SERVER"
#define RES_ENV_ALLOW		"RES_ALLOW"
#define RES_PROTOCOL		"RES_PROTOCOL"

// HTTP response native variables
#define RES_ALLOW		"Allow"
#define RES_ACCEPT_POST		"Accept-Post"
#define RES_ACCEPT_RANGES	"Accept-Ranges"
#define RES_CONTENT_LENGTH	"Content-Length"
#define RES_CONTENT_TYPE	"Content-Type"
#define RES_CONTENT_ENCODING	"Content-Encoding"
#define RES_CONTENT_LANGUAGE	"Content-Language"
#define RES_CONTENT_DISPOSITION	"Content-Disposition"
#define RES_CONTENT_RANGE	"Content-Range"
#define RES_DATE		"Date"
#define RES_LAST_MODIFIED	"Last-Modified"
#define RES_TRANSFER_ENCODING	"Transfer-Encoding"
#define RES_CONNECTION		"Connection"
#define RES_EXPIRES		"Expires"
#define RES_UPGRADE		"Upgrade"
#define RES_SERVER		"Server"
#define RES_CACHE_CONTROL	"Cache-Control"
#define RES_SET_COOKIE		"Set-Cookie"
#define RES_LOCATION		"Location"
#define RES_XFRAME_OPTIONS	"X-Frame-Options"
#define RES_HEADER_FILE		"RES_HEADER_FILE"

// HTTP response code
#define HTTPRC_CONTINUE			100 // Continue
#define HTTPRC_SWITCHING_PROTOCOL	101 // Switching Protocols
#define HTTPRC_OK			200 // OK
#define HTTPRC_CREATED			201 // Created
#define HTTPRC_ACCEPTED			202 // Accepted
#define HTTPRC_NON_AUTH			203 // Non-Authoritative Information
#define HTTPRC_NO_CONTENT		204 // No Content
#define HTTPRC_RESET_CONTENT		205 // Reset Content
#define HTTPRC_PART_CONTENT		206 // Partial Content
#define HTTPRC_MULTICHOICES		300 // Multiple Choices
#define HTTPRC_MOVED_PERMANENTLY	301 // Moved Permanently
#define HTTPRC_FOUND			302 // Found
#define HTTPRC_SEE_OTHER		303 // See Other
#define HTTPRC_NOT_MODIFIED		304 // Not Modified
#define HTTPRC_USE_PROXY		305 // Use proxy
#define HTTPRC_TEMP_REDIRECT		307 // Temporary redirect
#define HTTPRC_BAD_REQUEST		400 // Bad Request
#define HTTPRC_UNAUTHORIZED		401 // Unauthorized
#define HTTPRC_PAYMENT_REQUIRED		402 // Payment Required
#define HTTPRC_FORBIDDEN		403 // Forbidden
#define HTTPRC_NOT_FOUND		404 // Not Found
#define HTTPRC_METHOD_NOT_ALLOWED	405 // Method Not Allowed
#define HTTPRC_NOT_ACCEPTABLE		406 // Not Acceptable
#define HTTPRC_PROXY_AUTH_REQUIRED	407 // Proxy Authentication Required
#define HTTPRC_REQUEST_TIMEOUT		408 // Request Time-out
#define HTTPRC_CONFLICT			409 // Conflict
#define HTTPRC_GONE			410 // Gone
#define HTTPRC_LENGTH_REQUIRED		411 // Length Required
#define HTTPRC_PRECONDITION_FAILED	412 // Precondition Failed
#define HTTPRC_REQ_ENTITY_TOO_LARGE	413 // Request Entity Too Large
#define HTTPRC_REQ_URI_TOO_LARGE	414 // Request-URI Too Large
#define HTTPRC_UNSUPPORTED_MEDIA_TYPE	415 // Unsupported Media Type
#define HTTPRC_RANGE_NOT_SATISFIABLE	416 // Range Not Satisfiable
#define HTTPRC_EXPECTATION_FAILED	417 // Expectation Failed
#define HTTPRC_UPGRADE_REQUIRED		426 // Upgrade protocol
#define HTTPRC_INTERNAL_SERVER_ERROR	500 // Internal Server Error
#define HTTPRC_NOT_IMPLEMENTED		501 // Not Implemented
#define HTTPRC_BAD_GATEWAY		502 // Bad Gateway
#define HTTPRC_SERVICE_UNAVAILABLE	503 // Service Unavailable
#define HTTPRC_GATEWAY_TIMEOUT		504 // Gateway Time-out
#define HTTPRC_VERSION_NOT_SUPPORTED	505 // HTTP Version not supported

#define METHOD_GET		0
#define METHOD_HEAD		1
#define METHOD_POST		2
#define METHOD_PUT		3
#define METHOD_DELETE		4
#define METHOD_CONNECT		5
#define METHOD_OPTIONS		6
#define METHOD_TRACE		7
#define METHOD_PATCH		8

// Encoding
#define	ENCODING_BR		(1 << 0)
#define	ENCODING_GZIP		(1 << 1)
#define ENCODING_DEFLATE	(1 << 2)

#define STR_ENCODING_BR		"br"
#define STR_ENCODING_GZIP	"gzip"
#define STR_ENCODING_DEFLATE	"deflate"

#define SUPPORTED_ENCODING	(ENCODING_GZIP | ENCODING_DEFLATE)

// Limits
#define MAX_PATH		1024
#define MAX_HOST_NAME		256
#define MAX_BUFFER_LEN		2048
#define MAX_COMPRESSION_CHUNK	16384
#define MAX_BOUNDARY		(SHA1HashSize * 2) + 1

#define COMPRESSION_TRESHOLD	1024

struct __attribute__((packed)) vhost {
	char	host[MAX_HOST_NAME];
	char	root[MAX_PATH];

	unsigned int _size(void) {
		return sizeof(struct vhost) - sizeof(root) + strlen(root) + 1;
	}
};

typedef struct vhost _vhost_t;

#define MAPPING_TYPE_URL	1
#define MAPPING_TYPE_ERR	2
#define MAPPING_TYPE_EXT	3

struct __attribute__((packed)) mapping_url {
	bool header;			// respond header by parent process
	bool exec;			// exec. flag
	bool no_stderr;			// dup2 for stderr
	bool input;			// use input
	short resp_code;		// HTTP response code
	char method[8];			// HTTP method
	short off_protocol;		// offset to req. protocols
	short off_url;			// offset to URL
	short off_header_append;	// offset to rest of header
	short off_proc;			// offset to response command or file
	short off_ext;			// extension offset
	short buffer_len;		// size of data in buffer
	char buffer[MAX_BUFFER_LEN];

	unsigned int _size(void) {
		return sizeof(struct mapping_url) - sizeof(buffer) + buffer_len;
	}

	char *_protocol(void) {
		return buffer + off_protocol;
	}

	char *_url(void) {
		return buffer + off_url;
	}

	char *_header_append(void) {
		return buffer + off_header_append;
	}

	char *_proc(void) {
		return buffer + off_proc;
	}

	char *_ext(void) {
		return buffer + off_ext;
	}

	bool _exec(void) {
		return exec;
	}

	bool _input(void) {
		return input;
	}

	bool _header(void) {
		return header;
	}
};

#define PREFIX_RESP_CODE	"RC_"

struct __attribute__((packed)) mapping_err {
	short code;			// HTTP response code
	bool header;			// respond header by parent process
	bool exec;			// exec. flag
	bool no_stderr;			// dup2 for stderr
	bool input;			// use input
	short off_header_append;	// offset to rest of header
	short off_proc;			// offset to response command or file
	short off_ext;			// extension offset
	short buffer_len;		// size of data in buffer
	char buffer[MAX_BUFFER_LEN];

	unsigned int _size(void) {
		return sizeof(struct mapping_err) - sizeof(buffer) + buffer_len;
	}

	char *_header_append(void) {
		return buffer + off_header_append;
	}

	char *_proc(void) {
		return buffer + off_proc;
	}

	char *_ext(void) {
		return buffer + off_ext;
	}

	bool _exec(void) {
		return exec;
	}

	bool _input(void) {
		return input;
	}

	bool _header(void) {
		return header;
	}
};

struct __attribute__((packed)) mapping_ext {
	short off_ident;
	short off_header_append;
	short off_compression;
	short buffer_len;
	char buffer[MAX_BUFFER_LEN];

	unsigned int _size(void) {
		return sizeof(struct mapping_ext) - sizeof(buffer) + buffer_len;
	}

	char *_ident(void) {
		return buffer + off_ident;
	}

	char *_header_append(void) {
		return buffer + off_header_append;
	}

	char *_compression(void) {
		return buffer + off_compression;
	}
};

typedef struct mapping_url _mapping_url_t;
typedef struct mapping_err _mapping_err_t;
typedef struct mapping_ext _mapping_ext_t;

typedef struct __attribute__((packed)) {
	unsigned char type;
	union {
		_mapping_url_t	url;
		_mapping_err_t	err;
		_mapping_ext_t	ext;
	};

	unsigned int _size(void) {
		unsigned int r = sizeof(type);

		switch (type) {
			case MAPPING_TYPE_URL:
				r += url._size();
				break;
			case MAPPING_TYPE_ERR:
				r += err._size();
				break;
			case MAPPING_TYPE_EXT:
				r += ext._size();
				break;
		}

		return r;
	}

	char *_protocol(void) {
		char *r = NULL;

		switch (type) {
			case MAPPING_TYPE_URL:
				r = url._protocol();
				break;
			case MAPPING_TYPE_ERR:
				break;
		}

		return r;
	}

	char *_header_append(void) {
		char *r = NULL;

		switch (type) {
			case MAPPING_TYPE_URL:
				r = url._header_append();
				break;
			case MAPPING_TYPE_ERR:
				r = err._header_append();
				break;
			case MAPPING_TYPE_EXT:
				r = ext._header_append();
				break;
		}

		return r;
	}

	char *_proc(void) {
		char *r = NULL;

		switch (type) {
			case MAPPING_TYPE_URL:
				r = url._proc();
				break;
			case MAPPING_TYPE_ERR:
				r = err._proc();
				break;
		}

		return r;
	}

	bool _exec(void) {
		bool r = false;

		switch (type) {
			case MAPPING_TYPE_URL:
				r = url._exec();
				break;
			case MAPPING_TYPE_ERR:
				r = err._exec();
				break;
		}

		return r;
	}

	char *_ext(void) {
		char *r = NULL;

		switch (type) {
			case MAPPING_TYPE_URL:
				r = url._ext();
				break;
			case MAPPING_TYPE_ERR:
				r = err._ext();
				break;
		}

		return r;
	}

	bool _input(void) {
		bool r = false;

		switch (type) {
			case MAPPING_TYPE_URL:
				r = url._input();
				break;
			case MAPPING_TYPE_ERR:
				r = err._input();
				break;
		}

		return r;
	}

	bool _header(void) {
		bool r = false;

		switch (type) {
			case MAPPING_TYPE_URL:
				r = url._header();
				break;
			case MAPPING_TYPE_ERR:
				r = err._header();
				break;
		}

		return r;
	}
} _mapping_t;

typedef struct {
	unsigned long begin; // start offset in content
	unsigned long end; // size in bytes
	_char_t	header[512]; // range header
} _range_t;

typedef std::vector<_range_t> _v_range_t;

// CFG

/* returns E_OK for success */
_err_t cfg_init(void);
void cfg_uninit(void);
/**
Returns pointer to _vhost_t ot NULL */
_vhost_t *cfg_get_vhost(_cstr_t host);
/**
Load vhost mapping by _vhost_t pointer */
_err_t cfg_load_mapping(_vhost_t *);
/**
Load mapping by name of virtual host */
_err_t cfg_load_mapping(_cstr_t);

/**
Get mapping record by vhost record and URL */
_mapping_t *cfg_get_url_mapping(_vhost_t *, _cstr_t method, _cstr_t url, _cstr_t proto);
/**
Get mapping record by vhost name and URL */
_mapping_t *cfg_get_url_mapping(_cstr_t host, _cstr_t method, _cstr_t url, _cstr_t proto);
/**
Get mapping record by vhost record and response code */
_mapping_t *cfg_get_err_mapping(_vhost_t *, short rc);
/**
Get mapping record by vhost name and response code */
_mapping_t *cfg_get_err_mapping(_cstr_t host, short rc);
/**
Get mapping record for fie extension by vhost and <.ext>>  */
_mapping_t *cfg_get_ext_mapping(_cstr_t host, _cstr_t ext);

// IO

/* returns E_OK for success */
_err_t io_start(void);
bool io_is_ssl(void);
SSL_CTX *io_get_ssl_context(void);

/**
Return number of bytes in input buffer */
int wait_input(int fd, int tmout);
int verify_input(int fd);
/**
Wait for input with timeout in seconds
return number of bytes  */
int io_wait_input(int timeout);
int io_verify_input(void);
int io_get_stdin_fd(void);
_err_t io_create_raw_client_connection(_cstr_t domain, int port, int *socket_fd);
_err_t io_create_ssl_client_connection(_cstr_t domain, int port, SSL **ssl_ctx);
void io_close_ssl_client_connection(SSL *ssl);
/**
Read line from input stream
return line size (without \r\n) */
int io_read_line(char *buffer, int size);
/**
return >0 for number of received bytes <=0 means fail */
int io_read(char *buffer, int size);
/**
return >0 for number of sent bytes <=0 means fail */
int io_write(const char *buffer, int size);
/**
formatted output
return >0 for number of sent bytes <=0 means fail */
int io_fwrite(const char *fmt, ...);
// Request
void set_env_var(char *vstr, const char *div);
_err_t req_receive(int timeout, int *req_len);
void req_decode_url(_cstr_t url);

// resp. tools
int rt_resolve_method(_cstr_t method);
_cstr_t rt_resp_text(int rc);
_cstr_t rt_static_content(int rc);
_cstr_t rt_file_ext(_cstr_t path);
_err_t rt_deflate_buffer(const unsigned char *src, long unsigned int sz_src,
		unsigned char *dst, long unsigned int *psz_dst);
_err_t rt_gzip_buffer(const unsigned char *src, long unsigned int sz_src,
		unsigned char *dst, long unsigned int *psz_dst);
_err_t rt_compress_buffer(const unsigned char *src, long unsigned int sz_src,
		unsigned char *dst, long unsigned int *psz_dst, char **pp_type);
_err_t rt_deflate_stream(int fd_in, int fd_out);
_err_t rt_gzip_stream(int fd_in, int fd_out);
_err_t rt_compress_stream(int encoding, int fd_in, int fd_out, _cstr_t *str_encoding);

/* returns encoding bit mask */
unsigned int rt_parse_encoding(_cstr_t str_alg);
/* returns encoding identifier */
_cstr_t rt_encoding_bit_to_name(unsigned int *encoding_bit);
/* returns encoding bitmask */
unsigned int rt_select_encoding(_cstr_t ext /* file extension */);
void rt_sha1_string(_cstr_t data, _str_t out, int sz_out);

// response ranges
_v_range_t *range_parse(_cstr_t req_range, _cstr_t path, _cstr_t boundary);
void range_generate_boundary(_cstr_t path, _str_t b, int sz = MAX_BOUNDARY);

// response exec
_err_t resp_exec_v(_cstr_t argv[],
		int (*out)(unsigned char *buf, unsigned int sz, void *udata),
		int (*in)(unsigned char *buf, unsigned int sz, void *udata),
		void *udata);
_err_t resp_exec(_cstr_t cmd,
		int (*out)(unsigned char *buf, unsigned int sz, void *udata),
		int (*in)(unsigned char *buf, unsigned int sz, void *udata),
		void *udata);
_err_t resp_exec(_cstr_t cmd, _proc_t *proc);

// Response
_err_t res_processing(void);
_err_t send_error_response(_vhost_t *p_vhost, int rc);
_err_t do_connect(_cstr_t method, _cstr_t scheme, _cstr_t domain, _cstr_t port, _cstr_t uri, _cstr_t proto);

// header
void hdr_init(void);
void hdr_set(_cstr_t var, _cstr_t val);
void hdr_set(_cstr_t var, int val);
void hdr_clear(void);
/* Returns size of header in bytes */
int hdr_export(_str_t hb, int sz);

// proxy
_err_t proxy_http(void);
_err_t proxy_https(void);
_err_t proxy_raw_connect(_cstr_t domain, int port);
_err_t proxy_ssl_connect(_cstr_t domain, int port);

// Mime Types
_err_t mime_open(void);
void mime_close(void);
_cstr_t mime_resolve(_cstr_t path);

// CACHE
void cache_init(_cstr_t path);
/* returns file descriptor for success or -1 for fail */
int cache_open(_cstr_t path/* in */,
		struct stat *p_stat,
		_cstr_t *encoding/* in/out Can be NULL */
		);

#endif

