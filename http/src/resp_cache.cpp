#include <stdio.h>
#include <sys/stat.h>
#include "http.h"

static void create_folders(_cstr_t path) {
	_char_t lb[MAX_PATH];
	int enc = SUPPORTED_ENCODING;
	int uenc = 0;
	_cstr_t fldr = NULL;

	mkdir(path, 0755);

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

