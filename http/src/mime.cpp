#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <utime.h>
#include <unistd.h>
#include "argv.h"
#include "hfile.h"
#include "http.h"
#include "trace.h"
#include "str.h"

#define MIME_TYPES_SRC	"/etc/mime.types"
#define MIME_TYPES_DST	"mime.types.dat"
#define MIME_TYPES_LOCK	"mime.types.lock"

#define	MT_CAPACITY	4096
#define	MT_DATA		64

static _hf_context_t	_g_hf_cxt_;
static volatile bool	_g_hf_open_ = false;

static void touch(const char *path) {
	close(open(path, O_CREAT | S_IRUSR | S_IWUSR));
}

static _err_t read_line(FILE *fs, char *buffer, int size) {
	_err_t r = E_FAIL;

	memset(buffer, 0, size);

	if (fgets(buffer, size, fs))
		r = E_OK;

	return r;
}

static _err_t compile_mime_types(void) {
	_err_t r = E_FAIL;
	FILE *fsrc = fopen(MIME_TYPES_SRC, "r");

	if (fsrc) {
		char line[MAX_PATH];

		while (!feof(fsrc)) {
			if (read_line(fsrc, line, sizeof(line)) == E_OK) {
				if (line[0] != '#') { // skip comments
					size_t l = strlen(line);

					// remove EOL
					while (l > 0 && line[l] < ' ') {
						line[l] = 0;
						l--;
					}

					str_split(line, "\t", [] (int idx, char *str, void __attribute__((unused)) *udata) -> int {
						static char *ctype = NULL;

						if (idx == 0)
							ctype = str;
						else {
							str_split(str, " ", [] (int __attribute__((unused)) idx, char *str, void *udata) -> int {
								char *ctype = (char *)udata;
								char fext[32];
								int sz = snprintf(fext, sizeof(fext), ".%s", str);

								hf_add(&_g_hf_cxt_, fext, sz, ctype, strlen(ctype) + 1);

								return 0;
							}, ctype);
						}

						return 0;
					}, NULL);
				}
			} else
				break;
		}

		fclose(fsrc);
		r = E_OK;
	}

	return r;
}

_err_t mime_open(void) {
	_err_t r = E_FAIL;

	if (!_g_hf_open_) {
		struct stat st_src;
		struct stat st_dst;
		char dst_path[MAX_PATH] = "";
		char lock_path[MAX_PATH] = "";

		memset(&st_src, 0, sizeof(struct stat));
		memset(&st_dst, 0, sizeof(struct stat));
		snprintf(dst_path, sizeof(dst_path), "%s/%s", argv_value(OPT_DIR), MIME_TYPES_DST);
		snprintf(lock_path, sizeof(lock_path), "%s/%s", argv_value(OPT_DIR), MIME_TYPES_LOCK);

_compare_time_:
		stat(dst_path, &st_dst);

		if ((r = stat(MIME_TYPES_SRC, &st_src)) == E_OK) {
			if (st_src.st_mtime > st_dst.st_mtime) {
				if ((r = hf_create(&_g_hf_cxt_, dst_path, MT_CAPACITY, MT_DATA)) == E_OK) {
					// compile ...
					if (access(lock_path, F_OK) == F_OK) {
						// locked, wait for unlock
						while (access(lock_path, F_OK) == F_OK)
							usleep(10000);

						goto _compare_time_;
					}

					// lock
					touch(lock_path);
					r = compile_mime_types();
					// unlock
					unlink(lock_path);
				}
			} else
				r = hf_open(&_g_hf_cxt_, dst_path, O_RDONLY);

			_g_hf_open_ = (r == E_OK);
		}
	} else
		r = E_OK;

	return r;
}

void mime_close(void) {
	if (_g_hf_open_) {
		hf_close(&_g_hf_cxt_);
		_g_hf_open_ = false;
	}
}

_cstr_t mime_resolve(_cstr_t path) {
	_cstr_t r = NULL;

	if (_g_hf_open_) {
		size_t l = strlen(path);
		unsigned int sz;

		while (l && path[l] != '.')
			l--;

		r = (_cstr_t)hf_get(&_g_hf_cxt_, (void *)&path[l], strlen(&path[l]), &sz);
	}

	return r;
}
