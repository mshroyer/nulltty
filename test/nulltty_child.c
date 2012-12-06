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
    int pid;

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);

    action.sa_handler = sigchld_handler;
    if ( sigaction(SIGCHLD, &action, NULL) < 0 )
        return -1;

    switch ( pid = fork() ) {
    case -1:
        return -1;

    case 0:
        execl(NULLTTY, NULLTTY, pty_a, pty_b, NULL);
        return -1;

    default:
        /*
         * Make sure we *probably* won't run any more of this test until
         * the new child process has had a chance to setup its
         * pseudoterminals and symlinks. This can, of course, fail -- but
         * it should be fine for our testing purposes. Probably.
         *
         * TODO implement this instead with a signal from nulltty to its
         * parent process, specified by an optional command line argument
         */
        sleep(3);
        return pid;
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
