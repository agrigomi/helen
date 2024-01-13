#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "http.h"
#include "argv.h"
#include "trace.h"

static SSL_CTX *_g_ssl_context_ = NULL;
static SSL	*_g_ssl_in_ = NULL;
static SSL	*_g_ssl_out_ = NULL;
static bool	_g_fork_ = false;
static bool	_g_listening_ = false;
static int	_g_server_fd_ = -1;

static _err_t setup_ssl_context(const char *method, const char *cert, const char *key) {
	_err_t r = E_FAIL;
	const SSL_METHOD *ssl_method = ssl_select_method(method);

	if (ssl_method) {
		if ((_g_ssl_context_ = ssl_create_context(ssl_method))) {
			if (ssl_load_cert(_g_ssl_context_, cert)) {
				if (ssl_load_key(_g_ssl_context_, key))
					r = E_OK;
				else {
					TRACE("http[%d] Failed to load SSL key '%s'\n", getpid(), key);
					SSL_CTX_free(_g_ssl_context_);
					_g_ssl_context_ = NULL;
				}
			} else {
				TRACE("http[%d] Failed to load SSL certificate '%s'\n", getpid(), cert);
				SSL_CTX_free(_g_ssl_context_);
				_g_ssl_context_ = NULL;
			}
		} else {
			TRACE("http[%d] Unable to allocate SSL context\n", getpid());
		}
	} else {
		TRACE("http[%d] Unsupported SSL method '%s'\n", getpid(), method);
	}

	return r;
}

static _err_t setup_ssl_io(int fd, SSL **pp_ssl) {
	_err_t r = E_FAIL;

	if (_g_ssl_context_) {
		SSL *pssl = SSL_new(_g_ssl_context_);

		if (pssl) {
			SSL_set_fd(pssl, fd);
			SSL_set_accept_state(pssl);
			if (SSL_accept(pssl)) {
				*pp_ssl = pssl;
				r = E_OK;
			} else
				SSL_free(pssl);
		}
	}

	return r;
}

