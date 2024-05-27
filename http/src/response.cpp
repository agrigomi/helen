#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
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

static std::map<int, _cstr_t> _g_resp_text_ = {
	{ HTTPRC_CONTINUE,		"Continue" },
	{ HTTPRC_SWITCHING_PROTOCOL,	"Switching Protocols" },
	{ HTTPRC_OK,			"OK" },
	{ HTTPRC_CREATED,		"Created" },
	{ HTTPRC_ACCEPTED,		"Accepted" },
	{ HTTPRC_NON_AUTH,		"Non-Authoritative Information" },
	{ HTTPRC_NO_CONTENT,		"No Content" },
	{ HTTPRC_RESET_CONTENT,		"Reset Content" },
	{ HTTPRC_PART_CONTENT,		"Partial Content" },
	{ HTTPRC_MULTICHOICES,		"Multiple Choices" },
	{ HTTPRC_MOVED_PERMANENTLY,	"Moved Permanently" },
	{ HTTPRC_FOUND,			"Found" },
	{ HTTPRC_SEE_OTHER,		"See Other" },
	{ HTTPRC_NOT_MODIFIED,		"Not Modified" },
	{ HTTPRC_USE_PROXY,		"Use proxy" },
	{ HTTPRC_TEMP_REDIRECT,		"Temporary redirect" },
	{ HTTPRC_BAD_REQUEST,		"Bad Request" },
	{ HTTPRC_UNAUTHORIZED,		"Unauthorized" },
	{ HTTPRC_PAYMENT_REQUIRED,	"Payment Required" },
	{ HTTPRC_FORBIDDEN,		"Forbidden" },
	{ HTTPRC_NOT_FOUND,		"Not Found" },
	{ HTTPRC_METHOD_NOT_ALLOWED,	"Method Not Allowed" },
	{ HTTPRC_NOT_ACCEPTABLE,	"Not Acceptable" },
	{ HTTPRC_PROXY_AUTH_REQUIRED,	"Proxy Authentication Required" },
	{ HTTPRC_REQUEST_TIMEOUT,	"Request Time-out" },
	{ HTTPRC_CONFLICT,		"Conflict" },
	{ HTTPRC_GONE,			"Gone" },
	{ HTTPRC_LENGTH_REQUIRED,	"Length Required" },
	{ HTTPRC_PRECONDITION_FAILED,	"Precondition Failed" },
	{ HTTPRC_REQ_ENTITY_TOO_LARGE,	"Request Entity Too Large" },
	{ HTTPRC_REQ_URI_TOO_LARGE,	"Request-URI Too Large" },
	{ HTTPRC_UNSUPPORTED_MEDIA_TYPE,"Unsupported Media Type" },
	{ HTTPRC_RANGE_NOT_SATISFIABLE,	"Range Not Satisfiable" },
	{ HTTPRC_EXPECTATION_FAILED,	"Expectation Failed" },
	{ HTTPRC_UPGRADE_REQUIRED,	"Upgrade required" },
	{ HTTPRC_INTERNAL_SERVER_ERROR,	"Internal Server Error" },
	{ HTTPRC_NOT_IMPLEMENTED,	"Not Implemented" },
	{ HTTPRC_BAD_GATEWAY,		"Bad Gateway" },
	{ HTTPRC_SERVICE_UNAVAILABLE,	"Service Unavailable" },
	{ HTTPRC_GATEWAY_TIMEOUT,	"Gateway Time-out" },
	{ HTTPRC_VERSION_NOT_SUPPORTED,	"HTTP Version not supported" },
};

static std::map<int, _cstr_t> _g_resp_content_ = {
	{ HTTPRC_NOT_FOUND,		"<!DOCTYPE html><html><head></head><body><h1>Not found #404</h1></body></html>" },
	{ HTTPRC_INTERNAL_SERVER_ERROR,	"<!DOCTYPE html><html><head></head><body><h1>Internal server error #500</h1></body></html>" },
	{ HTTPRC_FORBIDDEN,		"<!DOCTYPE html><html><head></head><body><h1>Forbidden path #403</h1></body></html>" },
	{ HTTPRC_NOT_IMPLEMENTED,	"<!DOCTYPE html><html><head></head><body><h1>Not implemented #501</h1></body></html>" },
	{ HTTPRC_RANGE_NOT_SATISFIABLE,	"<!DOCTYPE html><html><head></head><body><h1>Range Not Satisfiable #416</h1></body></html>" }
};

