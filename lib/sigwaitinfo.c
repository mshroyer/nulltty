/**
 * sigwaitinfo() stub for OS X
 *
 * Reimplements a simple version of sigwaitinfo() ignoring its info
 * parameter, on top of OS X's sigwait().
 */

#include <signal.h>

int sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
    int sig = -1;

    if ( sigwait(set, &sig) < 0 )
        return -1;

    return sig;
}
