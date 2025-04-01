#ifndef __HFILE_H__
#define __HFILE_H__

#include <fcntl.h>
#include "sha1.h"

#define HF_INVALID_OFFSET	0xffffffff
#define HF_MAX_PATH		1024
#define HF_MAX_CHUNK_SIZE	64 // in kilobytes
#define HF_MAX_RECORD_SIZE	(HF_MAX_CHUNK_SIZE * 1024)

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
	unsigned int	reserved;
	unsigned int	next;		/* next record in chain (offset om data area) */
	unsigned int	size;		/* record size */
} _hf_rec_hdr_t;

/* hash file context */
typedef struct {
	char		path[HF_MAX_PATH];	/* Path to file */
	int		fd;		/* file descriptor */
	unsigned long	size;		/* total file size */
	_hf_hdr_t 	*hdr;		/* file content (mapping) */
	unsigned int	*htable;	/* pointer to first element in hash table */
	_hf_rec_hdr_t 	*first;		/* pointer to first data record */
} _hf_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
\brief Create hash file.
\par \b Parameters:
\n\t- \b path: Path to file
\n\t- \b capacity: Size of hash table
\n\t- \b dataK: Size of data area in kilobytes

\brief \b Returns: 0 for success, -1 for failure.

@exception \c EILIB file operation errors */
int hf_create(_hf_context_t *, const char *path, unsigned int capacity, unsigned int dataK);

/**
\brief Open hash file.
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters
\n\t- \b path: Path to file
\n\t- \b flags: Combination of O_RDWR and O_RDONLY

\brief \b Returns: 0 for success, -1 for failure.

@exception \c EILIB file operation errors */
int hf_open(_hf_context_t *, const char *path, int flags);

/**
\brief Close hash file.
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters
\brief \b Returns: 0 for success, -1 for failure.

@exception \c EILIB file operation errors */
void hf_close(_hf_context_t *);

/**
\brief Add record to hash file.
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters
\n\t- \b key: Record identifier
\n\t- \b sz_key: Size of record identifier
\n\t- \b data: Pointer to record content
\n\t- \b sz_data: Size of record content

\brief \b Returns: Pointer to record content in case of success, NULL otherwise.

@exception \c EILIB file operation errors */
void *hf_add(_hf_context_t *, const void *key, int sz_key, void *data, unsigned int sz_data);

/**
\brief Search for record in hash file
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters
\n\t- \b key: Record identifier
\n\t- \b sz_key: Size of record identifier
\n\t- \b sz_data: [out] Size of record content (filled by the function)

\brief \b Returns: Pointer to record or NULL.

@exception \c EILIB file operation errors */
void *hf_get(_hf_context_t *, const void *key, int sz_key, unsigned int *sz_data);

/**
\brief Append empty block to hash file.
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters
\n\t- \b size: Number of bytes in each block

\brief \b Returns: 0 for success, -1 for failure.

@exception \c EILIB file operation errors */
int hf_append(_hf_context_t *, unsigned int n);

/**
\brief Return the file header.
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters

\brief \b Returns: Pointer to file header structure.

@exception \c EILIB file operation errors */
_hf_hdr_t *hf_header(_hf_context_t *);

/**
\brief Enumerate all keys in hash table.
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters
\n\t- \b callback: Callback function for enumerating each key

\brief \b Returns: 0 for success or -1 for error.

@exception \c EILIB file operation errors */
int hf_enum(_hf_context_t *, int (*pcb)(void *rec_ptr, unsigned int size,void *udata), void *udata);

/**
\brief Extend hash table.
\par \b Parameters:
\n\t- \b context: Structure containing file header and other parameters
\n\t- \b size: New size of the hash table

\brief \b Returns: 0 for success, -1 for failure.

@exception \c EILIB file operation errors */
int hf_extend(_hf_context_t *, unsigned int new_capacity);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __HFILE_H__ */

