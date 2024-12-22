#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "str.h"
#include "argv.h"
#include "http.h"
#include "trace.h"

static void send_header(int rc, _cstr_t header_append = NULL) {
	_char_t hdr[4096];
	_cstr_t proto = getenv(RES_PROTOCOL);
	_cstr_t hdr_fname = getenv(RES_HEADER_FILE);
	_cstr_t rc_text = rt_resp_text(rc);
	int sz_hdr = snprintf(hdr, sizeof(hdr), "%s %d %s\r\n", proto, rc, rc_text);

	// export header
	sz_hdr += hdr_export(hdr + sz_hdr, sizeof(hdr) - sz_hdr);

	// add header_append
	if (header_append && strlen(header_append))
		sz_hdr += str_resolve(header_append, hdr + sz_hdr, sizeof(hdr) - sz_hdr);

	// add header file
	int hdr_fd = (hdr_fname) ? open(hdr_fname, O_RDONLY) : -1;

	if (hdr_fd > 0) {
		int n = read(hdr_fd, hdr + sz_hdr, sizeof(hdr) - sz_hdr - 3);

		if (n > 0)
			// have header content
			sz_hdr += n;

		close(hdr_fd);
	}

	// end of header
	sz_hdr += snprintf(hdr + sz_hdr, sizeof(hdr) - sz_hdr, "\r\n");

	// send header
	io_write(hdr, sz_hdr);
}

static _err_t send_response_buffer(int rc, _cstr_t content, unsigned int sz,  _cstr_t header_append = NULL, _cstr_t ext = NULL) {
	_err_t r = E_FAIL;

	if (sz <= COMPRESSION_TRESHOLD) {
_no_compression_:
		hdr_set(RES_CONTENT_LENGTH, sz);
		// set content-type
		if (ext) {
			mime_open();
			hdr_set(RES_CONTENT_TYPE, mime_resolve(ext));
		}
		send_header(rc, header_append);
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
				// set content-type
				if (ext) {
					mime_open();
					hdr_set(RES_CONTENT_TYPE, mime_resolve(ext));
				}

				send_header(rc, header_append);
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
			_cstr_t header_append = NULL, _cstr_t ext = NULL) {
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
		_char_t buffer[MAX_COMPRESSION_CHUNK];
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
		if (ext) {
			mime_open();
			hdr_set(RES_CONTENT_TYPE, mime_resolve(ext));
		}

		// ...

		send_header(rc, header_append);

		if (rt_resolve_method(getenv(REQ_METHOD)) != METHOD_HEAD) {
			// send file content
			while (file_offset < st.st_size) {
				unsigned int n = read(fd, buffer, sizeof(buffer));
				io_write(buffer, n);
				file_offset += n;
			}
		}

		close(fd);
		r = E_OK;
	}

	return r;
}