static const char *methods[] = { "GET", "HEAD", "POST",
				"PUT", "DELETE", "CONNECT",
				"OPTIONS", "TRACE", "PATCH",
				NULL };
#define METHOD_GET	0
#define METHOD_HEAD	1
#define METHOD_POST	2
#define METHOD_PUT	3
#define METHOD_DELETE	4
#define METHOD_CONNECT	5
#define METHOD_OPTIONS	6
#define METHOD_TRACE	7
#define METHOD_PATCH	8

static char _g_resp_buffer_[256 * 1024];
static char _g_req_buffer_[256 * 1024];

typedef struct {
	unsigned long begin; // start offset in content
	unsigned long end; // size in bytes
	char	header[512]; // range header
} _range_t;

typedef std::vector<_range_t> _v_range_t;

#define RCT_NONE	0
#define RCT_STATIC	1
#define RCT_MAPPING	2

#define MAX_BOUNDARY	(SHA1HashSize * 2) + 1

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

static int resolve_method(_cstr_t method) {
	int n = 0;

	for (; methods[n]; n++) {
		if (strcasecmp(method, methods[n]) == 0)
			break;
	}

	return n;
}

static void split_by_space(_cstr_t str, _u32 str_size, _str_t dst_arr[], _u32 arr_size) {
	_str_t p_str = (_str_t)malloc(str_size + 1);

	if (p_str) {
		_str_t rest = NULL;
		_str_t token;
		_u32 i = 0, l = 0;

		memset(p_str, 0, str_size + 1);
		strncpy(p_str, str, str_size);

		for (token = strtok_r(p_str, " ", &rest); token != NULL; token = strtok_r(NULL, " ", &rest)) {
			l = strlen(token) + 1;
			if ((dst_arr[i] = (_str_t)malloc(l))) {
				strcpy(dst_arr[i], token);
				i++;
				if (i >= arr_size)
					break;
			} else {
				TRACEfl("Unable to allocate memory !\n");
			}
		}

		free(p_str);
	}
}

static _cstr_t file_extension(_cstr_t path) {
	_cstr_t r = NULL;
	size_t l = strlen(path);

	while (l && path[l] != '.')
		l--;

	if (l)
		r = &path[l];

	return r;
}

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
	_cstr_t text = _g_resp_text_[p->rc];
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

static _err_t exec(_cstr_t argv[], int __attribute__((unused)) tmout, bool input = false, _cstr_t _write = NULL) {
	_err_t r = E_FAIL;
	_proc_t proc;
	pthread_t pt;
	int nb_out = 0;
	unsigned int sum_out = 0;

	signal(SIGCHLD, [](__attribute__((unused)) int sig) {});

	if (proc_exec_v(&proc, argv[0], argv) == 0) {
		if (_write)
			proc_write(&proc, (void *)_write, strlen(_write));

		if (input) {
			pthread_create(&pt, NULL, [] (void *udata) -> void * {
				int nb_in = 0;
				_proc_t *p = (_proc_t *)udata;
				unsigned int sum_in = 0;

				while ((nb_in = io_read(_g_req_buffer_, sizeof(_g_req_buffer_))) > 0) {
					TRACE("http[%d] >> %s", getpid(), _g_req_buffer_);
					proc_write(p, _g_req_buffer_, nb_in);
					sum_in += nb_in;
				}

				proc_break(p);
				TRACE("http[%d] in: %u\n", getpid(), sum_in);
				return NULL;
			}, &proc);
		}

		while ((nb_out = proc_read(&proc, _g_resp_buffer_, sizeof(_g_resp_buffer_))) > 0) {
			TRACE("http[%d] << %s", getpid(), _g_resp_buffer_);
			io_write(_g_resp_buffer_, nb_out);
			sum_out += nb_out;
		}

		proc_break(&proc);
		TRACE("http[%d] out: %u\n", getpid(), sum_out);

		r = E_DONE;
	}

	return r;
}

