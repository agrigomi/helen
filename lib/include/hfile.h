#ifndef __HFILE_H__
#define __HFILE_H__

#include "sha1.h"

#define HF_INVALID_OFFSET	0xffffffffffffffff

/* File header */
typedef struct __attribute__((packed)) {
	unsigned int	capacity;	/* hash table capacity */
	unsigned int	records;	/* number of used slots */
	unsigned int	collisions;	/* number of collisions */
	unsigned long	dsize;		/* size of data area */
} _hf_hdr_t;

/* record header */
typedef struct __attribute__((packed)) {
	unsigned char	hash[SHA1HashSize]; /* key hash */
	unsigned long	next;		/* next record in chain (offset om data area) */
	unsigned int	size;		/* record size */
} _hf_rec_hdr_t;

/* hash file context */
typedef struct {
	int		fd;		/* file descriptor */
	unsigned long	size;		/* total file size */
	_hf_hdr_t 	*hdr;		/* file content (mapping) */
	unsigned long	*htable;	/* pointer to first element in hash table */
	_hf_rec_hdr_t 	*first;		/* pointer to first data record */
	_hf_rec_hdr_t	*next;		/* pointer to next record */
} _hf_context_t;


#endif

