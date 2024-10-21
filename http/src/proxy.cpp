#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "http.h"
#include "trace.h"

static int read_header(_str_t b, int s) {
	int sz_lb = 0, sz = 0;

	// Read the rest of the request
	while ((sz = io_read_line(b + sz_lb, s - sz_lb)) > 0) {
		sz_lb += sz;
		strcat(b, "\r\n");
		sz_lb += 2;
	}
	strcat(b, "\r\n");
	sz_lb += 2;

	// Read content
	sz = io_verify_input();
	if (sz)
		sz_lb += io_read(b + sz_lb, s - sz_lb);

	return sz_lb;
}

_err_t proxy_http(void) {
	_err_t r = E_FAIL;
	_cstr_t method = getenv(REQ_METHOD);
	_cstr_t domain = getenv(REQ_DOMAIN);
	_cstr_t port = getenv(REQ_PORT);
	_cstr_t uri = getenv(REQ_URI);
	_cstr_t proto = getenv(REQ_PROTOCOL);
	_char_t lb[4096] = "";
	int sfd = -1;
	int sz_lb = snprintf(lb, sizeof(lb), "%s %s %s\r\n", method, uri, proto);

	sz_lb += read_header(lb + sz_lb, sizeof(lb) - sz_lb);
	TRACE("http[%d] Request: %s\n", getpid(), lb);

	if ((r = io_create_raw_client_connection(domain, (port) ? atoi(port) : 80, &sfd)) == E_OK) {
		write(sfd, lb, sz_lb);

		// read ...
		if (wait_input(sfd, 30)) {
			int sz_respb = 256 * 1024;
			_str_t respb = (_str_t)malloc(sz_respb);
			int t = 100;
			size_t in = 0;

			while (t--) {
				if (verify_input(sfd)) {
					if ((in = read(sfd, respb, sz_respb)) > 0) {
						if (io_write(respb, in) <= 0)
							break;
					} else
						break;

					t = 100;
				} else
					usleep(10000);
			}

			free(respb);
		}

		close(sfd);
	} else {
		switch (r) {
			case E_FAIL:
				send_error_response(NULL, HTTPRC_INTERNAL_SERVER_ERROR);
				break;
			case E_RESOLVE:
				send_error_response(NULL, HTTPRC_NOT_FOUND);
				break;
		}
	}

	return r;
}

_err_t proxy_https(void) {
	_err_t r = E_FAIL;
	_cstr_t method = getenv(REQ_METHOD);
	_cstr_t domain = getenv(REQ_DOMAIN);
	_cstr_t port = getenv(REQ_PORT);
	_cstr_t uri = getenv(REQ_URI);
	_cstr_t proto = getenv(REQ_PROTOCOL);
	_char_t lb[4096] = "";
	SSL *ssl = NULL;
	int sz_lb = snprintf(lb, sizeof(lb), "%s %s %s\r\n", method, uri, proto);

	sz_lb += read_header(lb + sz_lb, sizeof(lb) - sz_lb);
	TRACE("http[%d] Request: %s\n", getpid(), lb);

	if ((r = io_create_ssl_client_connection(domain, (port) ? atoi(port) : 443, &ssl)) == E_OK) {
		SSL_write(ssl, (void *)lb, sz_lb);

		// read ...
		if (wait_input(SSL_get_fd(ssl), 30)) {
			int sz_respb = 256 * 1024;
			_str_t respb = (_str_t)malloc(sz_respb);
			int t = 100;
			size_t in = 0;

			while (t--) {
				if (verify_input(SSL_get_fd(ssl))) {
					if ((in = SSL_read(ssl, respb, sz_respb)) > 0) {
						if (io_write(respb, in) <= 0)
							break;
					} else
						break;

					t = 100;
				} else
					usleep(10000);
			}

			free(respb);
		}

		io_close_ssl_client_connection(ssl);
	} else {
		switch (r) {
			case E_FAIL:
				send_error_response(NULL, HTTPRC_INTERNAL_SERVER_ERROR);
				break;
			case E_RESOLVE:
				send_error_response(NULL, HTTPRC_NOT_FOUND);
				break;
		}
	}

	return r;
}