static _err_t send_exec(_cstr_t cmd, bool input = false) {
	_err_t r = E_FAIL;
	_str_t argv[256];
	int i = 0;
	int timeout = atoi(argv_value(OPT_TIMEOUT));

	memset(argv, 0, sizeof(argv));
	split_by_space(cmd, strlen(cmd), argv, 256);

	TRACE("http[%d] Execute '%s'\n", getpid(), cmd);
	r = exec((_cstr_t *)argv, timeout, input);

	while (argv[i]) {
		free(argv[i]);
		i++;
	}

	return r;
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

					TRACE("http[%d]: Partial content (%lu / %lu)\n", getpid(), (*i).begin, s);
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

				TRACE("http[%d]: Partial content (%lu / %lu)\n", getpid(), (*i).begin, s);

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

		TRACE("http[%d]: #%d %s '%s'\n", getpid(), p->rc, _g_resp_text_[p->rc], p->uri);
		if ((p->p_mapping = cfg_get_err_mapping(p->p_vhost, p->rc)))
			p->rc_type = RCT_MAPPING;
		else if ((p->static_text = _g_resp_content_[p->rc]))
			p->rc_type = RCT_STATIC;
	}
}

static _v_range_t _gv_ranges_; // ranges vector
static _range_t _g_range_; // range element

static _err_t parse_range(_resp_t *p, _cstr_t range) {
	_err_t r = E_FAIL;
	_cstr_t if_range = getenv(REQ_IF_RANGE);

	_gv_ranges_.clear();
	strncpy(_g_vhdr_, range, sizeof(_g_vhdr_));

	str_split(_g_vhdr_, "=", [] (int idx, char *str, void *udata) -> int {
		int r = -1;
		_resp_t *p = (_resp_t *)udata;

		if (idx == 0) {
			// Range units
			if (strncasecmp(str, "bytes", 5) == 0)
				r = 0;
		} else if (idx == 1) {
			// ranges
			str_split(str, ",", [] (int __attribute__((unused)) idx, char *str,
					void *udata) -> int {
				int r = 0;
				_resp_t *p = (_resp_t *)udata;

				memset(&_g_range_, 0, sizeof(_range_t));

				str_split(str, "-", [] (int idx, char *str,
						void __attribute__((unused)) *udata) -> int {
					if (idx == 0)
						// range start
						_g_range_.begin = atol(str);
					else if (idx == 1)
						// range end
						_g_range_.end = atol(str);

					return 0;
				}, p);

				if (!_g_range_.end)
					// to the end of file
					_g_range_.end = p->st.st_size - 1;

				// _g_range_ shoult contains a range metrics (offset ans size)
				if ((_g_range_.end >= _g_range_.begin) &&
						_g_range_.begin + (_g_range_.end - _g_range_.begin) <= (unsigned long)p->st.st_size) {
					int i = 0;

					/////// range header //////////

					// boundary
					i += snprintf(_g_range_.header + i, sizeof(_g_range_.header) - i,
							"\r\n--%s\r\n", p->boundary);
					// content type
					if (p->path) {
						mime_open();

						_cstr_t mt = mime_resolve(p->path);

						if (mt)
							i += snprintf(_g_range_.header + i, sizeof(_g_range_.header) - i,
									RES_CONTENT_TYPE ": %s\r\n", mt);
					}

					// content range + EOH
					i += snprintf(_g_range_.header + i, sizeof(_g_range_.header) - i,
							RES_CONTENT_RANGE ": Bytes %lu-%lu/%lu\r\n\r\n",
							_g_range_.begin, _g_range_.end, p->st.st_size);

					//////////////////////////////

					_gv_ranges_.push_back(_g_range_);
				} else
					// invalid range
					r = -1;

				return r;
			}, p);

			r = 0;
		}

		return r;
	}, p);

	if (_gv_ranges_.size()) {
		p->pv_ranges = &_gv_ranges_;
		r = E_OK;
	}

	return r;
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

					if (parse_range(p, range) == E_OK)
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
				TRACE("http[%d]: No read permissions\n", getpid());
			}
		} else { // Directory request
			p->b_st = false; // temporary !!!
			switch_to_err(p, HTTPRC_NOT_IMPLEMENTED);
			TRACE("http[%d]: Directory request\n", getpid());
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
		resp.i_method = (resp.s_method) ? resolve_method(resp.s_method) : 0;
		resp.uri = getenv(REQ_PATH);
		resp.protocol = getenv(REQ_PROTOCOL);
		resp.proto_upgrade = getenv(REQ_UPGRADE);
		resp.header = _g_resp_buffer_;
		resp.sz_hbuffer = sizeof(_g_resp_buffer_);

		if (p_err_map) {
			resp.rc_type = RCT_MAPPING;
			resp.p_mapping = p_err_map;
		} else if ((content = _g_resp_content_[rc])) {
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

static _err_t proxy_raw_client_connection(_cstr_t domain, int port, _cstr_t _write = NULL, bool response = false) {
	_err_t r = E_FAIL;
	int sock = -1;
	struct sockaddr_in server;
	struct hostent *hp;
	_cstr_t proto = getenv(REQ_PROTOCOL);
	_char_t lb[128] = "";

	if (!proto)
		proto = "HTTP/1.1";

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
		server.sin_family = AF_INET;

		if ((hp = gethostbyname(domain))) {
			memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
			server.sin_port = htons(port);
			int set_opt = 1;

			setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &set_opt, sizeof(set_opt));

			if (connect(sock, (struct sockaddr *) &server, sizeof(server)) == 0) {
				pthread_t pt;
				int nb_in = 0;
				unsigned int sum_in = 0;

				if (_write)
					write(sock, (void *)_write, strlen(_write));

				if (response) {
					// send positive response
					int sz = snprintf(lb, sizeof(lb), "%s %d Connection Established\r\n\r\n", proto, HTTPRC_OK);
					io_write(lb, sz);
				}

				pthread_create(&pt, NULL, [] (void *udata) -> void * {
					int nb_out = 0;
					int *p = (int *)udata;
					unsigned int sum_out = 0;

					while ((nb_out = read(*p, _g_resp_buffer_, sizeof(_g_resp_buffer_))) > 0) {
						TRACE("http[%d] << [%d] %s", getpid(), nb_out, _g_resp_buffer_);
						if (io_write(_g_resp_buffer_, nb_out) <= 0)
							break;
						sum_out += nb_out;
						memset(_g_resp_buffer_, 0, nb_out);
					}

					TRACE("http[%d] out: %u\n", getpid(), sum_out);
					return NULL;
				}, &sock);

				pthread_setname_np(pt, "RAW connection");
				pthread_detach(pt);

				while ((nb_in = io_read(_g_req_buffer_, sizeof(_g_req_buffer_))) > 0) {
					TRACE("http[%d] >> [%d] %s", getpid(), nb_in, _g_req_buffer_);
					if (write(sock, _g_req_buffer_, nb_in) <= 0)
						break;
					sum_in += nb_in;
					memset(_g_req_buffer_, 0, nb_in);
				}

				TRACE("http[%d] in: %u\n", getpid(), sum_in);

				pthread_cancel(pt);
				r = E_OK;
			} else {
				if (response) {
					int sz = snprintf(lb, sizeof(lb), "%s %d Internal Server Error\r\n\r\n", proto, HTTPRC_INTERNAL_SERVER_ERROR);
					io_write(lb, sz);
				}
			}
		} else {
			TRACE("http:[%d] Failed to resolve host name '%s'\n", getpid(), domain);
		}

		if(sock > 0)
			close(sock);
	} else {
		TRACE("http[%d] Failed to create client socket\n", getpid());
	}

	return r;
}

