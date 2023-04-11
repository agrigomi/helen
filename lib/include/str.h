#ifndef __STR_H__
#define __STR_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
Fills destination buffer with replaced $(SOMETHING)
env. variables.*/
int str_resolve(const char *src, char *dst, int sz_dst);


void str_trim_left(char *str);
void str_trim_right(char *str);
void str_trim(char *str);
char *str_toupper(char *s);

#ifdef __cplusplus
}
#endif


#endif
