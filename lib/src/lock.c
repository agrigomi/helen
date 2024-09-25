#include <sys/file.h>
#include "lock.h"


int lock_fd(int fd) {
	return flock(fd, LOCK_EX);
}

int lock(const char *fname) {
	int r = open(fname, O_CREAT);

	if (r > 0)
		flock(r, LOCK_EX);

	return r;
}

void unlock(int fd_lock) {
	flock(fd_lock, LOCK_UN);
}