static SSL_CTX *create_ssl_context(void) {
	const SSL_METHOD *method;

	OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
	SSL_load_error_strings();   /* Bring in and register error messages */
	method = TLSv1_2_client_method();  /* Create new client-method instance */

	return SSL_CTX_new(method);
}

static _err_t proxy_ssl_client_connection(_cstr_t domain, int port, _cstr_t _write = NULL, bool response = false) {
	_err_t r = E_FAIL;
	SSL_CTX *ssl_context = create_ssl_context();

	if (ssl_context) {
		int sd;
		struct hostent *host;
		struct sockaddr_in addr;
		_cstr_t proto = getenv(REQ_PROTOCOL);
		_char_t lb[128] = "";

		if (!proto)
			proto = "HTTP/1.1";

		TRACE("http[%d]: Connecting to %s:%d...\n", getpid(), domain, port);

		if ((host = gethostbyname(domain)) != NULL) {
			if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) > 0) {
				bzero(&addr, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_port = htons(port);
				addr.sin_addr.s_addr = *(long*)(host->h_addr);

				if (connect(sd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
					SSL *ssl = SSL_new(ssl_context);

					SSL_set_fd(ssl, sd);
					if (SSL_connect(ssl) == 1) {
						pthread_t pt;
						int nb_in = 0;
						unsigned int sum_in = 0;

						if (_write)
							SSL_write(ssl, (void *)_write, strlen(_write));

						if (response) {
							// send positive response
							int sz = snprintf(lb, sizeof(lb), "%s %d Connection Established\r\n\r\n", proto, HTTPRC_OK);
							io_write(lb, sz);
						}

						pthread_create(&pt, NULL, [] (void *udata) -> void * {
							int nb_out = 0;
							SSL **p = (SSL **)udata;
							unsigned int sum_out = 0;

							while ((nb_out = SSL_read(*p, _g_resp_buffer_, sizeof(_g_resp_buffer_))) > 0) {
								TRACE("http[%d] << [%d] %s", getpid(), nb_out, _g_resp_buffer_);
								if (io_write(_g_resp_buffer_, nb_out) <= 0)
									break;
								sum_out += nb_out;
								memset(_g_resp_buffer_, 0, nb_out);
							}

							TRACE("http[%d] out: %u\n", getpid(), sum_out);
							return NULL;
						}, &ssl);

						pthread_setname_np(pt, "SSL connection");
						pthread_detach(pt);

						while ((nb_in = io_read(_g_req_buffer_, sizeof(_g_req_buffer_))) > 0) {
							TRACE("http[%d] >> [%d] %s", getpid(), nb_in, _g_req_buffer_);
							if (SSL_write(ssl, _g_req_buffer_, nb_in) <= 0)
								break;
							sum_in += nb_in;
							memset(_g_req_buffer_, 0, nb_in);
						}

						TRACE("http[%d] in: %u\n", getpid(), sum_in);

						pthread_cancel(pt);
						r = E_OK;
					} else {
						TRACE("http[%d] Unable to establish SSL connection to '%s'\n", getpid(), domain);
						if (response) {
							int sz = snprintf(lb, sizeof(lb), "%s %d Bad Gateway\r\n\r\n", proto, HTTPRC_BAD_GATEWAY);
							io_write(lb, sz);
						}
					}

					SSL_free(ssl);
				} else {
					TRACE("http[%d] Unable to create client socket\n", getpid());
					if (response) {
						int sz = snprintf(lb, sizeof(lb), "%s %d Bad Gateway\r\n\r\n", proto, HTTPRC_BAD_GATEWAY);
						io_write(lb, sz);
					}
				}

				close(sd);
			}
		} else {
			TRACE("http[%d] Unable to resolve '%s'\n", getpid(), domain);
			if (response) {
				int sz = snprintf(lb, sizeof(lb), "%s %d Domain not found\r\n\r\n", proto, HTTPRC_NOT_FOUND);
				io_write(lb, sz);
			}
		}

		SSL_CTX_free(ssl_context);
	} else {
		TRACE("http[%d] Failed to create SSL client context\n", getpid());
	}

	return r;
}

