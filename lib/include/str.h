#ifndef __STR_H__
#define __STR_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
Fills destination buffer with replaced $(SOMETHING)
env. variables.*/
int str_resolve(const char *src, char *dst, int sz_dst);

/**
Split string separated by div character
str:	[in] Source string
div:	[in] Delimiter
pcb:	[in] Callback function
	idx:	[in] Token index
	str:	[in] Token (substring)
	udata:	[in] Pointer to user data
	Return:	0 - Continue / -1 - break
udata:	[in] Pointer to user data */
void str_split(char *str, const char *div, int (*pcb)(int idx, char *str, void *udata), void *udata);

/**
Divide string by string delimiter
Split string separated by div character
str:	[in] Source string
div:	[in] Delimiter
left:	[out] left part of string
right:	[out] Right part (NULL if divider not found)*/
void str_div_s(char *str, const char *div, char **left, char **right);
void str_trim_left(char *str);
void str_trim_right(char *str);
void str_trim(char *str);
char *str_toupper(char *s);

#ifdef __cplusplus
}
#endif


#endif
