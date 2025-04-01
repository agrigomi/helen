#ifndef __HT_CONTEXT_H__
#define __HT_CONTEXT_H__

/* Machine order constants */
#define MACHINE_ORDER_LE	1 /* Use little-endian byte order */
#define MACHINE_ORDER_BE	2 /* Use big-endian byte order */

/* Encoding and content type definitions */
#define E_UTF_8		0 /* Extended ASCII encoding */
#define E_UTF_16_BE	1 /* 16-bit big-endian encoding */
#define E_UTF_16_LE	2 /* 16-bit little-endian encoding */
#define E_UTF_32_BE	3 /* 32-bit big-endian encoding */
#define E_UTF_32_LE	4 /* 32-bit little-endian encoding */

/* Hypertext content information structure */
/**
 * @brief Structure containing hypertext content details.
 */
typedef struct { /* hypertext content info. */
	unsigned char	*p_content; 	/* Pointer to hypertext content data */
	unsigned long	sz_content; 	/* ize of the content in bytes */
	unsigned long	c_pos; 		/* Current position within the content */
	unsigned char	encoding; 	/* Encoding type used for the content */
	unsigned char	machine_order;	/* Machine byte order setting */
} _ht_content_t;

typedef void *_mem_alloc_t(unsigned int nbytes, void *udata);
typedef void _mem_free_t(void *ptr, unsigned int nbytes, void *udata);
typedef unsigned int _read_t(_ht_content_t *p_ht_content, unsigned long *pos);

/**
 * @brief Structure defining a hypertext context.
 */
typedef struct { /* context of hypertext parser */
	_ht_content_t	ht_content;	/* Hypertext content information. */
	_mem_alloc_t	*pf_mem_alloc;	/* Pointer to memory allocation function */
	_mem_free_t	*pf_mem_free;	/* Pointer to memory deallocation function */
	_read_t		*pf_read;	/* Pointer to the read operation implementation */
	void		*udata;		/* Additional user data */
} _ht_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate and return a new hypertext context.
 * @param mem_alloc Pointer to the memory allocation function
 * @param mem_free Pointer to the memory deallocation function
 * @param read Implementation of the read operation
 */
_ht_context_t *ht_create_context(_mem_alloc_t *, _mem_free_t *, void *);

/**
 * @brief Initialize a context with hypertext content and its size.
 * @param ctx Context pointer to initialize
 * @param content Content structure
 * @param len Length of content in bytes
 */
void ht_init_context(_ht_context_t *, void *, unsigned long);

/**
 * @brief Reset the current position within the hypertext context.
 * @param ctx Context pointer to reset
 */
void ht_reset_context(_ht_context_t *);

/**
 * @brief Get the current character index of the cursor.
 * @param ctx Context pointer
 */
unsigned long ht_position(_ht_context_t *);

/**
 * @brief Get the memory address at a given position in the context.
 * @param ctx Context pointer
 * @param pos Position within the content
 */
unsigned char *ht_ptr(_ht_context_t *);

/**
 * @brief Compare two memory addresses, considering document encoding differences.
 * @param base Base address to compare from
 * @param a Address of first element to compare
 * @param b Address of second element to compare
 * @param len Number of bytes to compare
 */
int ht_compare(_ht_context_t *, unsigned char *, unsigned char *, unsigned int);

/**
 * @brief Count the number of symbols between two memory addresses.
 * @param ctx Context pointer
 * @param base Base address
 * @param end Target address
 * @param length Number of bytes to consider
 */
unsigned int ht_symbols(_ht_context_t *, unsigned char *, unsigned char *);

/**
 * @brief Get the size in bytes for a given number of symbols.
 * @param ctx Context pointer
 * @param num_symbols Number of symbols (bytes) requested
 */
unsigned int ht_bytes(_ht_context_t *, unsigned int num_symbols);

/* read symbol */
/**
 * @brief Read one or more characters from memory into a buffer.
 * @param ctx Context pointer
 * @param input_ptr Input address to read from
 * @param output_buffer Buffer where the read characters are stored
 */
unsigned int ht_read(_ht_context_t *, /* context */
			unsigned char *, /* input pointer */
			unsigned char ** /* output pointer (to next symbol) */
			);

/**
 * @brief Destroy (deallocate) the hypertext context.
 * @param ctx Pointer to the context to destroy
 */
void ht_destroy_context(_ht_context_t *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