static _err_t send_exec(_cstr_t cmd, int rc, bool input = false,
			bool header = true, _cstr_t header_append = NULL, _cstr_t ext = NULL) {
	_char_t tmp_fname[64];
	_char_t hdr_fname[64];
	_err_t r = E_FAIL;
	_proc_t proc;

	if (header) {
		int tmp_fd = -1, hdr_fd = -1;
		unsigned int encoding = rt_select_encoding(ext);

		snprintf(tmp_fname, sizeof(tmp_fname), "/tmp/http-%d.out", getpid());
		snprintf(hdr_fname, sizeof(hdr_fname), "/tmp/http-%d.hdr", getpid());

		if ((hdr_fd = open(hdr_fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > 0) {
			setenv(RES_HEADER_FILE, hdr_fname, 1);
			close(hdr_fd);
		}

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

				proc_wait(&proc);
			}

			close(tmp_fd);
			r = send_file_response(tmp_fname, rc, NULL,
						false, // do not use cache
						str_enc, // encoding name
						header_append,
						ext
						);
			unlink(tmp_fname);
		}

		unlink(hdr_fname);
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

static _err_t send_partial_content(_cstr_t path, struct stat *p_stat, _v_range_t *pv_ranges, _cstr_t boundary) {
	_err_t r = E_FAIL;
	int fd = open(path, O_RDONLY);
	int nb = 0;

	if (fd > 0) {
		_v_range_t::iterator i = pv_ranges->begin();
		unsigned long l = 0, s = 0;
		_char_t len[32];
		_char_t resp_buffer[MAX_COMPRESSION_CHUNK];
		_char_t date[128];
		tm *_tm = gmtime(&(p_stat->st_mtime));

		// set last-modified
		strftime(date, sizeof(date),
			 "%a, %d %b %Y %H:%M:%S GMT", _tm);
		hdr_set(RES_LAST_MODIFIED, date);

		// set connection
		hdr_set(RES_CONNECTION, getenv(REQ_CONNECTION));

		// ...

		if (pv_ranges->size() > 1) { // multipart
			// calculate content-length
			while (i != pv_ranges->end()) {
				l += strlen((*i).header) + ((*i).end - (*i).begin + 1);
				i++;
			}

			l += strlen(boundary) + 6; // \r\n--<boundary>--
			snprintf(len, sizeof(len), "%lu", l);
			hdr_set(RES_CONTENT_LENGTH, len);

			snprintf(resp_buffer, sizeof(resp_buffer), "multipart/byterange; boundary=%s", boundary);
			hdr_set(RES_CONTENT_TYPE, resp_buffer);

			send_header(HTTPRC_PART_CONTENT);

			// send content
			i = pv_ranges->begin();
			while (i != pv_ranges->end()) {
				_cstr_t hdr = (*i).header;

				l = 0;
				s = (*i).end - (*i).begin + 1;

				if (lseek(fd, (*i).begin, SEEK_SET) == (off_t)-1)
					goto _close_file_;

				TRACE("http[%d] Partial content (%lu / %lu)\n", getpid(), (*i).begin, s);
				// send range header
				io_write(hdr, strlen(hdr));

				// send range content
				while ((nb = read(fd, resp_buffer,
						((s - l) < sizeof(resp_buffer)) ?
						(s - l) : sizeof(resp_buffer))) > 0 && l < s) {
					io_write(resp_buffer, nb);
					l += nb;
				}

				i++;
			}

			// done
			io_fwrite("\r\n--%s--", boundary);
		} else { // single part
			// calculate content-length
			snprintf(len, sizeof(len), "%lu", (*i).end - (*i).begin + 1);
			hdr_set(RES_CONTENT_LENGTH, len);

			// set content-type
			mime_open();
			hdr_set(RES_CONTENT_TYPE, mime_resolve(path));

			// set content range
			snprintf(resp_buffer, sizeof(resp_buffer), "Bytes %lu-%lu/%lu", (*i).begin, (*i).end, p_stat->st_size);
			hdr_set(RES_CONTENT_RANGE, resp_buffer);

			send_header(HTTPRC_PART_CONTENT);

			// send content
			l = 0;
			s = (*i).end - (*i).begin + 1;

			if (lseek(fd, (*i).begin, SEEK_SET) == (off_t)-1)
				goto _close_file_;

			TRACE("http[%d] Partial content (%lu / %lu)\n", getpid(), (*i).begin, s);

			// send range content
			while ((nb = read(fd, resp_buffer,
					((s - l) < sizeof(resp_buffer)) ?
					(s - l) : sizeof(resp_buffer))) > 0 && l < s) {
				io_write(resp_buffer, nb);
				l += nb;
			}
		}

		r = E_OK;
_close_file_:
		close(fd);
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
		// not a directory
		if ((st.st_mode & S_IXUSR) && rt_allow_executables())
			// executable file request
			r = send_exec(path, rc, /* allow input */ true, /* header */ false);
		else {
			// regular file response
			_cstr_t range = getenv(REQ_RANGE);
			int imethod = rt_resolve_method(getenv(REQ_METHOD));

			if (range && imethod != METHOD_HEAD) {
				// range request
				_v_range_t	*pv_ranges;
				_char_t 	boundary[SHA1HashSize * 2 + 1];

				// generate boubdary
				range_generate_boundary(path, boundary, sizeof(boundary));

				if ((pv_ranges = range_parse(range, path, boundary)))
					r = send_partial_content(path, &st, pv_ranges, boundary);
				else
					r = send_error_response(NULL, HTTPRC_RANGE_NOT_SATISFIABLE);
			} else
				r = send_file_response(path, rc, &st, /* use cache */ true,
					/* encoding */ NULL, /* no header append */ NULL,
					/* ext */ rt_file_ext(path));
		}
	} else {
		// directory
		_char_t fname[MAX_PATH];

		TRACE("http[%d] Directory request '%s'\n", getpid(), path);

		// try index.html
		snprintf(fname, sizeof(fname), "%s/%s", path, "index.html");
		if (stat(fname, &st) == 0)
			r = send_file_response(fname, rc, &st, /* use cache */ true,
				/* encoding */ NULL, /* no header append */ NULL,
				/* ext */ ".html");
		else if (rt_allow_executables()) {
			// try index executable
			snprintf(fname, sizeof(fname), "%s/%s", path, "index");
			if (stat(fname, &st) == 0) {
				if ((st.st_mode & S_IXUSR))
					r = send_exec(fname, rc, /* allow input */ true, /* header */ false);
				else
					// found but not a executable
					r = send_error_response(NULL, HTTPRC_NOT_FOUND);
			} else
				r = send_error_response(NULL, HTTPRC_NOT_FOUND);
		} else
			r = send_error_response(NULL, HTTPRC_NOT_FOUND);
	}

	return r;
}

static _cstr_t resolve_path(_cstr_t path, char *resolved) {
	return realpath(path, resolved);
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
	_cstr_t proto = p_mapping->_protocol();

	str_resolve(proc, resolved_cmd, sizeof(resolved_cmd));

	if (proto && strlen(proto))
		// changes response protocol
		setenv(RES_PROTOCOL, proto, 1);

	if (p_mapping->_exec())
		r = send_exec(resolved_cmd, _rc, input, header, header_append, ext);
	else {
		struct stat st;

		if (stat(resolved_cmd, &st) == 0)
			r = send_file_response(resolved_cmd, _rc, &st, true, NULL, header_append, ext);
		else
			r = send_response_buffer(_rc, resolved_cmd, strlen(resolved_cmd), header_append, ext);
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

			if (content) {
				hdr_set(RES_CONTENT_TYPE, "text/html");
				r = send_response_buffer(rc, content, sz_content);
			} else {
				hdr_set(RES_CONTENT_LENGTH, "0");
				send_header(rc);
				r = E_OK;
			}
		}
	}

	return r;
}

static _char_t 	_g_proxy_dst_port_[8] = "443";
static _char_t 	_g_proxy_dst_host_[256] = "";

static _err_t do_connect(_cstr_t url) {
	_err_t r = E_DONE;
	_char_t lb[1024] = "";

	strncpy(lb, url, sizeof(lb) - 1);

	str_split(lb, ":", [] (int idx, char *str,
			void __attribute__((unused)) *udata) -> int {
		switch (idx) {
			case 0:
				strncpy(_g_proxy_dst_host_, str, sizeof(_g_proxy_dst_host_) - 1);
				break;
			case 1:
				strncpy(_g_proxy_dst_port_, str, sizeof(_g_proxy_dst_port_) - 1);
				break;
		}

		return 0;
	}, NULL);

	int port = atoi(_g_proxy_dst_port_);

	if (port) {
		if (port == 443 || port == 8443) {
			TRACE("http[%d] Try secure connection to '%s:%d'\n", getpid(), _g_proxy_dst_host_, port);
			r = proxy_ssl_connect(_g_proxy_dst_host_, port);
		} else {
			TRACE("http[%d] Try connection to '%s:%d'\n", getpid(), _g_proxy_dst_host_, port);
			r = proxy_raw_connect(_g_proxy_dst_host_, port);
		}
	}

	return r;
}

static _err_t process_file_request(int imethod, _vhost_t *p_vhost, _cstr_t path) {
	_err_t r = E_FAIL;
	char doc_path[MAX_PATH+1];
	char resolved_path[PATH_MAX];
	_cstr_t rpath = NULL;

	snprintf(doc_path, sizeof(doc_path), "%s%s", p_vhost->root, path);
	if ((rpath = resolve_path(doc_path, resolved_path))) {
		if (memcmp(rpath, p_vhost->root, strlen(p_vhost->root)) == 0) {
			switch (imethod) {
				case METHOD_GET:
				case METHOD_POST:
				case METHOD_HEAD:
					r = send_file(rpath, HTTPRC_OK);
					break;
			}
		} else {
			TRACE("http[%d] Forbidden path '%s'\n", getpid(), rpath);
			r = send_error_response(p_vhost, HTTPRC_FORBIDDEN);
		}
	} else {
		TRACE("http[%d] File not found '%s'\n", getpid(), doc_path);
		r = send_error_response(p_vhost, HTTPRC_NOT_FOUND);
	}

	return r;
}

_err_t res_processing(void) {
	_err_t r = E_FAIL;
	_cstr_t host = getenv(REQ_HOST);
	_vhost_t *p_vhost = cfg_get_vhost(host);
	_cstr_t method = getenv(REQ_METHOD);
	int imethod = (method) ? rt_resolve_method(method) : -1;
	_cstr_t proto = getenv(REQ_PROTOCOL);

	if (proto && strlen(proto))
		// response protocol as requested protocol
		setenv(RES_PROTOCOL, proto, 1);

	if (imethod != -1) {
		if (p_vhost) {
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
				switch (imethod) {
					case METHOD_GET:
					case METHOD_POST:
					case METHOD_HEAD:
						r = process_file_request(imethod, p_vhost, path);
						break;
					case METHOD_CONNECT:
						if (argv_check(OPT_PROXY))
							r = do_connect(url);
						else
							r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
						break;
					default:
						TRACE("http[%d] Unsupported  method '%s' #%d\n", getpid(), method, imethod);
						r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
						break;
				}
			}
		} else {
			TRACE("http[%d] Bad request method: '%s' [%d]; host: %s\n", getpid(), method, imethod, (p_vhost) ? p_vhost->host : "");
			r = send_error_response(p_vhost, HTTPRC_BAD_REQUEST);
		}
	} else {
		TRACE("http[%d] Unsupported  method '%s'\n", getpid(), method);
		r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
	}

	return r;
}
