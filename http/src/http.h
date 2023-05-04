#ifndef __HTTP_H__
#define __HTTP_H__

#include <string.h>
#include "config.h"
#include "api_ssl.h"
#include "err.h"

// command line options
#define OPT_HELP	"help"
#define OPT_SHELP	"h"
#define OPT_VER		"v"
#define OPT_DIR		"dir"
#define OPT_LISTEN	"l"
#define OPT_PORT	"p"
#define OPT_TIMEOUT	"timeout"
#define OPT_SSL_CERT	"ssl-cert"
#define OPT_SSL_KEY	"ssl-key"
#define OPT_SSL_METHOD	"ssl-method"

#define SERVER_NAME	"Helen"

// Common environment variables
#define DOC_ROOT		"DOC_ROOT"

// HTTP request environment variables
#define REQ_METHOD		"REQ_METHOD"
#define REQ_URL			"REQ_URL"
#define REQ_PROTOCOL		"REQ_PROTOCOL"
#define REQ_CONNECTION		"REQ_CONNECTION"
#define REQ_ACCEPT		"REQ_ACCEPT"
#define REQ_ACCEPT_ENCODING	"REQ_ACCEPT_ENCODING"
#define REQ_HOST		"REQ_HOST"
#define REQ_USER_AGENT		"REQ_USER_AGENT"
#define REQ_ACCEPT_LANGUAGE	"REQ_ACCEPT_LANGUAGE"
#define REQ_REFERER		"REQ_REFERER"
#define REQ_PRAGMA		"REQ_PRAGMA"
#define REQ_CACHE_CONTROL	"REQ_CACHE_CONTROL"
#define REQ_RANGE		"REQ_RANGE"

// HTTP response environment variables
#define RES_SERVER		"RES_SERVER"

// HTTP response native variables
#define RES_CONTENT_LENGTH	"Content-Length"
#define RES_CONTENT_TYPE	"Content-Type"
#define RES_LAST_MODIFIED	"Last-Modified"

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
#define HTTPRC_EXPECTATION_FAILED	417 // Expectation Failed
#define HTTPRC_INTERNAL_SERVER_ERROR	500 // Internal Server Error
#define HTTPRC_NOT_IMPLEMENTED		501 // Not Implemented
#define HTTPRC_BAD_GATEWAY		502 // Bad Gateway
#define HTTPRC_SERVICE_UNAVAILABLE	503 // Service Unavailable
#define HTTPRC_GATEWAY_TIMEOUT		504 // Gateway Time-out
#define HTTPRC_VERSION_NOT_SUPPORTED	505 // HTTP Version not supported


#define MAX_PATH	1024
#define MAX_HOST_NAME	256

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

struct __attribute__((packed)) mapping_url {
	bool header;			// respond header by parent process
	bool exec;			// exec. flag
	bool no_stderr;			// dup2 for stderr
	short resp_code;		// HTTP response code
	char method[8];			// HTTP method
	short off_url;			// offset to URL
	short off_header_append;	// offset to rest of header
	short off_proc;			// offset to response command or file
	short buffer_len;		// size of data in buffer
	char buffer[2048];

	unsigned int _size(void) {
		return sizeof(struct mapping_url) - sizeof(buffer) + buffer_len;
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
};

#define PREFIX_RESP_CODE	"RC_"

struct __attribute__((packed)) mapping_err {
	short code;			// HTTP response code
	bool header;			// respond header by parent process
	bool exec;			// exec. flag
	bool no_stderr;			// dup2 for stderr
	short off_header_append;	// offset to rest of header
	short off_proc;			// offset to response command or file
	short buffer_len;		// size of data in buffer
	char buffer[2048];

	unsigned int _size(void) {
		return sizeof(struct mapping_err) - sizeof(buffer) + buffer_len;
	}

	char *_header_append(void) {
		return buffer + off_header_append;
	}

	char *_proc(void) {
		return buffer + off_proc;
	}
};

typedef struct mapping_url _mapping_url_t;
typedef struct mapping_err _mapping_err_t;

typedef struct __attribute__((packed)) {
	unsigned char type;
	union {
		_mapping_url_t	url;
		_mapping_err_t	err;
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
} _mapping_t;

// CFG

/* returns E_OK for success */
_err_t cfg_init(void);
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
_mapping_t *cfg_get_url_mapping(_vhost_t *, _cstr_t method, _cstr_t url);
/**
Get mapping record by vhost name and URL */
_mapping_t *cfg_get_url_mapping(_cstr_t host, _cstr_t method, _cstr_t url);
/**
Get mapping record by vhost record and response code */
_mapping_t *cfg_get_err_mapping(_vhost_t *, short rc);
/**
Get mapping record by vhost name and response code */
_mapping_t *cfg_get_err_mapping(_cstr_t host, short rc);

// IO

/* returns E_OK for success */
_err_t io_start(void);
/**
Wait for input with timeout in seconds
return number of bytes  */
int io_wait_input(int timeout);
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
_err_t req_receive(int timeout);

// Response
_err_t res_processing(void);
_err_t send_error_response(_vhost_t *p_vhost, int rc);

#endif

