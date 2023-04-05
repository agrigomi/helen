#ifndef __HTTP_H__
#define __HTTP_H__

#include "api_ssl.h"
#include "err.h"

// command line options
#define OPT_HELP	"help"
#define OPT_SHELP	"h"
#define OPT_VER		"v"
#define OPT_DIR		"dir"
#define OPT_LISTEN	"l"
#define OPT_PORT	"p"
#define OPT_LPORT	"port"
#define OPT_SSL_CERT	"ssl-cert"
#define OPT_SSL_KEY	"ssl-key"
#define OPT_SSL_METHOD	"ssl-method"


// HTTP request variables
#define REQ_METHOD	"METHOD"
#define REQ_URL		"URL"
#define REQ_PROTOCOL	"PROTOCOL"

#define MAX_PATH	1024
#define MAX_HOST_NAME	256

extern SSL_CTX	*_g_ssl_context_;
extern SSL	*_g_ssl_in_;
extern SSL	*_g_ssl_out_;

typedef struct {
	char	host[MAX_HOST_NAME];
	char	root[MAX_PATH];
	int	timeout;
} _vhost_t;

#define MAPPING_TYPE_URL	1
#define MAPPING_TYPE_ERR	2

typedef struct {
	char method[16];	// HTTP method
	char url[256];		// URL handler
	bool header;		// respond header by parent process
	bool exec;		// exec. flag
	bool no_stderr;		// dup2 for stderr
	char proc[MAX_PATH];	// processing buffer
} _mapping_url_t;

#define PREFIX_RESP_CODE	"RC_"

typedef struct {
	short code;		// HTTP response code
	bool header;		// respond header by parent process
	bool exec;		// exec. flag
	bool no_stderr;		// dup2 for stderr
	char proc[MAX_PATH];	// processing buffer
} _mapping_err_t;

typedef struct {
	unsigned char type;
	union {
		_mapping_url_t	url;
		_mapping_err_t	err;
	};
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

// IO

/* returns E_OK for success */
_err_t io_init(void);


#endif

