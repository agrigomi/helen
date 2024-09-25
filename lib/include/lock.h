#ifndef __LOCK_H__
#define __LOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* returns 0 for success */
int lock_fd(int fd);
/* Create locked file
returns file descriptor */
int lock(const char *fname);
void unlock(int fd_lock);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
