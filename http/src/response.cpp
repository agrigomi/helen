#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "str.h"
#include "argv.h"
#include "http.h"
#include "trace.h"

static void send_header(int rc, _cstr_t header_append = NULL) {
	_char_t hdr[2048];
	_cstr_t proto = getenv(REQ_PROTOCOL);
	_cstr_t rc_text = rt_resp_text(rc);
	int sz_hdr = snprintf(hdr, sizeof(hdr), "%s %d %s\r\n", proto, rc, rc_text);

	// export header
	sz_hdr += hdr_export(hdr + sz_hdr, sizeof(hdr) - sz_hdr);

	// add header_append
	if (header_append && strlen(header_append))
		sz_hdr += str_resolve(header_append, hdr + sz_hdr, sizeof(hdr) - sz_hdr);

	// end of header
	sz_hdr += snprintf(hdr + sz_hdr, sizeof(hdr) - sz_hdr, "\r\n");

	// send header
	io_write(hdr, sz_hdr);
}

static _err_t send_response_buffer(int rc, _cstr_t content, unsigned int sz) {
	_err_t r = E_FAIL;

	if (sz <= COMPRESSION_TRESHOLD) {
_no_compression_:
		hdr_set(RES_CONTENT_LENGTH, sz);
		send_header(rc);
		// send content
		io_write(content, sz);
		r = E_OK;
	} else {
		unsigned char *p_gzip_buffer = (unsigned char *)malloc(sz);
		long unsigned int sz_dst = sz;
		char *p_type = NULL;

		if (p_gzip_buffer) {
			// do compression
			if ((r = rt_compress_buffer((const unsigned char *)content, sz, p_gzip_buffer, &sz_dst, &p_type)) == E_OK) {
				// set header options
				hdr_set(RES_CONTENT_LENGTH, sz_dst);
				hdr_set(RES_CONTENT_ENCODING, p_type);

				send_header(rc);
				// send content
				io_write((char *)p_gzip_buffer, sz_dst);
				// release gzip buffer
				free(p_gzip_buffer);
			} else {
				// release gzip buffer
				free(p_gzip_buffer);
				goto _no_compression_;
			}
		} else {
			LOG("http[%d] Failed to allocate buffer (%d bytes) for compression\n", getpid(), sz);
			goto _no_compression_;
		}
	}

	return r;
}

static _err_t send_file_response(_cstr_t path, int rc, struct stat *pstat = NULL,
			bool use_cache = true, _cstr_t encoding = NULL,
			_cstr_t header_append = NULL) {
	_err_t r = E_FAIL;
	_cstr_t _encoding = encoding;
	struct stat st;
	int fd = -1;

	if (use_cache)
		fd = cache_open(path, &st, &_encoding);
	else {
		if (pstat)
			memcpy(&st, pstat, sizeof(struct stat));
		else
			stat(path, &st);

		fd = open(path, O_RDONLY);
	}

	if (fd > 0) {
		_char_t buffer[8192];
		unsigned int file_offset = 0;
		_char_t date[128];
		tm *_tm = gmtime(&(st.st_mtime));

		hdr_set(RES_CONTENT_LENGTH, st.st_size);

		if (_encoding)
			hdr_set(RES_CONTENT_ENCODING, _encoding);

		// set last-modified
		strftime(date, sizeof(date),
			 "%a, %d %b %Y %H:%M:%S GMT", _tm);
		hdr_set(RES_LAST_MODIFIED, date);

		// set connection
		hdr_set(RES_CONNECTION, getenv(REQ_CONNECTION));

		// set content-type
		mime_open();
		hdr_set(RES_CONTENT_TYPE, mime_resolve(path));

		// ...

		send_header(rc, header_append);

		// send file content
		while (file_offset < st.st_size) {
			unsigned int n = read(fd, buffer, sizeof(buffer));
			io_write(buffer, n);
			file_offset += n;
		}

		close(fd);
		r = E_OK;
	}

	return r;
}

