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

void str_trim_left(char *str) {
	unsigned i,l;

	l = strlen(str);

	for (i = 0; i < l; i++) {
		if (str[i] != ' ' && str[i] != '\t')
			break;
	}

	if (i == 0)
		return;

	memmove(str, (str + i), (strlen(str + i) + 1));
}

void str_trim_right(char *str) {
	unsigned int i,l;

	l = strlen(str);
	if (l) {
		i=l-1;

		while (str[i] == ' ' || str[i] == '\t') {
			str[i]= 0;
			i--;
		}
	}
}

void str_trim(char *str) {
	str_trim_right(str);
	str_trim_left(str);
}

char *str_toupper(char *s) {
	unsigned int i = 0;

	while (s[i]) {
		s[i] = (s[i] - 32 * (s[i] >= 'a' && s[i] <= 'z'));
		i++;
	}

	return s;
}

