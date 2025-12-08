#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <signal.h>
#include "json.h"

unsigned char *open_fmap(const char *fname, int *fd, unsigned long *sz) {
	unsigned char *r = NULL;
	int _fd = open(fname, O_RDONLY);
	unsigned long _sz = 0;

	if (_fd > 0) {
		_sz = lseek(_fd, 0, SEEK_END);
		lseek(_fd, 0, SEEK_SET);
		r = mmap(NULL, _sz, PROT_READ, MAP_SHARED, _fd, 0);
	}

	if (r) {
		*fd = _fd;
		*sz = _sz;
	} else
		close(_fd);

	return r;
}

static void *mem_alloc(unsigned int sz, void __attribute__((unused)) *udata) {
	return malloc(sz);
}

static void mem_free(void *ptr, unsigned int __attribute__((unused)) size,
			void __attribute__((unused)) *udata) {
	free(ptr);
}

static void print_value(_json_value_t *p_jv);

static void print_object(_json_object_t *p_jobj) {
	unsigned int i = 0;

	printf("{\n");
	for (; i < p_jobj->num; i++) {
		_json_pair_t *p_jpair = p_jobj->pp_pairs[i];

		if (p_jpair) {
			printf("\"");
			fwrite(p_jpair->name.data, p_jpair->name.size, 1, stdout);
			printf("\": ");
			print_value(&p_jpair->value);
			printf(",\n");
		}
	}
	printf("}");
}

static void print_string(_json_string_t *p_jstr) {
	printf("\"");
	fwrite(p_jstr->data, p_jstr->size, 1, stdout);
	printf("\"");
}

static void print_number(_json_number_t *p_jnum) {
	fwrite(p_jnum->data, p_jnum->size, 1, stdout);
}

static void print_array(_json_array_t *p_jarray) {
	unsigned int i = 0;

	printf("[\n");
	for (; i < p_jarray->num; i++) {
		_json_value_t *p_jv = p_jarray->pp_values[i];

		if (p_jv) {
			print_value(p_jv);
			printf(", ");
		}
	}
	printf("]");
}

static void print_const(_json_value_t *p_jv) {
	switch (p_jv->jvt) {
		case JSON_NULL:
			printf("null");
			break;
		case JSON_TRUE:
			printf("true");
			break;
		case JSON_FALSE:
			printf("false");
			break;
	}
}

static void print_value(_json_value_t *p_jv) {
	switch (p_jv->jvt) {
		case JSON_STRING:
			print_string(&p_jv->string);
			break;
		case JSON_NUMBER:
			print_number(&p_jv->number);
			break;
		case JSON_OBJECT:
			print_object(&p_jv->object);
			break;
		case JSON_ARRAY:
			print_array(&p_jv->array);
			break;
		case JSON_TRUE:
		case JSON_FALSE:
		case JSON_NULL:
			print_const(p_jv);
			break;
	}
}

static int _g_fd_ = -1;
static unsigned char *_g_content_ = NULL;
static unsigned long _g_size_ = 0;
static _json_context_t *_gp_jcxt_ = NULL;

void quit(int sig) {
	fprintf(stderr, "Exit because of signal #%d\n", sig);

	if (_gp_jcxt_)
		json_destroy_context(_gp_jcxt_);
	if (_g_content_)
		munmap(_g_content_, _g_size_);
	if (_g_fd_ > 0)
		close(_g_fd_);

	exit(0);
}

int main(int argc, char *argv[]) {
	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	signal(SIGKILL, quit);

	if (argc > 1) {
		_g_content_ = open_fmap(argv[1], &_g_fd_, &_g_size_);

		if (_g_content_) {
			_gp_jcxt_ = json_create_context(mem_alloc, mem_free, NULL);
			_json_err_t jerr = json_parse(_gp_jcxt_, _g_content_, _g_size_);

			if (jerr != JSON_OK) {
				char *pc = (char *)_gp_jcxt_->p_htc->ht_content.p_content;
				int i = 0;

				fprintf(stderr, "ERROR: (offset: %lu) ", _gp_jcxt_->err_pos);
				for (; i < 10 && *(pc + _gp_jcxt_->err_pos + i); i++)
					printf("%c", *(pc + _gp_jcxt_->err_pos + i));
				printf("\n");

				return -1;
			} else {
				char jpath[256];

_again_:
				fprintf(stderr, "jpath: ");

				fgets(jpath, sizeof(jpath), stdin);
				_json_value_t *p_jv = json_select(_gp_jcxt_, jpath, NULL);

				if (p_jv) {
					print_value(p_jv);
					printf("\n");
				} else
					fprintf(stdout, "NULL\n");

				goto _again_;
			}

			json_destroy_context(_gp_jcxt_);
			munmap(_g_content_, _g_size_);
			close(_g_fd_);
		}
	}

	return 0;
}
