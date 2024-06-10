#ifndef __RESP_H__
#define __RESP_H__

#define BUFFER_TYPE_HEAP	1
#define BUFFER_TYPE_STATIC	2

typedef struct {
	bool enabled;
	int size;
	_str_t buffer;
	int buffer_size;
	_u8 buffer_type;
	//...
} _resp_header_t;

typedef struct {
	int size;
	_u8 encoding;
	_str_t buffer;
	int buffer_size;
	_u8 buffer_type;
} _resp_content_t;

typedef struct {
	unsigned long begin; // start offset in content
	unsigned long end; // size in bytes
	_char_t	header[512]; // range header
} _range_t;

typedef std::vector<_range_t> _v_range_t;

typedef struct {
	_resp_header_t	header;
	_resp_content_t	content;
	_char_t		file[MAX_PATH]; // resolved (real) path
	_vhost_t	*p_vhost;
	_v_range_t	*pv_ranges;
	_char_t		boundary[MAX_BOUNDARY];
	_mapping_t	*p_mapping;
	//...
} _response_t;

// Response
_err_t res_processing(void);
_err_t send_error_response(_vhost_t *p_vhost, int rc);
_err_t do_connect(_cstr_t method, _cstr_t scheme, _cstr_t domain, _cstr_t port, _cstr_t uri, _cstr_t proto);



#endif

