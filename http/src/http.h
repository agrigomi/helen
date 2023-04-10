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


// HTTP request variables
#define REQ_METHOD	"REQ_METHOD"
#define REQ_URL		"REQ_URL"
#define REQ_PROTOCOL	"REQ_PROTOCOL"

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
	bool header;		// respond header by parent process
	bool exec;		// exec. flag
	bool no_stderr;		// dup2 for stderr
	char method[16];	// HTTP method
	char url[256];		// URL handler
	char proc[MAX_PATH];	// processing buffera

	unsigned int _size(void) {
		return sizeof(struct mapping_url) - sizeof(proc) + strlen(proc) + 1;
	}
};

#define PREFIX_RESP_CODE	"RC_"

struct __attribute__((packed)) mapping_err {
	short code;		// HTTP response code
	bool header;		// respond header by parent process
	bool exec;		// exec. flag
	bool no_stderr;		// dup2 for stderr
	char proc[MAX_PATH];	// processing buffer

	unsigned int _size(void) {
		return sizeof(struct mapping_err) - sizeof(proc) + strlen(proc) + 1;
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

		switch(type) {
			case MAPPING_TYPE_URL:
				r += url._size();
				break;
			case MAPPING_TYPE_ERR:
				r += err._size();
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
Read line from input stream
return line size (without \r\n) */
int io_read_line(char *buffer, int size, int timeout);
/**
return >0 for number of received bytes <=0 means fail */
int io_read(char *buffer, int size, int timeout);
/**
return >0 for number of sent bytes <=0 means fail */
int io_write(char *buffer, int size);

// Request

_err_t req_receive(int timeout);

#endif

