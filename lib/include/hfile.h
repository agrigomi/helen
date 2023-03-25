#ifndef __HFILE_H__
#define __HFILE_H__

#include <fcntl.h>
#include "sha1.h"

#define HF_INVALID_OFFSET	0xffffffffffffffff
#define HF_MAX_PATH		512
#define HF_MAX_RECORD_SIZE	(64 * 1024)

/* File header */
typedef struct __attribute__((packed)) {
	unsigned char 	reserved[12];
	unsigned int	capacity;	/* hash table capacity */
	unsigned int	records;	/* number of used slots */
	unsigned int	collisions;	/* number of collisions */
	unsigned long	dused;		/* size of used data */
} _hf_hdr_t;

/* record header */
typedef struct __attribute__((packed)) {
	unsigned char	hash[SHA1HashSize]; /* key hash */
	unsigned long	next;		/* next record in chain (offset om data area) */
	unsigned int	size;		/* record size */
} _hf_rec_hdr_t;

/* hash file context */
typedef struct {
	char		path[HF_MAX_PATH];	/* Path to file */
	int		fd;		/* file descriptor */
	unsigned long	size;		/* total file size */
	_hf_hdr_t 	*hdr;		/* file content (mapping) */
	unsigned long	*htable;	/* pointer to first element in hash table */
	_hf_rec_hdr_t 	*first;		/* pointer to first data record */
} _hf_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
Create hash file.
path	- path to file
capacity- size of hash table
dataK	- size of data area in kilobytes
hash file remains open for read and write */
int hf_create(_hf_context_t *, const char *path, unsigned int capacity, unsigned int dataK);

/**
Open hash file.
path	- path to file
flags	- combination of O_RDWR and O_RDONLY
returns 0 for success or -1 for fail */
int hf_open(_hf_context_t *, const char *path, int flags);

/**
Close hash file */
void hf_close(_hf_context_t *);

/**
Add record to hash file.
key	- record identifier
sz_key	- size of record identifier
data	- pointer to record content
sz_data	- size of record content
returns pointer to record content in case of success, or NULL */
void *hf_add(_hf_context_t *, void *key, int sz_key, void *data, int sz_data);

/**
Search for record in hash file
key	- record identifier
sz_key	- size of record identifier
returns pointer to record or NULL  */
void *hf_get(_hf_context_t *, void *key, int sz_key);

/**
Append empty block to hash file
n	- size of empty block in kilobytes
returns 0 for siccess or -1 for fail */
int hf_append(_hf_context_t *, unsigned int n);

/**
Extend hash table (capacity * 2)
returns 0 for success or < 0 for fail */
int hf_extend(_hf_context_t *);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __HFILE_H__ */

