#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include "http.h"
#include "lock.h"

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
		int cfd = open(cache_path, O_CREAT | O_WRONLY | O_TRUNC);

		if (cfd > 0) {
			unsigned char buffer[MAX_COMPRESSION_CHUNK];

			lock_fd(cfd);

			r = rt_compress_stream(encoding, cfd,
					buffer, sizeof(buffer),
					[] (unsigned char *data, unsigned int *psz, void *udata) -> int {
						int r = ENCODING_CONTINUE;
						int fd = *(int *)udata;
						unsigned int sz = read(fd, data, *psz);

						if (sz < *psz)
							r = ENCODING_FINISHED;

						*psz = sz;

						return r;
					}, &sfd, NULL);

			unlock(cfd);
			close(cfd);
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
	_cstr_t enc_name = rt_encoding_bit_to_name(&enc);
	struct stat o_stat, c_stat;

	if (enc) {
		if (stat(path, &o_stat) > 0) {
			_char_t cache_path[MAX_PATH];
			_char_t sha1_fname[SHA1HashSize * 2 + 1];

			memset(sha1_fname, 0, sizeof(sha1_fname));
			rt_sha1_string(path, sha1_fname, sizeof(sha1_fname));
			snprintf(cache_path, sizeof(cache_path), "%s/%s/%s", _g_cache_path_, enc_name, sha1_fname);

			if (stat(cache_path, &c_stat) == 0) {
				// cache file exists (compare time)
				//...
			} else {
				// cache file don't exists
				if (cache_update(path, cache_path, enc) == E_OK) {
					if (stat(cache_path, p_stat) == 0)
						r = open(cache_path, O_RDONLY);
				}
			}

			*encoding = enc_name;
		}
	} else {
		if (stat(path, p_stat) == 0)
			r = open(path, O_RDONLY);
	}

	return r;
}
