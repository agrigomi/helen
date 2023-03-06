#ifndef __SIG_H__
#define __SIG_H__

#include <signal.h>

// signal handling
typedef void _sig_action_t(int signum, siginfo_t *info, void *);

#ifdef __cplusplus
extern "C" {
#endif
void handle(int sig, _sig_action_t *pf_action);
void dump_stack(void);
#ifdef __cplusplus
}
#endif

#endif

