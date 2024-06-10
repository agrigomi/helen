#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <map>
#include <vector>
#include "http.h"
#include "trace.h"
#include "fcfg.h"
#include "str.h"
#include "respawn.h"
#include "sha1.h"
#include "argv.h"

static char _g_resp_buffer_[256 * 1024];

#define RCT_NONE	0
#define RCT_STATIC	1
#define RCT_MAPPING	2

typedef struct {
	_cstr_t		s_method;
	int		i_method; // resolved method
	_cstr_t		url; // request url
	_cstr_t		uri; // request URI
	_cstr_t		protocol;
	_cstr_t		proto_upgrade;
	_cstr_t		path; // resolved path (real path)
	bool		b_st; // valid stat
	struct stat	st; // file status
	_vhost_t	*p_vhost;
	_v_range_t	*pv_ranges;
	char		boundary[MAX_BOUNDARY];
	int		rc; // response code
	int		rc_type; // response content type
	union {
		_cstr_t		static_text;
		_mapping_t	*p_mapping;
	};
	char		*header;
	int		sz_hbuffer;
} _resp_t;

typedef struct {
	_cstr_t		var;
	_cstr_t 	(*vcb)(_resp_t *);
} _hdr_t;

static char _g_vhdr_[4096];

static _hdr_t _g_hdef_[] = {
	{ RES_ACCEPT_RANGES,		[] (_resp_t *p) -> _cstr_t {
						_cstr_t r = NULL;

						if (p->rc >= 200 && p->rc < 400)
							r = "Bytes";

						return r;
					}},
	{ RES_ALLOW,			[] (_resp_t *p) -> _cstr_t {
						_cstr_t r = NULL;

						if (p->rc == HTTPRC_METHOD_NOT_ALLOWED)
							r = ALLOW_METHOD;

						return r;
					}},
	{ RES_CONTENT_RANGE,		[] (_resp_t *p) -> _cstr_t {
						_cstr_t r = NULL;

						if (p->rc == HTTPRC_PART_CONTENT && p->pv_ranges && p->b_st) {
							if (p->pv_ranges->size() == 1) {
								_v_range_t::iterator i = p->pv_ranges->begin();

								snprintf(_g_vhdr_, sizeof(_g_vhdr_),
										"Bytes %lu-%lu/%lu",
										(*i).begin, (*i).end,
										p->st.st_size);
								r = _g_vhdr_;
							}
						}

						return r;
					}},
	{ RES_CONTENT_LENGTH,		[] (_resp_t *p) -> _cstr_t {
						if (p->b_st) {
							if (p->rc == HTTPRC_PART_CONTENT && p->pv_ranges) {
								_v_range_t::iterator i = p->pv_ranges->begin();

								if (p->pv_ranges->size() > 1) {
									// multipart
									unsigned long l = 0;

									while (i != p->pv_ranges->end()) {
										l += strlen((*i).header) + ((*i).end - (*i).begin + 1);
										i++;
									}

									l += strlen(p->boundary) + 6; // \r\n--<boundary>--
									snprintf(_g_vhdr_, sizeof(_g_vhdr_), "%lu", l);
								} else
									// single part
									snprintf(_g_vhdr_, sizeof(_g_vhdr_), "%lu", (*i).end - (*i).begin + 1);
							} else
								snprintf(_g_vhdr_, sizeof(_g_vhdr_), "%lu", p->st.st_size);
						} else if (p->rc_type == RCT_STATIC && p->static_text)
								snprintf(_g_vhdr_, sizeof(_g_vhdr_), "%lu", strlen(p->static_text));
						else if (p->rc_type == RCT_MAPPING && p->p_mapping) {
							if (p->p_mapping->_exec())
								_g_vhdr_[0] = 0;
							else if (p->path)
								snprintf(_g_vhdr_, sizeof(_g_vhdr_), "%lu", strlen(p->path));
							else {
								_g_vhdr_[0] = '0';
								_g_vhdr_[1] = 0;
							}
						} else
							_g_vhdr_[0] = 0;

						return _g_vhdr_;
					}},
	{ RES_CONTENT_TYPE,		[] (_resp_t *p) -> _cstr_t {
						_cstr_t r = NULL;

						if (p->rc == HTTPRC_PART_CONTENT && p->pv_ranges && p->pv_ranges->size() > 1) {
							snprintf(_g_vhdr_, sizeof(_g_vhdr_),
								"multipart/byteranges; boundary=%s",
								p->boundary);
							r = _g_vhdr_;
						} else {
							if (p->rc_type == RCT_STATIC && p->static_text)
								r = "text/html";
							else {
								if (p->path && p->b_st) {
									mime_open();
									r = mime_resolve(p->path);
								}
							}
						}

						return r;
					}},
	{ RES_CONTENT_ENCODING,		[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						return "";
					}},
	{ RES_DATE,			[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						time_t _time = time(NULL);
						struct tm *_tm = gmtime(&_time);

						strftime(_g_vhdr_, sizeof(_g_vhdr_),
								 "%a, %d %b %Y %H:%M:%S GMT", _tm);
						return _g_vhdr_;
					}},
	{ RES_EXPIRES,			[] (_resp_t *p) -> _cstr_t {
						if (p->rc >= 200 && p->rc < 400) {
							time_t _time = time(NULL) + (72 * 60 * 60);
							struct tm *_tm = gmtime(&_time);

							strftime(_g_vhdr_, sizeof(_g_vhdr_),
								 "%a, %d %b %Y %H:%M:%S GMT", _tm);
						} else
							_g_vhdr_[0] = 0;

						return _g_vhdr_;
					}},
	{ RES_LAST_MODIFIED,		[] (_resp_t *p) -> _cstr_t {
						if (p->b_st) {
							tm *_tm = gmtime(&(p->st.st_mtime));

							strftime(_g_vhdr_, sizeof(_g_vhdr_),
								 "%a, %d %b %Y %H:%M:%S GMT", _tm);
						} else
							_g_vhdr_[0] = 0;

						return _g_vhdr_;
					}},
	{ RES_CONNECTION,		[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						_cstr_t r = getenv(REQ_CONNECTION);

						return (r) ? r : "Close";
					}},
	{ RES_UPGRADE,			[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						_cstr_t r = getenv(REQ_UPGRADE);

						return r;
					}},
	{ RES_SERVER,			[] (_resp_t __attribute__((unused)) *p) -> _cstr_t {
						return SERVER_NAME;
					}},
	{ NULL,				NULL }
};

