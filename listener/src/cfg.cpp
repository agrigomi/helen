#include <sys/types.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <sys/wait.h>
#include "config.h"
#include "json.h"
#include "trace.h"
#include "cfg.h"
#include "argv.h"

extern void ssl_io(_listen_t *, SSL *);
extern bool _g_fork_;

static std::vector<_listen_t> _gv_listen;

static _u8 *map_file(_cstr_t fname, int *fd, _u64 *size) {
	_u8 *r = NULL;
	int _fd = open(fname, O_RDONLY);
	size_t _size = 0;

	if (_fd > 0) {
		_size = lseek(_fd, 0, SEEK_END);
		lseek(_fd, 0, SEEK_SET);
		if ((r = (_u8 *)mmap(NULL, _size, PROT_READ, MAP_SHARED, _fd, 0))) {
			*fd = _fd;
			*size = _size;
		} else
			close(_fd);
	}

	return r;
}

void cfg_enum_listen(void (*pcb)(_listen_t *, void *), void *udata) {
	std::vector<_listen_t>::iterator it = _gv_listen.begin();

	while (it != _gv_listen.end()) {
		pcb(&(*it),udata);
		it++;
	}
}

static std::string jv_string(_json_value_t *pjv) {
	std::string r("");

	if (pjv && (pjv->jvt == JSON_STRING || pjv->jvt == JSON_NUMBER))
		r.assign(pjv->string.data, pjv->string.size);

	return r;
}

static void jv_string(_json_value_t *pjv, _char_t *dst, unsigned int sz_dst) {
	if (pjv && (pjv->jvt == JSON_STRING || pjv->jvt == JSON_NUMBER))
		strncpy(dst, pjv->string.data, ((sz_dst-1) < pjv->string.size) ? (sz_dst-1) : pjv->string.size);
}

static void jv_string(_json_string_t *pjs, _char_t *dst, unsigned int sz_dst) {
	if (pjs)
		strncpy(dst, pjs->data, ((sz_dst-1) < pjs->size) ? (sz_dst-1) : pjs->size);
}

