#include <string.h>
#include "fcfg.h"
#include "str.h"

#define MAX_CFG_LINE	4096

static int fcfg_parse_line(unsigned char *line, char **param, char **value, const char div, bool enable_comments) {
	int r = 0;
	int len = strlen((char *)line);

	if(strcasecmp((char *)line, EOF_SIGNATURE) == 0)
		r = EOF;
	else if (len > 2) {
		bool comment = (enable_comments) ? (line[0] == '#') : false;

		if(!comment) {
			char *p = (char *)line;
			char *v = NULL;
			int sz_param = 0, sz_value = 0;

			for (int i = 0; i < len; i++) {
				if (line[i] == div) {
					if (!sz_param) {
						sz_param = i;
						line[i] = 0;
					}
				} else {
					if (sz_param) {
						if (line[i] >= ' ') {
							if (!v)
								v = (char *)&line[i];
							sz_value++;
						} else {
							line[i] = 0;
							if (v)
								break;
						}
					}
				}
			}

			if (p && v) {
				str_trim(p);
				*param = p;
				str_trim(v);
				*value = v;
				r = 1;
			}
		}
	}

	return r;
}

_err_t fcfg_read(FILE *stream, _fcfg_map_t &map, const char div, bool enable_comments) {
	_err_t r = E_FAIL;
	unsigned char cfg_line[MAX_CFG_LINE] = "";

	while (!feof(stream)) {
		char *param = NULL;
		char *value = NULL;

		if(fgets((char *)cfg_line, sizeof(cfg_line), stream)) {
			int _r = fcfg_parse_line(cfg_line, &param, &value, div, enable_comments);
			if(_r == 1) {
				map.insert(_fcfg_pair_t(param, value));
				r = E_OK;
			} else if(_r == EOF)
				break;
		}
	}

	return r;
}

_err_t fcfg_read(const char *fname, _fcfg_map_t &map, const char div, bool enable_comments) {
	_err_t r = E_FAIL;
	FILE *f = fopen(fname, "r");

	if (f) {
		r = fcfg_read(f, map, div, enable_comments);
		fclose(f);
	}

	return r;
}

_err_t fcfg_write(FILE *stream, _fcfg_map_t &map, const char div) {
	_err_t r = E_OK;
	_fcfg_map_t::iterator it = map.begin();

	while(it != map.end()) {
		if(fprintf(stream, "%s%c	%s\n", (*it).first.c_str(), div, (*it).second.c_str()) < 0) {
			r = E_FAIL;
			break;
		}
		it++;
	}

	return r;
}

_err_t fcfg_write_eof(FILE *stream) {
	_err_t r = E_OK;

	if(fprintf(stream, "%s\n", EOF_SIGNATURE) < 0)
		r = E_FAIL;

	return r;
}

_err_t fcfg_write(const char *fname, _fcfg_map_t &map, const char div) {
	_err_t r = E_FAIL;
	FILE *f = fopen(fname, "w");

	if(f) {
		r = fcfg_write(f, map, div);
		fclose(f);
	}

	return r;
}
