#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string>
#include <map>
#include "http.h"
#include "argv.h"
#include "json.h"
#include "hfile.h"
#include "trace.h"

#define VHOST_CAPACITY		128
#define VHOST_DATA_SIZE		64 // in KBytes
#define MAPPING_CAPACITY	256
#define MAPPING_DATA_SIZE	64

#define VHOST_SRC	"http.json"
#define VHOST_DAT	"http.dat"
#define VHOST_LOCK	"http.lock"
#define MAPPING_SRC	"mapping.json"
#define MAPPING_DAT	"mapping.dat"
#define MAPPING_LOCK	"mapping.lock"

#define DEFAULT_HOST	"default"
#define RC_PREFIX	"RC_" // response code prefix

static _hf_context_t _g_vhost_cxt_;

typedef std::map<std::string, _hf_context_t> _vhost_mapping_t;

_vhost_mapping_t _g_mapping_;

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

static _err_t stat_compare(const char *src, const char *dat) {
	_err_t r = E_FAIL;
	static struct stat stat_dat;
	static struct stat stat_src;

	memset(&stat_src, 0, sizeof(struct stat));
	memset(&stat_dat, 0, sizeof(struct stat));

	stat(src, &stat_src);
	stat(dat, &stat_dat);

	if (stat_dat.st_mtim.tv_sec >= stat_src.st_mtim.tv_sec)
		r = E_OK;

	return r;
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

static void fill_vhost(_json_context_t *p_jcxt, _json_object_t *pjo, _vhost_t *pvh) {
	_json_value_t *pjv_host = json_select(p_jcxt, "host", pjo);
	_json_value_t *pjv_root = json_select(p_jcxt, "root", pjo);
	_json_value_t *pjv_timeout = json_select(p_jcxt, "timeout", pjo);

	memset(pvh, 0, sizeof(_vhost_t));

	jv_string(pjv_host, pvh->host, sizeof(pvh->host));
	jv_string(pjv_root, pvh->root, sizeof(pvh->root));
	pvh->timeout = atoi(jv_string(pjv_timeout).c_str());
}

static _err_t compile_vhosts(const char *json_fname, const char *dat_fname) {
	_err_t r = E_FAIL;
	int fd = -1;
	_u64 size = 0;
	_u8 *content = NULL;

	if ((content = map_file(json_fname, &fd, &size))) {
		_json_context_t *p_jcxt = json_create_context(
						// memory allocation
						[] (_u32 size, __attribute__((unused)) void *udata) ->void* {
							return malloc(size);
						},
						// memory free
						[] (void *ptr, __attribute__((unused)) _u32 size,
								__attribute__((unused)) void *udata) {
							free(ptr);
						}, NULL);

		if (json_parse(p_jcxt, content, size) == JSON_OK) {
			_json_value_t *pjv_http = json_select(p_jcxt, "http", NULL);

			if (pjv_http && pjv_http->jvt == JSON_OBJECT) {
				if (hf_create(&_g_vhost_cxt_, dat_fname,
						VHOST_CAPACITY, VHOST_DATA_SIZE) == 0) {
					_vhost_t rec;

					_json_value_t *pjv_default = json_select(p_jcxt, DEFAULT_HOST, &(pjv_http->object));

					if (pjv_default && pjv_default->jvt == JSON_OBJECT) {
						fill_vhost(p_jcxt, &(pjv_default->object), &rec);
						strncpy(rec.host, DEFAULT_HOST, sizeof(rec.host));
						// Add to hash table
						hf_add(&_g_vhost_cxt_, (void *)DEFAULT_HOST, strlen(DEFAULT_HOST), &rec, rec._size());
					}

					_json_value_t *pjv_vhost = json_select(p_jcxt, "vhost", &(pjv_http->object));

					if (pjv_vhost && pjv_vhost->jvt == JSON_ARRAY) {
						unsigned int i = 0;
						_json_value_t *pjvae = NULL;

						while ((pjvae = json_array_element(&(pjv_vhost->array), i))) {
							if (pjvae && pjvae->jvt == JSON_OBJECT) {
								fill_vhost(p_jcxt, &(pjvae->object), &rec);
								hf_add(&_g_vhost_cxt_, rec.host, strlen(rec.host), &rec, rec._size());
							}

							i++;
						}
					}

					r = E_OK;
				}
			}
		} else {
			TRACE("http[%d] Failed to parse '%s'\n", getpid(), json_fname);
		}

		json_destroy_context(p_jcxt);
		munmap(content, size);
		close(fd);
	}

	return r;
}

static void fill_url_rec(_json_context_t *p_jcxt, _json_object_t *pjo, _mapping_t *p) {
	_json_value_t *method = json_select(p_jcxt, "method", pjo);
	_json_value_t *url = json_select(p_jcxt, "url", pjo);
	_json_value_t *header = json_select(p_jcxt, "header", pjo);
	_json_value_t *no_stderr = json_select(p_jcxt, "no-stderr", pjo);
	_json_value_t *exec = json_select(p_jcxt, "exec", pjo);
	_json_value_t *response = json_select(p_jcxt, "response", pjo);

	p->type = MAPPING_TYPE_URL;
	jv_string(method, p->url.method, sizeof(p->url.method));
	jv_string(url, p->url.url, sizeof(p->url.url));
	if (header)
		p->url.header = (header->jvt == JSON_TRUE);
	if (no_stderr)
		p->url.no_stderr = (no_stderr->jvt == JSON_TRUE);
	if (exec && exec->jvt == JSON_STRING) {
		p->url.exec = true;
		jv_string(exec, p->url.proc, sizeof(p->url.proc));
	} else if (response && response->jvt == JSON_STRING)
		jv_string(response, p->url.proc, sizeof(p->url.proc));
}

static void fill_err_rec(_json_context_t *p_jcxt, _json_object_t *pjo, _mapping_t *p) {
	_json_value_t *code = json_select(p_jcxt, "code", pjo);
	_json_value_t *header = json_select(p_jcxt, "header", pjo);
	_json_value_t *no_stderr = json_select(p_jcxt, "no-stderr", pjo);
	_json_value_t *exec = json_select(p_jcxt, "exec", pjo);
	_json_value_t *response = json_select(p_jcxt, "response", pjo);
	char str_code[32] = "";

	p->type = MAPPING_TYPE_ERR;
	jv_string(code, str_code, sizeof(str_code));
	p->err.code = atoi(str_code);
	if (header)
		p->err.header = (header->jvt == JSON_TRUE);
	if (no_stderr)
		p->err.no_stderr = (no_stderr->jvt == JSON_TRUE);
	if (exec && exec->jvt == JSON_STRING) {
		p->err.exec = true;
		jv_string(exec, p->err.proc, sizeof(p->err.proc));
	} else if (response && response->jvt == JSON_STRING)
		jv_string(response, p->err.proc, sizeof(p->err.proc));
}

static _err_t compile_mapping(const char *json_fname, const char *dat_fname, _hf_context_t *p_hfcxt) {
	_err_t r = E_FAIL;
	int fd = -1;
	_u64 size = 0;
	_u8 *content = map_file(json_fname, &fd, &size);

	if (content) {
		_json_context_t *p_jcxt = json_create_context(
						// memory allocation
						[] (_u32 size, __attribute__((unused)) void *udata) ->void* {
							return malloc(size);
						},
						// memory free
						[] (void *ptr, __attribute__((unused)) _u32 size,
								__attribute__((unused)) void *udata) {
							free(ptr);
						}, NULL);

		if (json_parse(p_jcxt, content, size) == JSON_OK) {
			_json_value_t *pjv_mapping = json_select(p_jcxt, "mapping", NULL);

			if (pjv_mapping && pjv_mapping->jvt == JSON_OBJECT) {
				if (hf_create(p_hfcxt, dat_fname,
						MAPPING_CAPACITY, MAPPING_DATA_SIZE) == 0) {
					_mapping_t rec;
					unsigned int i = 0;

					_json_value_t *pjv_url = json_select(p_jcxt, "url", &(pjv_mapping->object));

					if (pjv_url && pjv_url->jvt == JSON_ARRAY) {
						_json_value_t *pjv = NULL;

						i = 0;
						while ((pjv = json_array_element(&(pjv_url->array), i))) {
							if (pjv->jvt == JSON_OBJECT) {
								char key[256] = "";
								unsigned int l = 0;

								memset(&rec, 0, sizeof(_mapping_t));
								fill_url_rec(p_jcxt, &(pjv->object), &rec);
								l = snprintf(key, sizeof(key), "%s_%s", rec.url.method, rec.url.url);
								hf_add(p_hfcxt, key, l, &rec, rec._size());
							}

							i++;
						}
					}

					_json_value_t *pjv_err = json_select(p_jcxt, "err", &(pjv_mapping->object));

					if (pjv_err && pjv_err->jvt == JSON_ARRAY) {
						_json_value_t *pjv = NULL;

						i = 0;
						while ((pjv = json_array_element(&(pjv_err->array), i))) {
							if (pjv->jvt == JSON_OBJECT) {
								char key[32] = "";
								unsigned int l = 0;

								memset(&rec, 0, sizeof(_mapping_t));
								fill_err_rec(p_jcxt, &(pjv->object), &rec);
								l = snprintf(key, sizeof(key), RC_PREFIX "%d", rec.err.code);
								hf_add(p_hfcxt, key, l, &rec, rec._size());
							}

							i++;
						}
					}

					r = E_OK;
				}
			}
		}

		json_destroy_context(p_jcxt);
		munmap(content, size);
		close(fd);
	}

	return r;
}

static void touch(const char *path) {
	close(open(path, O_CREAT | S_IRUSR | S_IWUSR));
}

static _err_t load_vhosts(void) {
	_err_t r = E_FAIL;
	char src_path[MAX_PATH] = "", dat_path[MAX_PATH] = "";
	const char *dir = argv_value(OPT_DIR);

	memset(&_g_vhost_cxt_, 0, sizeof(_hf_context_t));
	snprintf(src_path, sizeof(src_path), "%s/%s", dir, VHOST_SRC);
	snprintf(dat_path, sizeof(dat_path), "%s/%s", dir, VHOST_DAT);

_vhost_stat_cmp_:
	if (stat_compare(src_path, dat_path) != E_OK) {
		char lock_path[MAX_PATH] = "";

		snprintf(lock_path, sizeof(lock_path), "%s/%s", dir, VHOST_LOCK);

		if (access(lock_path, F_OK) == F_OK) {
			while (access(lock_path, F_OK) == F_OK)
				usleep(10000);

			goto _vhost_stat_cmp_;
		}

		touch(lock_path);
		r = compile_vhosts(src_path, dat_path);
		unlink(lock_path);
	} else
		r = hf_open(&_g_vhost_cxt_, dat_path, O_RDONLY);

	return r;
}

/**
Load vhost mapping by _vhost_t pointer */
_err_t cfg_load_mapping(_vhost_t *pvhost) {
	_err_t r = E_FAIL;
	_vhost_mapping_t::iterator it = _g_mapping_.find(pvhost->host);

	if (it == _g_mapping_.end()) {
		_hf_context_t hf_cxt;
		char src_path[MAX_PATH] = "", dat_path[MAX_PATH] = "";
		char *root = pvhost->root;

		memset(&hf_cxt, 0, sizeof(_hf_context_t));
		snprintf(src_path, sizeof(src_path), "%s/%s", root, MAPPING_SRC);
		snprintf(dat_path, sizeof(dat_path), "%s/%s", root, MAPPING_DAT);

_mapping_stat_cmp_:
		if (stat_compare(src_path, dat_path) != E_OK) {
			char lock_path[MAX_PATH] = "";

			snprintf(lock_path, sizeof(lock_path), "%s/%s", root, MAPPING_LOCK);

			if (access(lock_path, F_OK) == F_OK) {
				while (access(lock_path, F_OK) == F_OK)
					usleep(10000);

				goto _mapping_stat_cmp_;
			}

			touch(lock_path);
			r = compile_mapping(src_path, dat_path, &hf_cxt);
			unlink(lock_path);
		} else
			r = hf_open(&hf_cxt, dat_path, O_RDONLY);

		_g_mapping_.insert({pvhost->host, hf_cxt});
	} else
		r = E_OK;

	return r;
}

/**
Load mapping by name of virtual host */
_err_t cfg_load_mapping(_cstr_t vhost) {
	_err_t r = E_FAIL;
	_vhost_t *pvhost = cfg_get_vhost(vhost);

	if (pvhost)
		r = cfg_load_mapping(pvhost);

	return r;
}

_err_t cfg_init(void) {
	_err_t r = E_FAIL;

	r = load_vhosts();

	if (argv_check(OPT_LISTEN)) {
		hf_enum(&_g_vhost_cxt_, [] (void *p,
				__attribute__((unused)) unsigned int size,
				__attribute__((unused)) void *udata)->int {
			_vhost_t *pvhost = (_vhost_t *)p;

			cfg_load_mapping(pvhost);
			return 0;
		}, NULL);
	}

	return r;
}

/**
Returns pointer to _vhost_t ot NULL */
_vhost_t *cfg_get_vhost(_cstr_t host) {
	_vhost_t *r = NULL;

	if (!(r = (_vhost_t *)hf_get(&_g_vhost_cxt_, (void *)host, strlen(host))))
		r = (_vhost_t *)hf_get(&_g_vhost_cxt_, (void *)DEFAULT_HOST, strlen(DEFAULT_HOST));

	return r;
}

/**
Get mapping record by vhost record and URL */
_mapping_t *cfg_get_url_mapping(_vhost_t *pvhost, _cstr_t method, _cstr_t url) {
	_mapping_t *r = NULL;

	if (pvhost)
		r = cfg_get_url_mapping(pvhost->host, method, url);

	return r;
}

/**
Get mapping record by vhost name and URL */
_mapping_t *cfg_get_url_mapping(_cstr_t host, _cstr_t method, _cstr_t url) {
	_mapping_t *r = NULL;
	_vhost_mapping_t::iterator it = _g_mapping_.find(host);

	if (it != _g_mapping_.end()) {
		_hf_context_t *phf_cxt = &(*it).second;

		if (phf_cxt) {
			char key[256] = "";
			unsigned int l = snprintf(key, sizeof(key), "%s_%s", method, url);

			r = (_mapping_t *)hf_get(phf_cxt, key, l);
		}
	}

	return r;
}

/**
Get mapping record by vhost record and response code */
_mapping_t *cfg_get_err_mapping(_vhost_t *pvhost, short rc) {
	_mapping_t *r = NULL;

	if (pvhost)
		r = cfg_get_err_mapping(pvhost->host, rc);

	return r;
}

/**
Get mapping record by vhost name and response code */
_mapping_t *cfg_get_err_mapping(_cstr_t host, short rc) {
	_mapping_t *r = NULL;
	_vhost_mapping_t::iterator it = _g_mapping_.find(host);

	if (it != _g_mapping_.end()) {
		_hf_context_t *phf_cxt = &(*it).second;

		if (phf_cxt) {
			char key[32] = "";
			unsigned int l = snprintf(key, sizeof(key), RC_PREFIX "%d", rc);

			r = (_mapping_t *)hf_get(phf_cxt, key, l);
		}
	}

	return r;
}
