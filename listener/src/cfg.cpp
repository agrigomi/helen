#include <vector>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include "cfg.h"
#include "config.h"
#include "json.h"
#include "trace.h"

static std::vector<_listen_t> _gv_listen;

static _u8 *map_file(_cstr_t fname, int *fd, _u64 *size) {
	_u8 *r = NULL;
	int _fd = open(fname, O_RDONLY);
	_u64 _size = 0;

	if(_fd > 0) {
		_size = lseek(_fd, 0, SEEK_END);
		lseek(_fd, 0, SEEK_SET);
		if((r = (_u8 *)mmap(NULL, _fd, PROT_READ, MAP_SHARED, _fd, 0))) {
			*fd = _fd;
			*size = _size;
		} else
			close(_fd);
	}

	return r;
}

void cfg_enum_listen(void (*pcb)(_listen_t *)) {
	std::vector<_listen_t>::iterator it = _gv_listen.begin();

	while(it != _gv_listen.end()) {
		pcb(&(*it));
		it++;
	}
}

static void init_config(void) {
	cfg_enum_listen([](_listen_t *p) {
		int i = 0;

		for(i = 0; i < MAX_ARGV && p->argv[i]; i++)
			free((void *)p->argv[i]);

		for(i = 0; i < MAX_ENV && p->env[i]; i++)
			free((void *)p->env[i]);

	});

	_gv_listen.clear();
}

static void split_by_space(_cstr_t str, _u32 str_size, _cstr_t dst_arr[], _u32 arr_size) {
	//...
}

static void parse_env(_json_object_t *pjo_env, _cstr_t sdt_arr[], _u32 arr_size) {
	//...
}

static void add_listener(_json_context_t *p_jcxt, _json_pair_t *p_jp) {
	_listen_t l;
	_json_value_t *pjv_port = json_select(p_jcxt, "port", &(p_jp->value.object));
	_json_value_t *pjv_exec = json_select(p_jcxt, "exec", &(p_jp->value.object));
	_json_value_t *pjv_env  = json_select(p_jcxt, "env",  &(p_jp->value.object));

	memset(&l, 0, sizeof(_listen_t));
	strncpy(l.name, p_jp->name.data, (p_jp->name.size < sizeof(l.name)) ? p_jp->name.size : sizeof(l.name) - 1);

	if(pjv_port && pjv_port->jvt == JSON_STRING &&  pjv_exec && pjv_exec->jvt == JSON_STRING) {
		l.port = atoi(pjv_port->string.data);
		split_by_space(pjv_exec->string.data, pjv_exec->string.size, l.argv, MAX_ARGV);

		if(pjv_env && pjv_env->jvt == JSON_OBJECT)
			parse_env(&pjv_env->object, l.env, MAX_ENV);

		_gv_listen.push_back(l);
	}
}

_err_t cfg_load(_cstr_t fname) {
	_err_t r = E_FAIL;
	int cfg_fd = -1;
	_u64 cfg_size = 0;
	_u8 *content = map_file(fname, &cfg_fd, &cfg_size);

	init_config();

	if(content) {
		_json_context_t *p_jcxt = json_create_context(
						[](_u32 size, __attribute__((unused)) void *udata) ->void* {
							return malloc(size);
						},
						[](void *ptr, __attribute__((unused)) _u32 size,
								__attribute__((unused)) void *udata) {
							free(ptr);
						}, NULL);
		_json_err_t jerr = json_parse(p_jcxt, content, cfg_size);

		if(jerr == JSON_OK) {
			_json_value_t *p_jv = json_select(p_jcxt, "listen", NULL);

			if(p_jv && p_jv->jvt == JSON_OBJECT) {
				_json_pair_t *p_jp = NULL;
				_u32 idx = 0;

				while((p_jp = json_object_pair(&p_jv->object, idx))) {
					add_listener(p_jcxt, p_jp);
					idx++;
				}
			}
		} else {
			TRACEfl("Failed to parse JSON file '%s'\n", fname);
		}

		json_destroy_context(p_jcxt);
		munmap(content, cfg_size);
		close(cfg_fd);
	}

	return r;
}


