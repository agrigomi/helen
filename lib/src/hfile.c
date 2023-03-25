#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "hfile.h"

static unsigned char *map_file(const char *fname, int flags, int *fd, unsigned long *size) {
	unsigned char *r = NULL;
	int _fd = open(fname, flags);
	unsigned long _size = 0;
	int map_flags = PROT_READ;

	if (flags & O_RDWR)
		map_flags |= PROT_WRITE;

	if (_fd > 0) {
		_size = lseek(_fd, 0, SEEK_END);
		lseek(_fd, 0, SEEK_SET);
		if ((r = (unsigned char *)mmap(NULL, _size, map_flags, MAP_SHARED, _fd, 0))) {
			*fd = _fd;
			*size = _size;
		} else
			close(_fd);
	}

	return r;
}

static int open_context(_hf_context_t *p_cxt, const char *path, int flags) {
	int r = -1;

	if ((p_cxt->hdr = (_hf_hdr_t *)map_file(path, flags, &p_cxt->fd, &p_cxt->size))) {
		strncpy(p_cxt->path, path, sizeof(p_cxt->path) - 1);

		/* pointer to hash table */
		p_cxt->htable = (unsigned long *)(p_cxt->hdr + 1);

		/* pointer to first record */
		p_cxt->first = (_hf_rec_hdr_t *)(((unsigned char *)p_cxt->hdr) +
				/* behind file header */
				 sizeof(_hf_hdr_t) +
				 /* behind hash table */
				 (p_cxt->hdr->capacity * sizeof(unsigned long)));
		r = 0;
	}

	return r;
}

/**
Create hash file.
path	- path to file
capacity- size of hash table
dataK	- size of data area in kilobytes
hash file remains open for read and write */
int hf_create(_hf_context_t *p_cxt, const char *path, unsigned int capacity, unsigned int dataK) {
	int r = -1;
	int fd = -1;

	if (strlen(path) < sizeof(p_cxt->path)) {
		if ((fd = open(path, O_CREAT | O_WRONLY | O_TRUNC,
				S_IRUSR | S_IWUSR |
				S_IRGRP | S_IWGRP |
				S_IROTH | S_IWOTH |
				S_ISUID
				)) > 0) {
			unsigned long idx = HF_INVALID_OFFSET;
			_hf_hdr_t hdr;
			unsigned char data[1024];
			unsigned int nc = 0;

			memset(data, 0, sizeof(data));
			memset(&hdr, 0, sizeof(hdr));

			hdr.capacity = capacity;

			/* write header */
			write(fd, &hdr, sizeof(hdr));

			/* write empty hash table */
			while (nc < capacity) {
				write(fd, &idx, sizeof(idx));
				nc++;
			}

			/* write empty data area */
			nc = 0;
			while (nc < dataK) {
				write(fd, data, sizeof(data));
				nc++;
			}

			close(fd);

			/* reopen for read and write */
			r = open_context(p_cxt, path, O_RDWR);
		}
	}

	return r;
}

/**
Open hash file.
path	- path to file
flags	- O_RDONLY or O_RDWR ... see open (man 2 open)
returns 0 for success or -1 for fail */
int hf_open(_hf_context_t *p_cxt, const char *path, int flags) {
	return open_context(p_cxt, path, flags);
}

/**
Close hash file */
void hf_close(_hf_context_t *p_cxt) {
	if (p_cxt->fd > 0 && p_cxt->hdr) {
		munmap((void *)p_cxt->hdr, p_cxt->size);
		close(p_cxt->fd);
		p_cxt->fd = -1;
		p_cxt->hdr = NULL;
		p_cxt->htable = NULL;
		p_cxt->size = 0;
		p_cxt->first = NULL;
	}
}

/* make long integer from string */
static unsigned long hash(const unsigned char *key, unsigned int sz) {
	unsigned long r = 5381;
	unsigned int c, n=0;

	while (n < sz) {
		c = key[n];
		r = ((r << 5) + r) + c; /* hash * 33 + c */
		n++;
	}

	return r;
}

