#include <stdio.h>
#include <map>
#include <string>
#include "http.h"

static std::map<std::string, std::string> _g_hdr_map_;

void hdr_init(void) {
	_g_hdr_map_.clear();

	time_t _time = time(NULL);
	struct tm *_tm = gmtime(&_time);
	_char_t date[32];

	strftime(date, sizeof(date),
		 "%a, %d %b %Y %H:%M:%S GMT", _tm);

	_g_hdr_map_[RES_DATE] = date;
	_g_hdr_map_[RES_SERVER] = SERVER_NAME;
	_g_hdr_map_[RES_ACCEPT_RANGES] = "Bytes";
}

void hdr_set(_cstr_t var, _cstr_t val) {
	_g_hdr_map_[var] = val;
}

void hdr_set(_cstr_t var, int val) {
	_g_hdr_map_[var] = std::to_string(val);
}

void hdr_clear(void) {
	_g_hdr_map_.clear();
}

/* Returns size of header in bytes */
int hdr_export(_str_t hb, int sz) {
	int r = 0;
	std::map<std::string, std::string>::iterator it = _g_hdr_map_.begin();

	while (it != _g_hdr_map_.end()) {
		r += snprintf(hb + r, sz - r, "%s: %s\r\n", (*it).first.c_str(), (*it).second.c_str());
		it++;
	}

	return r;
}