static _char_t 	_g_proxy_dst_port_[8] = "443";
static _char_t 	_g_proxy_dst_host_[256] = "";

_err_t do_connect(_cstr_t method, _cstr_t scheme, _cstr_t domain, _cstr_t port, _cstr_t uri, _cstr_t proto) {
	_err_t r = E_FAIL;
	_char_t lb[1024] = "";
	int _port = (port) ? atoi(port) : 0;;


	memset(lb, 0, sizeof(lb));

	TRACE("domain: %s\n", domain);
	TRACE("uri: %s\n", uri);

	if (strcasecmp(scheme, "http") == 0) {
		if (!_port)
			_port = 80;

		snprintf(lb, sizeof(lb), "%s %s %s\r\n", method, uri, proto);
		TRACE("http[%d] RAW client '%s%s %d'\n", getpid(), domain, uri, _port);
		r = proxy_raw_client_connection(domain, _port, lb);
	} else if (strcasecmp(scheme, "https") == 0) {
		if (!port)
			_port = 443;

		snprintf(lb, sizeof(lb), "%s %s %s\r\n", method, uri, proto);
		TRACE("http[%d] SSL client '%s%s %d'\n", getpid(), domain, uri, _port);
		r = proxy_ssl_client_connection(domain, _port, lb);
	}

	return r;
}

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
		if (port == 443 || port == 8443) {
			TRACE("http[%d]: RAW client. '%s %d'\n", getpid(), _g_proxy_dst_host_, port);
			r = proxy_ssl_client_connection(_g_proxy_dst_host_, port, NULL, true);
		} else {
			TRACE("http[%d]: RAW client. '%s %d'\n", getpid(), _g_proxy_dst_host_, port);
			r = proxy_raw_client_connection(_g_proxy_dst_host_, port, NULL, true);
		}
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
			resp.i_method = (resp.s_method) ? resolve_method(resp.s_method) : 0;

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
