#include <stubs.h>

#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "nulltty_child.h"

/** Keep GCC 4.6's -Wunused-result happy */
#define IGNORE_RESULT(x) do { (void) sizeof(x); } while ( 0 )

static void sigchld_handler(int signum)
{
    const char msg[] = "nulltty child exited unexpectedly, terminating\n";
    int stat_loc;

    wait(&stat_loc);
    IGNORE_RESULT(write(2, msg, sizeof(msg)-1));
    _exit(1);
}

int nulltty_child(const char *pty_a, const char *pty_b)
{
    struct sigaction action;
    sigset_t new_mask, prev_mask, wait_set;
    int wait_result, signum, pid;

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);

    action.sa_handler = sigchld_handler;
    if ( sigaction(SIGCHLD, &action, NULL) < 0 )
        return -1;

    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGUSR1);
    if ( sigprocmask(SIG_BLOCK, &new_mask, &prev_mask) < 0 )
        return -1;

    sigemptyset(&wait_set);
    sigaddset(&wait_set, SIGUSR1);
    sigaddset(&wait_set, SIGINT);

    switch ( pid = fork() ) {
    case -1:
        return -1;

    case 0:
        execl(NULLTTY, NULLTTY, "-s", "USR1", pty_a, pty_b, NULL);
        return -1;

    default:
        /* Wait for any of SIGUSR1 indicating nulltty ready, SIGCHLD
         * indicating that it terminated unexpectedly, or for the user to
         * kill us. */
        wait_result = sigwait(&wait_set, &signum);
        if ( sigprocmask(SIG_SETMASK, &prev_mask, NULL) < 0 )
            return -1;

        if ( wait_result == 0 && signum == SIGUSR1 )
            return pid;

        return -1;
    }
}

int nulltty_kill(int pid)
{
    int status;
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;

    if ( sigaction(SIGCHLD, &action, NULL) < 0 )
        return -1;

    if ( kill(pid, SIGTERM) < 0 )
        return -1;

    waitpid(pid, &status, 0);
    if ( ! WIFEXITED(status) )
        return -2;

    return WEXITSTATUS(status);
}