static _err_t send_exec(_cstr_t cmd, int rc, bool input = false,
			bool header = true, _cstr_t header_append = NULL, _cstr_t ext = NULL) {
	_char_t tmp_fname[256];
	_err_t r = E_FAIL;
	_proc_t proc;

	if (header) {
		int tmp_fd = -1;
		unsigned int encoding = rt_select_encoding(ext);

		snprintf(tmp_fname, sizeof(tmp_fname), "/tmp/http-%d.out", getpid());

		if ((tmp_fd = open(tmp_fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > 0) {
			_cstr_t str_enc = NULL; // encoding name

			if (resp_exec(cmd, &proc) == E_OK) {
				_uchar_t buffer[MAX_COMPRESSION_CHUNK];
				int nin = 0;

				if (input) {
					while ((nin = io_verify_input()) > 0) {
						nin = io_read((_str_t)buffer, sizeof(buffer));

						proc_write(&proc, buffer, nin);
					}
				}

				if (encoding)
					// use compression
					rt_compress_stream(encoding, proc.PREAD_FD, tmp_fd, &str_enc);
				else {
					// no compression
					while ((nin = proc_read(&proc, buffer, sizeof(buffer))) > 0)
						write (tmp_fd, buffer, nin);
				}
			}

			close(tmp_fd);
			r = send_file_response(tmp_fname, rc, NULL,
						false, // do not use cache
						str_enc, // encoding name
						header_append
						);
			unlink(tmp_fname);
		}
	} else {
		r = resp_exec(cmd,
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

	return r;
}

static _err_t send_file(_cstr_t path, int rc, struct stat *pstat = NULL) {
	_err_t r = E_FAIL;
	struct stat st;

	if (pstat)
		memcpy(&st, pstat, sizeof(struct stat));
	else
		stat(path, &st);

	if (!S_ISDIR(st.st_mode)) {
		if ((st.st_mode & S_IXUSR) && argv_check(OPT_EXEC))
			// executable file request
			r = send_exec(path, rc, /* allow input */ true, /* no header */ false);
		else
			// regular file response
			r = send_file_response(path, rc, &st, /* use cache */ true,
						/* encoding */ NULL, /* no header append */ NULL);
	} else {
		TRACE("http[%d] Directory request '%s'\n", getpid(), path);
		//...
	}

	return r;
}

static _cstr_t resolve_path(_cstr_t path, char *resolved) {
	_cstr_t r = NULL;

	if (strstr(path, ".."))
		r = realpath(path, resolved);
	else
		r = path;

	return r;
}

static _err_t send_mapping_response(_mapping_t *p_mapping, int rc) {
	_err_t r = E_FAIL;
	int _rc = rc;

	if (p_mapping->type == MAPPING_TYPE_URL) {
		if (p_mapping->url.resp_code)
			// Use response code from mapping
			_rc = p_mapping->url.resp_code;
	}

	_cstr_t proc = p_mapping->_proc();
	bool input = p_mapping->_input();
	_cstr_t header_append = p_mapping->_header_append();
	_char_t resolved_cmd[MAX_PATH] = "";
	bool header = p_mapping->_header();
	_cstr_t ext = p_mapping->_ext();

	str_resolve(proc, resolved_cmd, sizeof(resolved_cmd));

	if (p_mapping->_exec())
		r = send_exec(resolved_cmd, _rc, input, header, header_append, ext);
	else {
		struct stat st;

		if (stat(resolved_cmd, &st) == 0)
			r = send_file_response(resolved_cmd, _rc, &st, true, NULL, header_append);
		else
			r = send_response_buffer(_rc, resolved_cmd, strlen(resolved_cmd));
	}

	return r;
}

_err_t send_error_response(_vhost_t *p_vhost, int rc) {
	_err_t r = E_FAIL;

	if (rc >= HTTPRC_BAD_REQUEST) {
		_mapping_t *mapping = (p_vhost) ? cfg_get_err_mapping(p_vhost->host, rc) : NULL;

		if (mapping)
			r = send_mapping_response(mapping, rc);
		else {
			_cstr_t content = rt_static_content(rc);
			unsigned int sz_content = (content) ? strlen(content) : 0;

			if (content)
				hdr_set(RES_CONTENT_TYPE, "text/html");

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

	if (p_vhost) {
		_cstr_t proto = getenv(REQ_PROTOCOL);
		_cstr_t url = getenv(REQ_URL);
		_cstr_t path = getenv(REQ_PATH);

		if (!path)
			path = url;

		hdr_init();
		cfg_load_mapping(p_vhost);
		_mapping_t *mapping = cfg_get_url_mapping(p_vhost->host, method, path, proto);

		setenv(DOC_ROOT, p_vhost->root, 1);

		if (mapping)
			// process mapping
			r = send_mapping_response(mapping, HTTPRC_OK);
		else {
			char doc_path[MAX_PATH+1];
			char resolved_path[PATH_MAX];
			_cstr_t rpath = NULL;

			snprintf(doc_path, sizeof(doc_path), "%s%s", p_vhost->root, path);

			if ((rpath = resolve_path(doc_path, resolved_path))) {
				if (memcmp(rpath, p_vhost->root, strlen(p_vhost->root)) == 0) {
					switch (imethod) {
						case METHOD_GET:
						case METHOD_POST:
							if ((r = send_file(rpath, HTTPRC_OK)) != E_OK)
								r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
							break;
						case METHOD_HEAD:
							//...
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
	} else {
		TRACE("http[%d] Bad request method: '%s' [%d]; host: %s\n", getpid(), method, imethod, (p_vhost) ? p_vhost->host : NULL);
		r = send_error_response(p_vhost, HTTPRC_BAD_REQUEST);
	}

	return r;
}
