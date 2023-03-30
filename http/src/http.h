#ifndef __HTTP_H__
#define __HTTP_H__

#include "api_ssl.h"

// command line options
#define OPT_HELP	"help"
#define OPT_SHELP	"h"
#define OPT_VER		"v"
#define OPT_DIR		"dir"
#define OPT_SSL_CERT	"ssl-cert"
#define OPT_SSL_KEY	"ssl-key"
#define OPT_SSL_METHOD	"ssl-method"


// HTTP request variables
#define REQ_METHOD	"METHOD"
#define REQ_URL		"URL"
#define REQ_PROTOCOL	"PROTOCOL"

extern SSL_CTX	*_g_ssl_context_;
extern SSL	*_g_ssl_in_;
extern SSL	*_g_ssl_out_;

#endif

