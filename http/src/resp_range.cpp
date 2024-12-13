#include <string.h>
#include <sys/stat.h>
#include "str.h"
#include "http.h"

static _v_range_t _gv_ranges_; // ranges vector
static _range_t _g_range_; // range element

void range_generate_boundary(_cstr_t path, _str_t b, int sz) {
	rt_sha1_string(path, b, sz);
}

typedef struct {
	_cstr_t path;
	_cstr_t boundary;
	struct stat st;
} _rdata_t;

_v_range_t *range_parse(_cstr_t req_range, _cstr_t path, _cstr_t boundary) {
	_v_range_t *r = NULL;
	_cstr_t range = (req_range) ? req_range : getenv(REQ_RANGE);
	_char_t vhdr[256];
	_rdata_t rdata;

	_gv_ranges_.clear();

	if (range) {
		rdata.path = path;
		rdata.boundary = boundary;
		stat(path, &rdata.st);

		strncpy(vhdr, range, sizeof(vhdr) - 1);

		str_split(vhdr, "=", [] (int idx, char *str, void *udata) -> int {
			int r = -1;
			_rdata_t *p = (_rdata_t *)udata;

			if (idx == 0) {
				// Range units
				if (strncasecmp(str, "bytes", 5) == 0)
					r = 0;
			} else if (idx == 1) {
				// ranges
				str_split(str, ",", [] (int __attribute__((unused)) idx, char *str,
						void *udata) -> int {
					int r = 0;
					_rdata_t *p = (_rdata_t *)udata;

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
						mime_open();

						_cstr_t mt = mime_resolve(p->path);

						if (mt)
							i += snprintf(_g_range_.header + i, sizeof(_g_range_.header) - i,
									RES_CONTENT_TYPE ": %s\r\n", mt);

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
		}, &rdata);
	}

	if (_gv_ranges_.size())
		r = &_gv_ranges_;

	return r;
}