static unsigned long key_to_index(const unsigned char *key, int sz_key,
			unsigned int capacity, unsigned char *sha1_buffer) {
	SHA1Context sha1_cxt;

	SHA1Reset(&sha1_cxt);
	SHA1Input(&sha1_cxt, key, sz_key);
	SHA1Result(&sha1_cxt, sha1_buffer);

	return hash(sha1_buffer, SHA1HashSize) % capacity;
}

/* returns offset in data area
Writes only record header + data content.
Do not changes context and file header */
static unsigned long write_record(_hf_context_t *p_cxt, _hf_rec_hdr_t *p_rh,
				void *data, int sz_data) {
	unsigned long r = HF_INVALID_OFFSET;
	unsigned long free_space = p_cxt->size -
				sizeof(_hf_hdr_t) -
				(p_cxt->hdr->capacity * sizeof(unsigned long)) -
				p_cxt->hdr->dused;

	if (free_space > (sz_data + sizeof(_hf_rec_hdr_t))) {
		void *ptr_rh = (unsigned char *)p_cxt->first + p_cxt->hdr->dused;
		void *ptr_data = ptr_rh + sizeof(_hf_rec_hdr_t);

		r = p_cxt->hdr->dused;
		memcpy(ptr_rh, p_rh, sizeof(_hf_rec_hdr_t));
		memcpy(ptr_data, data, sz_data);
	}

	return r;
}

/**
Add record to hash file.
key	- record identifier
sz_key	- size of record identifier
data	- pointer to record content
sz_data	- size of record content
returns pointer to record content in case of success, or NULL */
void *hf_add(_hf_context_t *p_cxt, void *key, int sz_key, void *data, int sz_data) {
	void *r = NULL;
	_hf_rec_hdr_t rh;
	unsigned long idx;
	unsigned long off;

	memset(&rh, 0, sizeof(_hf_rec_hdr_t));
	rh.next = HF_INVALID_OFFSET;
	rh.size = sz_data;

	if (p_cxt->fd > 0 && p_cxt->hdr) {
		/* produces an index in hash table */
		idx = key_to_index(key, sz_key, p_cxt->hdr->capacity, rh.hash);
_fit_record_:
		/* offset in data area for newly added record */
		off = write_record(p_cxt, &rh, data, sz_data);

		if (off != HF_INVALID_OFFSET) {
			if (p_cxt->htable[idx] == HF_INVALID_OFFSET)
				/* OK, no collision */
				p_cxt->htable[idx] = off;
			else {
				/* COLLISION !!! chain record */
				//...
			}

			/* pointer to record content in data area */
			r = (void *)p_cxt->first + p_cxt->hdr->dused + sizeof(_hf_rec_hdr_t);

			/* updates the amount of data area used */
			p_cxt->hdr->dused += sizeof(_hf_rec_hdr_t) + sz_data;
			p_cxt->hdr->records++;
		} else {
			/* append empty block to file */
			if (hf_append(p_cxt, 64) == 0)
				goto _fit_record_;
		}
	}

	return r;
}

/**
Append empty block to hash file
n	- size of empty block in kilobytes
returns 0 for siccess or -1 for fail */
int hf_append(_hf_context_t *p_cxt, unsigned int n) {
	int r = -1;
	unsigned char data[1024];
	int _fd;

	memset(data, 0, sizeof(data));
	hf_close(p_cxt);
	_fd = open(p_cxt->path, O_WRONLY);

	if (_fd > 0) {
		unsigned int i = 0;

		lseek(_fd, 0, SEEK_END);
		for (; i < n; i++)
			write(_fd, data, sizeof(data));

		close(_fd);
		r = open_context(p_cxt, p_cxt->path, O_RDWR);
	}

	return r;
}