static _err_t send_header(_resp_t *p) {
	_err_t r = E_OK;
	_cstr_t protocol = (p->protocol) ? p->protocol : "HTTP/1.1";
	_cstr_t text = rt_static_content(p->rc);
	char *header_append = NULL;
	int i = 0, n = 0;

	// First response line
	i += snprintf(p->header + i, p->sz_hbuffer - i, "%s %d %s\r\n",
			protocol, p->rc, (text) ? text : "Unknown");

	if (p->i_method == METHOD_HEAD)
		goto _eoh_;

	if (p->rc_type == RCT_MAPPING && p->p_mapping)
		header_append = p->p_mapping->_header_append();

	while (_g_hdef_[n].var) {
		bool set = true;

		if (header_append && header_append[0])
			set = strcasestr(header_append, _g_hdef_[n].var) == NULL;

		if (set) {
			_cstr_t val = _g_hdef_[n].vcb(p);

			if (val && val[0])
				i += snprintf(p->header + i, p->sz_hbuffer - i, "%s: %s\r\n", _g_hdef_[n].var, val);
		}

		n++;
	}

	// header append
	if (header_append)
		i += str_resolve(header_append, p->header + i, p->sz_hbuffer - i);

_eoh_:
	// EOH
	strncpy(p->header + i, "\r\n", p->sz_hbuffer - i);
	i += 2;

	io_write(p->header, i);

	return r;
}

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

			if (nb)
				r = io_read((_str_t)buf, sz);

			return r;
		},
		NULL);
}

static _err_t send_file_content(_resp_t *p) {
	_err_t r = E_FAIL;
	int fd = (p->path) ? open(p->path, O_RDONLY) : -1;
	int nb = 0;

	if (fd > 0) {
		if (p->rc == HTTPRC_PART_CONTENT && p->pv_ranges) {
			_v_range_t::iterator i = p->pv_ranges->begin();
			unsigned long l = 0, s = 0;

			if (p->pv_ranges->size() > 1) {
				// multipart
				while (i != p->pv_ranges->end()) {
					_cstr_t hdr = (*i).header;

					l = 0;
					s = (*i).end - (*i).begin + 1;

					if (lseek(fd, (*i).begin, SEEK_SET) == (off_t)-1)
						goto _close_file_;

					TRACE("http[%d] Partial content (%lu / %lu)\n", getpid(), (*i).begin, s);
					// send range header
					io_write(hdr, strlen(hdr));

					// send range content
					while ((nb = read(fd, _g_resp_buffer_,
							((s - l) < sizeof(_g_resp_buffer_)) ?
							(s - l) : sizeof(_g_resp_buffer_))) > 0 && l < s) {
						io_write(_g_resp_buffer_, nb);
						l += nb;
					}

					i++;
				}

				// done
				io_fwrite("\r\n--%s--", p->boundary);
			} else {
				// single part
				l = 0;
				s = (*i).end - (*i).begin + 1;

				if (lseek(fd, (*i).begin, SEEK_SET) == (off_t)-1)
					goto _close_file_;

				TRACE("http[%d] Partial content (%lu / %lu)\n", getpid(), (*i).begin, s);

				// send range content
				while ((nb = read(fd, _g_resp_buffer_,
						((s - l) < sizeof(_g_resp_buffer_)) ?
						(s - l) : sizeof(_g_resp_buffer_))) > 0 && l < s) {
					io_write(_g_resp_buffer_, nb);
					l += nb;
				}
			}

			r = E_OK;
		} else {
			while ((nb = read(fd, _g_resp_buffer_, sizeof(_g_resp_buffer_))) > 0)
				io_write(_g_resp_buffer_, nb);

			r = E_OK;
		}
_close_file_:
		close(fd);
	}

	return r;
}

