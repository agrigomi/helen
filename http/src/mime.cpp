#include <string.h>
#include <utime.h>
#include <unistd.h>
#include "argv.h"
#include "hfile.h"
#include "http.h"

#define MIME_TYPES_SRC	"/etc/mime.types"
#define MIME_TYPES_DST	"mime.types.dat"
#define MIME_TYPES_LOCK	"mime.types.lock"

#define	MT_CAPACITY	2048
#define	MT_DATA		64

static _hf_context_t	_g_hf_cxt_;
static volatile bool	_g_hf_open_ = false;

static void touch(const char *path) {
	close(open(path, O_CREAT | S_IRUSR | S_IWUSR));
}

_err_t mime_open(void) {
	_err_t r = E_FAIL;

	if (!_g_hf_open_) {
		struct utimbuf tm_src;
		struct utimbuf tm_dst;
		char dst_path[MAX_PATH] = "";
		char lock_path[MAX_PATH] = "";

		memset(&tm_src, 0, sizeof(struct utimbuf));
		memset(&tm_dst, 0, sizeof(struct utimbuf));
		snprintf(dst_path, sizeof(dst_path), "%s/%s", argv_value(OPT_DIR), MIME_TYPES_DST);
		snprintf(lock_path, sizeof(lock_path), "%s/%s", argv_value(OPT_DIR), MIME_TYPES_LOCK);

_compare_time_:
		utime(MIME_TYPES_SRC, &tm_src);
		utime(dst_path, &tm_dst);

		if (tm_src.modtime > tm_dst.modtime) {
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
				//...
				unlink(lock_path);
			}
		} else
			r = hf_open(&_g_hf_cxt_, dst_path, O_RDONLY);

		_g_hf_open_ = (r == E_OK);
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