static _err_t setup_server_socket(int *p_fd) {
	_err_t r = E_FAIL;

	if ((*p_fd = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
		static struct sockaddr_in serv;
		_s32 opt = 1;
		int port = atoi(argv_value(OPT_PORT));

		setsockopt(*p_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		setsockopt(*p_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

		serv.sin_family = AF_INET;
		serv.sin_port = htons(port);
		serv.sin_addr.s_addr = htonl(INADDR_ANY);

		if ((bind(*p_fd, (struct sockaddr *)&serv, sizeof(struct sockaddr_in))) == 0) {
			if (listen(*p_fd, SOMAXCONN) == 0)
				r = E_OK;
			else
				close(*p_fd);
		} else
			close(*p_fd);
	}

	return r;
}

static void server_accept(int server_fd, int tmout) {
	_g_listening_ = true;

	while (_g_listening_) {
		int sl;
		struct sockaddr_in client;
		socklen_t clen = sizeof(struct sockaddr_in);
		pid_t cpid;

		if ((sl = accept(server_fd, (struct sockaddr *) &client, &clen)) > 0) {
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

			if (tmout > 0) {
				struct timeval timeout;

				timeout.tv_sec = tmout;
				timeout.tv_usec = 0;

				setsockopt(sl, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
				setsockopt(sl, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
			}

			TRACE("\nhttp[%d]: Incoming connection from %s\n", getpid(), strip);
			setenv("PEER_IP", strip, 1);

			if ((cpid = fork()) == 0) { // child
				_g_fork_ = true;
				if (_g_ssl_context_) { // SSL case
					SSL *cl_cxt = NULL;

					if (setup_ssl_io(sl, &cl_cxt) == E_OK) {
						_g_ssl_in_ = _g_ssl_out_ = cl_cxt;
						TRACE("http[%d]: Fork PID = %d SSL I/O\n", getpid(), getpid());
					} else {
						TRACE("http[%d]: Failed to setup SSL I/O\n", getpid());
					}
				} else {
					dup2(sl, STDIN_FILENO);
					dup2(sl, STDOUT_FILENO);
					TRACE("http[%d]: Fork PID = %d RAW I/O\n", getpid(), getpid());
				}

				close(_g_server_fd_);
				break;
			}

			close(sl);
		} else {
			TRACE("http[%d] Stop listening\n", getpid());
			break;
		}
	}

	_g_listening_ = false;
}

static int wait_input(int fd, int tmout) {
	fd_set selectset;
	struct timeval timeout = {tmout, 0}; //timeout in sec.

	FD_ZERO(&selectset);
	FD_SET(fd, &selectset);

	return select(fd + 1, &selectset, NULL, NULL, &timeout);
}

/**
Wait for input with timeout in seconds
return number of bytes  */
int io_wait_input(int timeout) {
	int r = 0;

	if (_g_ssl_in_)
		r = wait_input(SSL_get_fd(_g_ssl_in_), timeout);
	else
		r = wait_input(STDIN_FILENO, timeout);

	return r;
}

int io_get_stdin_fd(void) {
	int r = STDIN_FILENO;

	if (_g_ssl_in_)
		r = SSL_get_fd(_g_ssl_in_);

	return r;
}

int io_verify_input(void) {
	int r = 0;

	if (_g_ssl_in_)
		ioctl(SSL_get_fd(_g_ssl_in_), FIONREAD, &r);
	else
		ioctl(STDIN_FILENO, FIONREAD, &r);

	return r;
}

/**
return >0 for number of received bytes <=0 means fail */
int io_read(char *buffer, int size) {
	int r = -1;

	if (_g_ssl_in_)
		r = ssl_read(_g_ssl_in_, buffer, size);
	else
		r = read(STDIN_FILENO, buffer, size);

	return r;
}

/**
return >0 for number of sent bytes <=0 means fail */
int io_write(const char *buffer, int size) {
	int r = -1;

	if (_g_ssl_out_)
		r = ssl_write(_g_ssl_out_, buffer, size);
	else
		r = write(STDOUT_FILENO, buffer, size);

	return r;
}

/**
formatted output
return >0 for number of sent bytes <=0 means fail */
int io_fwrite(const char *fmt, ...) {
	int r = -1;
	va_list va;

	va_start(va, fmt);

	if (_g_ssl_out_) {
		char lb[2048];

		r = vsnprintf(lb, sizeof(lb), fmt, va);
		r = ssl_write(_g_ssl_out_, lb, r);
	} else
		r = vdprintf(STDOUT_FILENO, fmt, va);

	va_end(va);

	return r;
}

/**
Read line from input stream
return line size (without \r\n) */
int io_read_line(char *buffer, int size) {
	int r = 0;

	if (_g_ssl_in_)
		// SSL input
		r = ssl_read_line(_g_ssl_in_, buffer, size);
	else { // STDIO input
		while (r < size) {
			if (read(STDIN_FILENO, (buffer + r), 1) == 1) {
				if (*(buffer + r) == '\n') {
					*(buffer + r) = 0;
					break;
				} else if (*(buffer + r) == '\r')
					;
				else
					r++;
			} else {
				r = -1;
				break;
			}
		}
	}

	return r;
}

static _err_t io_loop(int timeout) {
	_err_t r = E_OK;
	int req_len = 0;

	while ((r = req_receive(timeout, &req_len)) == E_OK) {
		if (req_len > 0) {
			if ((r = res_processing()) == E_OK) {
				_cstr_t connection = getenv(REQ_CONNECTION);

				if (connection && strcasecmp(connection, "close") == 0)
					break;
			} else
				break;
		}
	}

	return r;
}

bool io_is_ssl(void) {
	return (_g_ssl_in_) ? true : false;
}

_err_t io_start(void) {
	_err_t r = E_OK;
	const char *ssl_method = NULL;
	const char *ssl_cert = NULL;
	const char *ssl_key = NULL;
	int timeout = atoi(argv_value(OPT_TIMEOUT));

	if (!(ssl_method = argv_value(OPT_SSL_METHOD)))
		ssl_method = getenv(OPT_SSL_METHOD);
	if (!(ssl_cert = argv_value(OPT_SSL_CERT)))
		ssl_cert = getenv(OPT_SSL_CERT);
	if (!(ssl_key = argv_value(OPT_SSL_KEY)))
		ssl_key = getenv(OPT_SSL_KEY);

	if (ssl_method && ssl_cert && ssl_key) {
		static char b_cert[MAX_PATH] = "";
		static char b_key[MAX_PATH] = "";
		const char *dir = argv_value(OPT_DIR);

		snprintf(b_cert, sizeof(b_cert), "%s/%s", dir, ssl_cert);
		snprintf(b_key, sizeof(b_key), "%s/%s", dir, ssl_key);

		// setup SSL context
		setup_ssl_context(ssl_method, b_cert, b_key);
	}

	if (argv_check(OPT_LISTEN)) {
		// setup server
		if ((r = setup_server_socket(&_g_server_fd_)) == E_OK)
			// start listening
			server_accept(_g_server_fd_, timeout);
	} else {
		if (_g_ssl_context_) {
			// SSL through STDIO
			SSL *pssl = NULL;

			if (setup_ssl_io(STDIN_FILENO, &pssl) == E_OK)
				_g_ssl_in_ = _g_ssl_out_ = pssl;
		}

		// Assume that the fork is outside initiated.
		_g_fork_ = true;
	}

	if (_g_fork_)
		r = io_loop(timeout);

	if (_g_ssl_in_)
		SSL_free(_g_ssl_in_);

	if (_g_ssl_context_)
		SSL_CTX_free(_g_ssl_context_);

	return r;
}