_err_t proxy_raw_connect(_cstr_t domain, int port) {
	_err_t r = E_FAIL;
	int sfd  = -1;
	_char_t lb[128] = "";

	if ((r = io_create_raw_client_connection(domain, port, &sfd)) == E_OK) {
		pthread_t pt;
		size_t nb_in = 0;
		int sz_reqb = 256 * 1024;
		_str_t reqb = (_str_t)malloc(sz_reqb);
		_cstr_t proto = getenv(REQ_PROTOCOL);
		int sz = snprintf(lb, sizeof(lb), "%s %d Connection Established\r\n\r\n", (proto) ? proto : "HTTP/1.1", HTTPRC_OK);

		io_write(lb, sz);

		pthread_create(&pt, NULL, [] (void *udata) -> void * {
			int nb_out = 0;
			int *p = (int *)udata;
			int sz_respb = 256 * 1024;
			_str_t respb = (_str_t)malloc(sz_respb);

			while ((nb_out = read(*p, respb, sz_respb)) > 0) {
				if (io_write(respb, nb_out) <= 0)
					break;
			}

			free(respb);

			return NULL;
		}, &sfd);

		pthread_setname_np(pt, "RAW connection");
		pthread_detach(pt);

		while ((nb_in = io_read(reqb, sz_reqb)) > 0) {
			if (write(sfd, reqb, nb_in) <= 0)
				break;
		}

		pthread_cancel(pt);
		free(reqb);
		close(sfd);
	} else {
		switch (r) {
			case E_FAIL:
				send_error_response(NULL, HTTPRC_INTERNAL_SERVER_ERROR);
				break;
			case E_RESOLVE:
				send_error_response(NULL, HTTPRC_NOT_FOUND);
				break;
		}
	}

	return r;
}

_err_t proxy_ssl_connect(_cstr_t domain, int port) {
	_err_t r = E_FAIL;
	SSL *ssl = NULL;
	_char_t lb[128] = "";

	if ((r = io_create_ssl_client_connection(domain, port, &ssl)) == E_OK) {
		pthread_t pt;
		size_t nb_in = 0;
		int sz_reqb = 256 * 1024;
		_str_t reqb = (_str_t)malloc(sz_reqb);
		_cstr_t proto = getenv(REQ_PROTOCOL);
		int sz = snprintf(lb, sizeof(lb), "%s %d Connection Established\r\n\r\n", (proto) ? proto : "HTTP/1.1", HTTPRC_OK);

		io_write(lb, sz);

		pthread_create(&pt, NULL, [] (void *udata) -> void * {
			int nb_out = 0;
			SSL **p = (SSL **)udata;
			int sz_respb = 256 * 1024;
			_str_t respb = (_str_t)malloc(sz_respb);

			while ((nb_out = SSL_read(*p, respb, sz_respb)) > 0) {
				if (io_write(respb, nb_out) <= 0)
					break;
			}

			free(respb);

			return NULL;
		}, &ssl);

		pthread_setname_np(pt, "SSL connection");
		pthread_detach(pt);

		while ((nb_in = io_read(reqb, sz_reqb)) > 0) {
			if (SSL_write(ssl, reqb, nb_in) <= 0)
				break;
		}

		pthread_cancel(pt);
		free(reqb);
		io_close_ssl_client_connection(ssl);
	} else {
		switch (r) {
			case E_FAIL:
				send_error_response(NULL, HTTPRC_INTERNAL_SERVER_ERROR);
				break;
			case E_RESOLVE:
				send_error_response(NULL, HTTPRC_NOT_FOUND);
				break;
		}
	}

	return r;
}

