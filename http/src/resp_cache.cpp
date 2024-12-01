#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include "http.h"
#include "lock.h"
#include "trace.h"

static _cstr_t _g_cache_path_ = NULL;

static void create_folders(_cstr_t path) {
	_char_t lb[MAX_PATH];
	unsigned int enc = SUPPORTED_ENCODING;
	int uenc = 0;
	_cstr_t fldr = NULL;

	mkdir(path, 0755);
	_g_cache_path_ = path;

	while ((fldr = rt_encoding_bit_to_name(&enc))) {
		snprintf(lb, sizeof(lb), "%s/%s", path, fldr);
		mkdir(lb, 0755);
		uenc |= enc;
		enc = SUPPORTED_ENCODING;
		if (!(enc &= ~uenc))
			break;
	}
}

void cache_init(_cstr_t path) {
	create_folders(path);
}

static _err_t cache_update(_cstr_t path, _cstr_t cache_path, unsigned int encoding) {
	unsigned int r = E_FAIL;
	int sfd = open(path, O_RDONLY);

	if (sfd > 0) {
		int cfd = open(cache_path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
		_cstr_t str_enc = NULL;

		if (cfd > 0) {
			lock_fd(cfd);

			r = rt_compress_stream(encoding, sfd, cfd, &str_enc);

			unlock(cfd);
			close(cfd);

			if (r != E_OK)
				unlink(cache_path);
		}

		close(sfd);
	}

	return r;
}

int cache_open(_cstr_t path/* in */,
		struct stat *p_stat,
		_cstr_t *encoding/* in/out Can be NULL */
		) {
	int r = -1;
	unsigned int enc = rt_select_encoding(rt_file_ext(path));
	struct stat o_stat, c_stat;

	if (enc) {
		if (stat(path, &o_stat) == 0) {
			if (o_stat.st_size > COMPRESSION_TRESHOLD) {
				_cstr_t enc_name = rt_encoding_bit_to_name(&enc);
				_char_t cache_path[MAX_PATH];
				_char_t sha1_fname[SHA1HashSize * 2 + 1];

				// cache path
				memset(sha1_fname, 0, sizeof(sha1_fname));
				rt_sha1_string(path, sha1_fname, sizeof(sha1_fname));
				snprintf(cache_path, sizeof(cache_path), "%s/%s/%s", _g_cache_path_, enc_name, sha1_fname);

				if (stat(cache_path, &c_stat) == 0) {
					// cache file exists (compare time)
					if (o_stat.st_mtime > c_stat.st_mtime)
						cache_update(path, cache_path, enc);
					else {
						memcpy(p_stat, &c_stat, sizeof(struct stat));
						goto _skip_stat_;
					}
				} else
					// cache file don't exists
					cache_update(path, cache_path, enc);

				if (stat(cache_path, p_stat) == 0) {
_skip_stat_:
					TRACE("http[%d] Use cache file '%s'\n", getpid(), cache_path);
					r = open(cache_path, O_RDONLY);
					*encoding = enc_name;
				} else
					// no cache file
					goto _skip_compression_;
			} else {
_skip_compression_:
				// less than compression threshold
				if ((r = open(path, O_RDONLY)) > 0) {
					memcpy(p_stat, &o_stat, sizeof(struct stat));
					*encoding = NULL;
				}
			}
		}
	} else {
		TRACE("http[%d] Skip encoding for '%s'\n", getpid(), path);
		if (stat(path, p_stat) == 0) {
			r = open(path, O_RDONLY);
			*encoding = NULL;
		}
	}

	return r;
}
