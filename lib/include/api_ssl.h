#ifndef __API_SSL_H__
#define __API_SSL_H__

#include "config.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "dtype.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
SSL internal ali
*/
void ssl_init(void);
const SSL_METHOD *ssl_select_method(_cstr_t method);
_bool ssl_load_cert(SSL_CTX *ssl_cxt, _cstr_t cert);
_bool ssl_load_key(SSL_CTX *ssl_cxt, _cstr_t key);
SSL_CTX *ssl_create_context(const SSL_METHOD *method);
_cstr_t ssl_error_string(void);

#ifdef __cplusplus
}
#endif

#endif

