#include <execinfo.h>
#include <malloc.h>
#include "unistd.h"
#include "trace.h"
#include "sig.h"
#include "config.h"

#define BT_BUF_SIZE 100

void dump_stack(void) {
#ifdef _TRACE_
        int j, nptrs;
        void *buffer[BT_BUF_SIZE];
        char **strings;

        nptrs = backtrace(buffer, BT_BUF_SIZE);

        /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
        would produce similar output to the following: */

        if((strings = backtrace_symbols(buffer, nptrs))) {
                for (j = 0; j < nptrs; j++) {
                        TRACE("%s\n", strings[j]);
		}

                free(strings);
        }
#endif
}

static void signal_action(int signum, siginfo_t *info,
			__attribute__((unused)) void *arg) {
        dump_stack();

        // info->si_addr holds the dereferenced pointer address
        if (info->si_addr == NULL) {
                // This will be thrown at the point in the code
                // where the exception was caused.
                TRACE("signal %d\n", signum);
        } else {
                // Now restore default behaviour for this signal,
                // and send signal to self.
                signal(signum, SIG_DFL);
                kill(getpid(), signum);
        }
}

void handle(int sig, _sig_action_t *pf_action) {
        struct sigaction act; // Signal structure

        act.sa_sigaction = (pf_action) ? pf_action : signal_action; // Set the action to our function.
        sigemptyset(&act.sa_mask); // Initialise signal set.
        act.sa_flags = SA_SIGINFO|SA_NODEFER; // Our handler takes 3 par.
        sigaction(sig, &act, NULL);
}

