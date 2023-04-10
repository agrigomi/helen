#include <string.h>
#include <stdlib.h>
#include "str.h"

/**
Fills destination buffer with replaced $(SOMETHING)
env. variables.*/
int str_resolve(const char *src, char *dst, int sz_dst) {
	char lb[256];
	unsigned int idst = 0, isrc = 0, ilb=0;
	unsigned int lsrc = strlen(src), szdst = sz_dst - 1;
	char *eval = NULL;

	memset(dst, 0, sz_dst);

	for (; isrc < lsrc && idst < szdst; isrc++) {
		if (*(src + isrc) == '$' && *(src + isrc + 1) == '(') {
			isrc += 2;

			memset(lb, 0, sizeof(lb));
			for (ilb = 0; ilb < sizeof(lb) && isrc < lsrc; ilb++, isrc++) {
				if (*(src + isrc) == ')') {
					if ((eval = getenv(lb))) {
						int leval = strlen(eval);
						int rem = szdst - idst;
						int sz = (leval < rem) ? leval : rem;

						strncpy((dst + idst), eval, sz);
						idst += sz;
						break;
					}
				} else
					*(lb + ilb) = *(src + isrc);
			}
		} else {
			*(dst + idst) =  *(src + isrc);
			idst++;
		}
	}

	return idst;
}

