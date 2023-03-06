#ifndef __DTYPE_H__
#define __DTYPE_H__

typedef unsigned long long	_u64;
typedef signed long long	_s64;
typedef unsigned int		_u32;
typedef signed int		_s32;
typedef unsigned short		_u16;
typedef short			_s16;
typedef unsigned char		_u8;
typedef char			_s8;
typedef char*			_str_t;
typedef char			_char_t;
typedef unsigned char		_uchar_t;
typedef const char* 		_cstr_t;
typedef unsigned long		_ulong;
typedef signed long		_slong;
typedef unsigned int		_bool;
typedef unsigned short		WORD;
typedef unsigned long		DWORD;
typedef unsigned char		BYTE;
typedef unsigned char		BOOLEAN;

// APP - added for compatibility
#ifndef TRUE
#define TRUE	0x01
#endif
#ifndef FALSE
#define FALSE	0
#endif
#ifndef MIN
#define MIN(x,y) (((x) > (y)) ? (y):(x))
#endif
#ifndef MAX
#define MAX(x,y) (((x) < (y)) ? (y):(x))
#endif
#ifndef _false
#define _false	(_bool)0
#endif
#ifndef _true
#define _true	(_bool)1
#endif
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

typedef union {
	_u64	_qw;	/* quad word */
	struct {
 		_u32	_ldw;	/* lo dword */
		_u32	_hdw;	/* hi dword */
	};
}__attribute__((packed)) _u64_t;

#ifndef va_list
#define va_list		__builtin_va_list
#endif
#ifndef va_start
#define va_start(v,l)	__builtin_va_start(v,l)
#endif
#ifndef va_end
#define va_end(v)	__builtin_va_end(v)
#endif
#ifndef va_arg
#define va_arg(v,l)	__builtin_va_arg(v,l)
#endif

#endif
