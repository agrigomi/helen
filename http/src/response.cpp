#include "trace.h"
#include "str.h"
#include "argv.h"
#include "http.h"

static _err_t send_exec(_cstr_t cmd, bool input = false) {
	return resp_exec(cmd,
		/* out */
		[] (unsigned char *buf, unsigned int sz,
				void __attribute__((unused)) *udata) -> int {
			return io_write((_cstr_t)buf, sz);
		},
		/* in */
		[] (unsigned char *buf, unsigned int sz,
				void __attribute__((unused)) *udata) -> int {
			int r = 0;
			int nb = io_verify_input();

			if (nb > 0)
				r = io_read((_str_t)buf, sz);
			else if (nb < 0)
				r = nb;

			return r;
		},
		NULL) == E_OK ? E_DONE : -1;
}

static _cstr_t resolve_path(_cstr_t path, char *resolved) {
	_cstr_t r = NULL;

	if (strstr(path, ".."))
		r = realpath(path, resolved);
	else
		r = path;

	return r;
}

static _err_t send_response_buffer(int rc, _cstr_t content, unsigned int sz) {
	_err_t r = E_FAIL;

	//...

	return r;
}

_err_t send_error_response(_vhost_t *p_vhost, int rc) {
	_err_t r = E_FAIL;

	if (rc >= HTTPRC_BAD_REQUEST) {
		_mapping_t *mapping = (p_vhost) ? cfg_get_err_mapping(p_vhost->host, rc) : NULL;

		if (mapping) {
			//...
		} else {
			_cstr_t content = rt_static_content(rc);
			unsigned int sz_content = (content) ? strlen(content) : 0;

			if (content) {
				hdr_set(RES_CONTENT_TYPE, "text/html");
				hdr_set(RES_CONTENT_LENGTH, sz_content);
			}

			r = send_response_buffer(rc, content, sz_content);
		}
	}

	return r;
}

static _char_t 	_g_proxy_dst_port_[8] = "443";
static _char_t 	_g_proxy_dst_host_[256] = "";

static _err_t do_connect(_cstr_t url) {
	_err_t r = E_DONE;
	_char_t lb[1024] = "";

	strncpy(lb, url, sizeof(lb));

	str_split(lb, ":", [] (int idx, char *str,
			void __attribute__((unused)) *udata) -> int {
		switch (idx) {
			case 0:
				strncpy(_g_proxy_dst_host_, str, sizeof(_g_proxy_dst_host_));
				break;
			case 1:
				strncpy(_g_proxy_dst_port_, str, sizeof(_g_proxy_dst_port_));
				break;
		}

		return 0;
	}, NULL);

	int port = atoi(_g_proxy_dst_port_);

	if (port) {
		if (port == 443 || port == 8443)
			r = proxy_ssl_connect(_g_proxy_dst_host_, port);
		else
			r = proxy_raw_connect(_g_proxy_dst_host_, port);
	}

	return r;
}

_err_t res_processing(void) {
	_err_t r = E_FAIL;
	_cstr_t host = getenv(REQ_HOST);
	_vhost_t *p_vhost = cfg_get_vhost(host);
	_cstr_t method = getenv(REQ_METHOD);
	int imethod = (method) ? rt_resolve_method(method) : 0;

	if (p_vhost && imethod) {
		_cstr_t proto = getenv(REQ_PROTOCOL);
		_cstr_t url = getenv(REQ_URL);
		_cstr_t path = getenv(REQ_PATH);

		if(!path)
			path = url;

		cfg_load_mapping(p_vhost);
		_mapping_t *mapping = cfg_get_url_mapping(host, method, path, proto);

		if (mapping) {
			// process mapping
			setenv(DOC_ROOT, p_vhost->root, 1);
			//...
		} else {
			char doc_path[MAX_PATH+1];
			char resolved_path[PATH_MAX];
			_cstr_t rpath = NULL;

			snprintf(doc_path, sizeof(doc_path), "%s%s", p_vhost->root, path);

			if ((rpath = resolve_path(doc_path, resolved_path))) {
				if (memcmp(rpath, p_vhost->root, strlen(p_vhost->root)) == 0) {
					switch (imethod) {
						case METHOD_GET:
						case METHOD_POST:
							break;
						case METHOD_HEAD:
							break;
						case METHOD_CONNECT:
							if (argv_check(OPT_PROXY))
								r = do_connect(url);
							else
								r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
							break;
						default:
							r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
							break;
					}
				} else
					r = send_error_response(p_vhost, HTTPRC_FORBIDDEN);
			} else
				r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
		}
	} else
		r = send_error_response(p_vhost, HTTPRC_BAD_REQUEST);

	return r;
}