static void switch_to_err(_resp_t *p, int rc) {
	if (rc >= HTTPRC_BAD_REQUEST) {
		p->rc = rc;

		TRACE("http[%d] #%d %s '%s'\n", getpid(), p->rc, rt_resp_text(rc), p->uri);
		if ((p->p_mapping = cfg_get_err_mapping(p->p_vhost, p->rc)))
			p->rc_type = RCT_MAPPING;
		else if ((p->static_text = rt_static_content(p->rc)))
			p->rc_type = RCT_STATIC;
	}
}

static void generate_boundary(_resp_t *p) {
	SHA1Context sha1_cxt;
	unsigned char sha1_result[SHA1HashSize];
	int i = 0, j = 0;
	static const char *hex = "0123456789abcdef";

	SHA1Reset(&sha1_cxt);
	memset(sha1_result, 0, sizeof(sha1_result));

	if (p->path)
		SHA1Input(&sha1_cxt, (unsigned char *)p->path, strlen(p->path));

	SHA1Input(&sha1_cxt, (unsigned char *)(&p->st), sizeof(struct stat));
	SHA1Result(&sha1_cxt, sha1_result);

	for (; i < SHA1HashSize && j < MAX_BOUNDARY; i++, j += 2) {
		p->boundary[j] = hex[(sha1_result[i] >> 4) & 0x0f];
		p->boundary[j + 1] = hex[sha1_result[i] & 0x0f];
	}
}

static _err_t send_response(_resp_t *p) {
	_err_t r = E_FAIL;
	bool header = true;
	_cstr_t proc = NULL;
	bool exec = false;
	char path[MAX_PATH];

	if (p->i_method == METHOD_HEAD)
		goto _send_header_;

_send_response_:
	// For mapping
	if (p->rc_type == RCT_MAPPING && p->p_mapping) {
		switch (p->p_mapping->type) {
			case MAPPING_TYPE_URL:
				proc = p->p_mapping->url._proc();
				header = p->p_mapping->url.header;
				exec = p->p_mapping->url.exec;
				break;
			case MAPPING_TYPE_ERR:
				proc = p->p_mapping->err._proc();
				header = p->p_mapping->err.header;
				exec = p->p_mapping->err.exec;
				break;
		}

		if (proc && proc[0]) {
			str_resolve(proc, path, sizeof(path));

			if (exec) {
				if (header)
					r = send_header(p);

				r = send_exec(path, p->p_mapping->_input());
			} else {
				p->path = path;

				if ((p->b_st = (stat(path, &(p->st)) == 0)))
					goto _send_file_;
				else {
					r = send_header(p);
					io_write(path, strlen(path));
				}
			}
		} else
			goto _send_header_;
	} else if (p->rc_type == RCT_STATIC && p->static_text) {
		int l = strlen(p->static_text);

		if (l) {
			if ((r = send_header(p)) >= E_OK)
				io_write(p->static_text, l);
		}
	} else if (p->b_st) {
_send_file_:
		if (!S_ISDIR(p->st.st_mode)) {
			// Not a directory
			if ((p->st.st_mode & S_IXUSR) && argv_check(OPT_EXEC)) {
				// executable
				if (p->path)
					r = send_exec(p->path);
			} else if (p->st.st_mode & S_IRUSR) {
				_cstr_t range = getenv(REQ_RANGE);

				if (range) {
					generate_boundary(p);

					if ((p->pv_ranges = range_parse(p->path, p->boundary)))
						p->rc = HTTPRC_PART_CONTENT;
					else {
						p->b_st = false;
						p->path = NULL;

						switch_to_err(p, HTTPRC_RANGE_NOT_SATISFIABLE);
						goto _send_response_;
					}
				}

				if (header)
					r = send_header(p);

				if (p->path)
					r = send_file_content(p);
			} else {
				TRACE("http[%d] No read permissions\n", getpid());
			}
		} else { // Directory request
			p->b_st = false; // temporary !!!
			switch_to_err(p, HTTPRC_NOT_IMPLEMENTED);
			TRACE("http[%d] Directory request\n", getpid());
			goto _send_response_;
		}
	} else {
_send_header_:
		if (header)
			r = send_header(p);
	}

	return r;
}