static void server_accept(_listen_t *pl) {
	pthread_create(&pl->thread, NULL, [](void *udata)->void* {
		_listen_t *pl = (_listen_t *)udata;

		pl->flags &= ~LISTEN_STOPPED;
		pl->flags |= LISTEN_RUNNING;

		pthread_setname_np(pl->thread, pl->name);
		TRACE("hl: Server '%s' running\n", pl->name);
		while (pl->flags & LISTEN_RUNNING) {
			int sl;
			struct sockaddr_in client;
			socklen_t clen = sizeof(struct sockaddr_in);
			pid_t cpid;

			if ((sl = accept(pl->server_fd, (struct sockaddr *) &client, &clen)) > 0) {
				int i = 0;
				_char_t strip[64] = "";
				struct sockaddr_storage addr;
				struct sockaddr_in *s = (struct sockaddr_in *)&addr;
				socklen_t _len = sizeof addr;

				getpeername(sl, (struct sockaddr*)&addr, &_len);
				inet_ntop(AF_INET, &s->sin_addr, strip, sizeof(strip));

				_s32 opt = 1;
				setsockopt(sl, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
				setsockopt(sl, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
				setsockopt(sl, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
				setsockopt(sl, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
				setsockopt(sl, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt));
				opt = 10;
				setsockopt(sl, SOL_TCP, TCP_KEEPIDLE, &opt, sizeof(opt));
				opt = 3;
				setsockopt(sl, SOL_TCP, TCP_KEEPCNT, &opt, sizeof(opt));

				if (pl->timeout > 0) {
					struct timeval timeout;
					timeout.tv_sec = pl->timeout;
					timeout.tv_usec = 0;

					setsockopt(sl, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
					setsockopt(sl, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
				}

				TRACE("hl[%d]: Incoming connection from %s on port %d\n", getpid(), strip, pl->port);

				if ((cpid = fork()) == 0) { // child
					_g_fork_ = true;
					cfg_enum_listen([](_listen_t *p, __attribute__((unused)) void *arg) {
						p->flags &= ~LISTEN_RUNNING;
						close(p->server_fd);
						p->server_fd = -1;
					}, pl);

					if (pl->ssl_enable && pl->ssl_context) { // SSL case
						SSL *cl_cxt = SSL_new(pl->ssl_context);

						if (cl_cxt) { // stop listen threads
							SSL_set_fd(cl_cxt, sl);
							SSL_set_accept_state(cl_cxt);
							ssl_io(pl, cl_cxt); // start I/O thread
							SSL_free(cl_cxt);
						} else {
							TRACE("hl[%d]: Failed to allocate SSL conection context\n", getpid());
						}
					} else { // execute child
						dup2(sl, STDIN_FILENO);
						dup2(sl, STDOUT_FILENO);
						if (pl->no_stderr == false)
							dup2(sl, STDERR_FILENO);
						if (execve(pl->argv[0], pl->argv, pl->env) == -1)
							TRACE("hl[%d]: Unable to execute '%s'\n", getpid(), pl->argv[0]);
					}

					exit(0);
				} else {
					TRACE("hl[%d]: Running '%s'; PID: %d; '", getpid(), pl->name, cpid);
					while (pl->argv[i]) {
						TRACE("%s ", pl->argv[i]);
						i++;
					}
					TRACE("'\n");
				}

				close(sl);
			}
		}

		TRACE("\nhl: Server '%s' stopped.\n", pl->name);

		pl->flags |= LISTEN_STOPPED;
		return NULL;
	}, pl);
}

void cfg_start(void) {
	cfg_enum_listen([](_listen_t *p, __attribute__((unused)) void *udata) {
		struct sockaddr_in serv;

		if ((p->server_fd = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
			_s32 opt = 1;

			setsockopt(p->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
			setsockopt(p->server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

			serv.sin_family = AF_INET;
			serv.sin_port = htons(p->port);
			serv.sin_addr.s_addr = htonl(INADDR_ANY);

			if ((bind(p->server_fd, (struct sockaddr *)&serv, sizeof(struct sockaddr_in))) == 0) {
				if (listen(p->server_fd, SOMAXCONN) == 0)
					server_accept(p);
				else
					close(p->server_fd);
			} else
				close(p->server_fd);
		}
	}, NULL);
}

void cfg_stop(void) {
	cfg_enum_listen([](_listen_t *p, __attribute__((unused)) void *udata) {
		TRACE("hl: Waiting '%s' to stop. ", p->name);
		p->flags &= ~LISTEN_RUNNING;
		if (p->server_fd > 0) {
			if (_g_fork_)
				close(p->server_fd);
			else {
				shutdown(p->server_fd, SHUT_RDWR);

				while (!(p->flags & LISTEN_STOPPED)) {
					TRACE(".");
					usleep(100000);
				}
			}
		}
	}, NULL);
}

static void init_config(void) {
	cfg_enum_listen([](_listen_t *p, __attribute__((unused)) void *udata) {
		int i = 0;

		for (i = 0; i < MAX_ARGV && p->argv[i]; i++)
			free(p->argv[i]);

		for (i = 0; i < MAX_ENV && p->env[i]; i++)
			free(p->env[i]);
	}, NULL);

	_gv_listen.clear();
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

static void parse_env_object(_json_object_t *pjo_env, _str_t dst_arr[], _u32 arr_size) {
	_u32 i = 0;
	_json_pair_t *p_jp = NULL;
	_char_t lb_name[256] = "";
	_char_t lb_value[256] = "";

	while ((p_jp = json_object_pair(pjo_env, i))) {
		_u32 l = 0;

		memset(lb_name, 0, sizeof(lb_name));
		memset(lb_value, 0, sizeof(lb_value));

		jv_string(&p_jp->name, lb_name, sizeof(lb_name));
		if (p_jp->value.jvt == JSON_STRING)
			jv_string(&p_jp->value, lb_value, sizeof(lb_value));

		l = strlen(lb_name) + strlen(lb_value) + 2;
		if ((dst_arr[i] = (_str_t)malloc(l))) {
			snprintf(dst_arr[i], l, "%s=%s", lb_name, lb_value);
			i++;
			if (i >= arr_size)
				break;
		}
	}
}

static void parse_env_array(_json_array_t *pja_env, _str_t dst_arr[], _u32 arr_size) {
	_json_value_t *p_jv = NULL;
	_u32 i = 0;

	while ((p_jv = json_array_element(pja_env, i))) {
		if (p_jv->jvt == JSON_STRING) {
			if ((dst_arr[i] = (_str_t)malloc(p_jv->string.size + 1))) {
				memset(dst_arr[i], 0, p_jv->string.size + 1);
				strncpy(dst_arr[i], p_jv->string.data, p_jv->string.size);
			}
		}

		i++;
		if (i >= arr_size)
			break;
	}
}

static void parse_ssl_object(_json_context_t *p_jcxt, _json_object_t *pjo_ssl, _listen_t *pl) {
	_json_value_t *pjv_enable = json_select(p_jcxt, "enable", pjo_ssl);
	_json_value_t *pjv_method = json_select(p_jcxt, "method", pjo_ssl);
	_json_value_t *pjv_cert = json_select(p_jcxt, "certificate", pjo_ssl);
	_json_value_t *pjv_key = json_select(p_jcxt, "key", pjo_ssl);

	if (pjv_enable && pjv_method && pjv_cert && pjv_key) {
		pl->ssl_enable = (pjv_enable->jvt == JSON_TRUE);
		jv_string(pjv_method, pl->ssl_method, sizeof(pl->ssl_method));
		jv_string(pjv_cert, pl->ssl_cert, sizeof(pl->ssl_cert));
		jv_string(pjv_key, pl->ssl_key, sizeof(pl->ssl_key));
	}
}

static void add_listener(_json_context_t *p_jcxt, _json_pair_t *p_jp) {
	_listen_t l;
	_json_value_t *pjv_port = json_select(p_jcxt, "port", &(p_jp->value.object));
	_json_value_t *pjv_exec = json_select(p_jcxt, "exec", &(p_jp->value.object));
	_json_value_t *pjv_env  = json_select(p_jcxt, "env",  &(p_jp->value.object));
	_json_value_t *pjv_tout = json_select(p_jcxt, "timeout",  &(p_jp->value.object));
	_json_value_t *pjv_nostderr = json_select(p_jcxt, "no-stderr",  &(p_jp->value.object));
	_json_value_t *pjv_ssl  = json_select(p_jcxt, "ssl",  &(p_jp->value.object));

	memset(&l, 0, sizeof(_listen_t));
	jv_string(&p_jp->name, l.name, sizeof(l.name));

	if (pjv_port && pjv_port->jvt == JSON_STRING &&  pjv_exec && pjv_exec->jvt == JSON_STRING) {
		l.port = atoi(jv_string(pjv_port).c_str());
		split_by_space(pjv_exec->string.data, pjv_exec->string.size, l.argv, MAX_ARGV);

		if (pjv_env) {
			if (pjv_env->jvt == JSON_OBJECT)
				parse_env_object(&pjv_env->object, l.env, MAX_ENV);
			else if (pjv_env->jvt == JSON_ARRAY)
				parse_env_array(&pjv_env->array, l.env, MAX_ENV);
		}

		l.timeout = atoi(jv_string(pjv_tout).c_str());

		if (pjv_nostderr)
			l.no_stderr = (pjv_nostderr->jvt == JSON_TRUE);

		if (pjv_ssl) {
			if (pjv_ssl->jvt == JSON_OBJECT)
				parse_ssl_object(p_jcxt, &pjv_ssl->object, &l);
		}

		if (l.ssl_enable) {
			const SSL_METHOD *ssl_method = ssl_select_method(l.ssl_method);

			TRACE("hl: Setup SSL/%s for incoming connections on port %d\n", l.ssl_method, l.port);
			if (ssl_method) {
				if ((l.ssl_context = ssl_create_context(ssl_method))) {
					if (SSL_CTX_use_certificate_file(l.ssl_context, l.ssl_cert, SSL_FILETYPE_PEM) > 0) {
						if (SSL_CTX_use_PrivateKey_file(l.ssl_context, l.ssl_key, SSL_FILETYPE_PEM) > 0) {
							if (!SSL_CTX_check_private_key(l.ssl_context)) {
								TRACE("hl: Private key does not match the public certificate");
								SSL_CTX_free(l.ssl_context);
								l.ssl_context = NULL;
								l.ssl_enable = false;
							}
						} else {
							TRACE("hl: Failed to load key file '%s'\n", l.ssl_key);
							SSL_CTX_free(l.ssl_context);
							l.ssl_context = NULL;
							l.ssl_enable = false;
						}
					} else {
						TRACE("hl: Failed to load certificate file '%s'\n", l.ssl_cert);
						SSL_CTX_free(l.ssl_context);
						l.ssl_context = NULL;
						l.ssl_enable = false;
					}
				} else {
					TRACE("hl: Failed to create SSL context\n");
					l.ssl_enable = false;
				}
			} else {
				TRACE("hl[%d] Unsupported SSL method '%s'\n", getpid(), l.ssl_method);
			}
		}

		l.flags = LISTEN_STOPPED;
		l.server_fd = -1;
		_gv_listen.push_back(l);
	}
}

_err_t cfg_load(_cstr_t fname) {
	_err_t r = E_FAIL;
	int cfg_fd = -1;
	_u64 cfg_size = 0;
	_u8 *content = map_file(fname, &cfg_fd, &cfg_size);

	if (content) {
		// Stop listen threads
		cfg_stop();
		// Initializing listen storage
		init_config();

		_json_context_t *p_jcxt = json_create_context(
						// memory allocation
						[](_u32 size, __attribute__((unused)) void *udata) ->void* {
							return malloc(size);
						},
						// memory free
						[](void *ptr, __attribute__((unused)) _u32 size,
								__attribute__((unused)) void *udata) {
							free(ptr);
						}, NULL);

		if (json_parse(p_jcxt, content, cfg_size) == JSON_OK) {
			_json_value_t *p_jv = json_select(p_jcxt, "listen", NULL);

			if (p_jv && p_jv->jvt == JSON_OBJECT) {
				_json_pair_t *p_jp = NULL;
				_u32 idx = 0;

				while ((p_jp = json_object_pair(&p_jv->object, idx))) {
					add_listener(p_jcxt, p_jp);
					idx++;
				}
			}

			r = E_OK;
		} else {
			TRACEfl("hl: Failed to parse JSON file '%s'\n", fname);
		}

		json_destroy_context(p_jcxt);
		munmap(content, cfg_size);
		close(cfg_fd);
	}

	return r;
}

