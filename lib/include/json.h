#ifndef __JSON_H__
#define __JSON_H__

#include "context.h"

#define JSON_OK			0
#define JSON_PARSE_ERROR	-1
#define JSON_MEMORY_ERROR	-2

/* JSON value types */
#define JSON_STRING	1 /**< Represents a string value */
#define JSON_NUMBER	2 /**< Represents a numeric value */
#define JSON_OBJECT	3 /**< Represents an object structure */
#define JSON_ARRAY	4 /**< Represents an array of values */
#define JSON_TRUE	5 /**< Boolean true */
#define JSON_FALSE	6 /**< Boolean false */
#define JSON_NULL	7 /**< NULL value */

typedef struct json_string 	_json_string_t;
typedef struct json_value	_json_value_t;
typedef struct json_array	_json_array_t;
typedef struct json_string	_json_number_t;
typedef struct json_pair	_json_pair_t;
typedef struct json_object	_json_object_t;

typedef int _json_err_t; /*!< Return code type */

/**
 * Structure representing a JSON string.
 *
 * - size: The number of characters in the string
 * - data: Pointer to the actual string content
 */
struct json_string {
	unsigned int	size; /* data size in symbols */
	char		*data;
}__attribute__((packed));

/**
 * Structure representing a JSON array.
 *
 * - num: Number of elements in the array
 * - pp_values: Array of pointers to JSON values
 */
struct json_array {
	unsigned int	num; /* number of elements */
	_json_value_t	**pp_values;
}__attribute__((packed));

/**
 * Structure representing a JSON object.
 *
 * - num: Number of key-value pairs
 * - pp_pairs: Array of pointers to JSON pair structures
 */
struct json_object {
	unsigned int	num; /* number of pairs */
	_json_pair_t	**pp_pairs;
}__attribute__((packed));

/**
 * Represents a basic value in JSON.
 *
 * Contains the type (jvt code) and one of:
 * - string_t (JSON_STRING)
 * - array_t (JSON_ARRAY)
 * - object_t (JSON_OBJECT)
 * - number_t (JSON_NUMBER)
 */
struct json_value {
	unsigned char	jvt; /*!< JSON value type */
	union {
		_json_string_t	string;
		_json_array_t	array;
		_json_object_t	object;
		_json_number_t	number;
	};
}__attribute__((packed));

struct json_pair {
	_json_string_t	name;
	_json_value_t	value;
}__attribute__((packed));

/**
 * Structure containing the context information for parsing JSON.
 *
 * - p_htc: Pointer to hypertext context
 * - root: Root object containing parsed data
 * - err_pos: Error position in the input string
 * - udata: Additional user data buffer
 */
typedef struct {
	_ht_context_t	*p_htc; /* hypertext context */
	_json_object_t	root; /* root object */
	unsigned long	err_pos; /* error position */
	void		*udata;
}_json_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate memory for a new JSON context.
 *
 * @param mem_alloc Memory allocation function (e.g., malloc)
 * @param mem_free Memory deallocation function (e.g., free)
 * @param udata Additional user data buffer
 * @return Pointer to the created JSON context structure
 */
_json_context_t *json_create_context(_mem_alloc_t *, _mem_free_t *, void *udata);

/**
 * Reset the context.
 *
 * @param p_jcxt Pointer to JSON context struct
 */
void json_reset_context(_json_context_t *);

/**
 * Destroy a JSON context.
 *
 * @param p_jcxt Pointer to JSON context struct
 */
void json_destroy_context(_json_context_t *);

/**
 * Parse JSON content into the given context.
 *
 * @param p_jcxt Pointer to JSON context
 * @param p_content Pointer to const input string
 * @param content_size Size of the input string in bytes
 * @return Pointer to a JSON value structure or NULL if error occurs
 */
_json_err_t json_parse(_json_context_t *p_jcxt, /* JSON context */
			const unsigned char *p_content, /* text content */
			unsigned long content_size /* size of text content in bytes */
			);

/**
 * Select and return a value from an object based on a path.
 *
 * @param p_jcxt Pointer to JSON context
 * @param jpath The path to select within the object
 * @param p_start_point Starting point (can be NULL)
 * @return Pointer to the selected JSON value or NULL if error occurs
 */
_json_value_t *json_select(_json_context_t *p_jcxt,
			const char *jpath,
			_json_object_t *p_start_point /* Can be NULL */
			);

/**
 * Access an element from a JSON array.
 *
 * @param p_jarray Pointer to JSON array structure
 * @param index Index of the desired element (0-based)
 * @return Pointer to the value at the given index or NULL if out of bounds
 */
_json_value_t *json_array_element(_json_array_t *p_jarray, unsigned int index);

/**
 * Access a pair from a JSON object.
 *
 * @param p_jobj Pointer to JSON object structure
 * @param index Index of the desired pair (0-based)
 * @return Pointer to the pair at the given index or NULL if out of bounds
 */
_json_pair_t *json_object_pair(_json_object_t *p_jobj, unsigned int index);

_json_value_t *json_object_value(_json_object_t *p_jobj, unsigned int index);

/**
will return 0 for continue or -1 for break */
typedef int _cb_enum_pairs_t(_json_pair_t *, void *);
typedef int _cb_enum_values_t(_json_value_t *, void *);

void json_enum_pairs(_json_object_t *, _cb_enum_pairs_t *, void *);
void json_enum_values(_json_value_t *, _cb_enum_values_t *, void *);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