_err_t send_error_response(_vhost_t *p_vhost, int rc) {
	_err_t r = E_FAIL;

	if (rc >= HTTPRC_BAD_REQUEST) {
		_resp_t resp;
		_mapping_t *p_err_map = (p_vhost) ? cfg_get_err_mapping(p_vhost->host, rc) : NULL;
		_cstr_t content = NULL;

		memset(&resp, 0, sizeof(_resp_t));
		resp.rc = rc;
		resp.p_vhost = p_vhost;
		resp.s_method = getenv(REQ_METHOD);
		resp.i_method = (resp.s_method) ? rt_resolve_method(resp.s_method) : 0;
		resp.uri = getenv(REQ_PATH);
		resp.protocol = getenv(REQ_PROTOCOL);
		resp.proto_upgrade = getenv(REQ_UPGRADE);
		resp.header = _g_resp_buffer_;
		resp.sz_hbuffer = sizeof(_g_resp_buffer_);

		if (p_err_map) {
			resp.rc_type = RCT_MAPPING;
			resp.p_mapping = p_err_map;
		} else if ((content = rt_static_content(rc))) {
			resp.rc_type = RCT_STATIC;
			resp.static_text = content;
		}

		r = send_response(&resp);
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

static _char_t 	_g_proxy_dst_port_[8] = "443";
static _char_t 	_g_proxy_dst_host_[256] = "";

static _err_t do_connect(_resp_t *p) {
	_err_t r = E_DONE;
	_char_t lb[1024] = "";

	strncpy(lb, p->url, sizeof(lb));

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

	if (p_vhost) {
		_resp_t resp;
		char doc_path[MAX_PATH+1];
		char resolved_path[PATH_MAX];

		memset(&resp, 0, sizeof(_resp_t));
		_g_resp_buffer_[0] = 0;
		setenv(DOC_ROOT, p_vhost->root, 1);
		cfg_load_mapping(p_vhost);

		resp.p_vhost = p_vhost;
		resp.url = getenv(REQ_URL);
		resp.uri = getenv(REQ_PATH);
		resp.s_method = getenv(REQ_METHOD);
		resp.protocol = getenv(REQ_PROTOCOL);
		resp.proto_upgrade = getenv(REQ_UPGRADE);
		resp.header = _g_resp_buffer_;
		resp.sz_hbuffer = sizeof(_g_resp_buffer_);

		if (!resp.uri)
			resp.uri = resp.url;

		if (resp.s_method && resp.uri && resp.protocol) {
			resp.i_method = (resp.s_method) ? rt_resolve_method(resp.s_method) : 0;

			if (resp.i_method == METHOD_GET || resp.i_method == METHOD_POST || resp.i_method == METHOD_HEAD) {
				if ((resp.p_mapping = cfg_get_url_mapping(p_vhost->host, resp.s_method, resp.uri, resp.protocol))) {
					_cstr_t proto = resp.p_mapping->_protocol();

					resp.rc_type = RCT_MAPPING;
					if (proto[0]) {
						resp.protocol = (char *)proto;
						setenv(REQ_PROTOCOL, proto, 1);
					}

					resp.rc = (resp.p_mapping->url.resp_code) ? resp.p_mapping->url.resp_code : HTTPRC_OK;
				} else {
					snprintf(doc_path, sizeof(doc_path), "%s%s", p_vhost->root, resp.uri);

					if ((resp.path = resolve_path(doc_path, resolved_path))) {
						if (memcmp(resp.path, p_vhost->root, strlen(p_vhost->root)) == 0) {
							if ((resp.b_st = (stat(resp.path, &resp.st) == 0)))
								resp.rc = HTTPRC_OK;
							else
								switch_to_err(&resp, HTTPRC_NOT_FOUND);
						} else {
							resp.path = NULL;
							switch_to_err(&resp, HTTPRC_FORBIDDEN);
						}
					} else
						switch_to_err(&resp, HTTPRC_NOT_FOUND);
				}

				r = send_response(&resp);
			} else if (resp.i_method == METHOD_CONNECT) {
				if (argv_check(OPT_PROXY))
					r = do_connect(&resp);
				else
					r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
			} else
				r = send_error_response(p_vhost, HTTPRC_METHOD_NOT_ALLOWED);
		} else
			r = send_error_response(p_vhost, HTTPRC_BAD_REQUEST);
	}

	return r;
}
