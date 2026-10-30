#include <string.h>
#include <stdio.h>
#include "api_ssl.h"

static _char_t _g_error_string_[2048] = "";
typedef const SSL_METHOD *_ssl_methodcb_t(void);

typedef struct {
	_cstr_t	ind;
	_ssl_methodcb_t *cb_method;
}_ssl_method_t;

static _ssl_method_t _g_ssl_method_map[] = {
	{"SSLv23",	SSLv23_server_method},
	{"TLSv1_2",	TLSv1_2_server_method},
	{"DTLS",	DTLS_server_method},
	{"TLS",		TLS_server_method},
	{NULL,		NULL}
};

void ssl_init(void) {
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
}

const SSL_METHOD *ssl_select_method(_cstr_t method) {
	const SSL_METHOD *r = NULL;

	_u32 n = 0;

	while(_g_ssl_method_map[n].ind) {
		if(strcmp(_g_ssl_method_map[n].ind, method) == 0) {
			r = _g_ssl_method_map[n].cb_method();
			break;
		}

		n++;
	}

	if(!_g_ssl_method_map[n].ind)
		snprintf(_g_error_string_, sizeof(_g_error_string_),
			"Unsupported SSL method '%s'", method);

	return r;
}

_bool ssl_load_cert(SSL_CTX *ssl_cxt, _cstr_t cert) {
	_bool r = _false;

	if(SSL_CTX_use_certificate_file(ssl_cxt, cert, SSL_FILETYPE_PEM) > 0)
		r = _true;

	return r;
}

_bool ssl_load_key(SSL_CTX *ssl_cxt, _cstr_t key) {
	_bool r = _false;

	if(SSL_CTX_use_PrivateKey_file(ssl_cxt, key, SSL_FILETYPE_PEM) > 0) {
		if(SSL_CTX_check_private_key(ssl_cxt))
			r = _true;
	}

	return r;
}

SSL_CTX *ssl_create_context(const SSL_METHOD *method) {
	return SSL_CTX_new(method);
}

_cstr_t ssl_error_string(void) {
	_ulong err = ERR_get_error();

	if(err)
		ERR_error_string_n(err, _g_error_string_, sizeof(_g_error_string_));

	return _g_error_string_;
}

int ssl_read(SSL *pssl, void *buffer, int size) {
	return SSL_read(pssl, buffer, size);
}

int ssl_write(SSL *pssl, const void *buffer, int size) {
	return SSL_write(pssl, buffer, size);
}

int ssl_read_line(SSL *pssl, char *buffer, int size) {
	int r = -1;

	if (pssl != NULL && size > 1) {
		int total_read = 0;
		int is_line_complete = 0;
		char peek_buf[1024];

		/* Loop blocks and waits until a full line (\n) is composed or buffer fills up */
		while (!is_line_complete && total_read < (size - 1)) {
			int max_to_peek = (size - 1) - total_read;
			if (max_to_peek > (int)sizeof(peek_buf)) {
				max_to_peek = (int)sizeof(peek_buf);
			}

			/* Look into OpenSSL internal decrypted buffer without consuming the data */
			int bytes_peeked = SSL_peek(pssl, peek_buf, max_to_peek);

			if (bytes_peeked > 0) {
				peek_buf[bytes_peeked] = '\0';
				char *newline_ptr = strchr(peek_buf, '\n');

				if (newline_ptr != NULL) {
					/* Found the newline marker. Calculate how much to consume from this chunk */
					int chunk_length = (int)(newline_ptr - peek_buf) + 1;

					/* Actually drain the exact line slice from the SSL stream */
					int bytes_drained = SSL_read(pssl, buffer + total_read, chunk_length);
					if (bytes_drained > 0) {
						total_read += bytes_drained;
						is_line_complete = 1;
					} else {
						break; /* SSL read error or connection drop */
					}
				} else {
					/* No newline found yet. Consume the entire peeked chunk blocking-wise */
					/* so the next loop iteration can wait for fresh encrypted TLS records */
					int bytes_drained = SSL_read(pssl, buffer + total_read, bytes_peeked);
					if (bytes_drained > 0) {
						total_read += bytes_drained;
					} else {
						break;
					}
				}
			} else {
				/* SSL_peek returned <= 0. Check for structural errors or clean EOF */
				int ssl_err = SSL_get_error(pssl, bytes_peeked);
				if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
					continue; /* Retry if the underlying non-blocking socket requires more I/O */
				}
				break;
			}
		}

		/* Post-processing the final gathered string buffer safely */
		if (total_read > 0) {
			r = total_read;
			buffer[r] = '\0'; /* Always ensure safe null-termination */
		} else {
			r = -1; /* Signal an absolute failure or immediate SSL EOF */
		}
	}

	return r;
}
