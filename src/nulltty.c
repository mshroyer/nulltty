#include <stubs.h>

#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "ptys.h"


#define SIG_NAME_MAX 128

static volatile sig_atomic_t exit_flag = 0;

static void sigterm_handler(int signum)
{
    exit_flag = 1;
}

static void print_usage(int retval)
{
    const char *usage_info =
        "Usage: nulltty [OPTIONS] path_a path_b\n"
        "\n"
        "Provides a pair of joined pseudoterminal slaves, symbolically linked from\n"
        "the given paths.  The terminals are joined such that the input to terminal\n"
        "A serves as the output from terminal B, and vice-versa; the pseudoterminals\n"
        "act like two ends of a null modem cable, except implemented in software.\n"
        "\n"
        "Options:\n"
        "\t-v, --verbose\n"
        "\t\tEnable verbose (debugging) output\n"
        "\n"
        "\t-d, --daemonize\n"
        "\t\tDaemonize the program\n"
        "\n"
        "\t-p <file>, --pid-file=<file>\n"
        "\t\tWrite PID file\n"
        "\n"
        "\t-s <sig>, --signal-parent=<sig>\n"
        "\t\tNotify nulltty's parent process with the given signal when\n"
        "\t\tthe pseoduterminals are ready\n"
        "\n"
        "\t-h, --help\n"
        "\t\tShow this help message and exit\n"
        "\n";

    printf("%s", usage_info);
    exit(retval);
}

static int write_pid(const char *pid_path)
{
    FILE *pid_file;

    if ( ( pid_file = fopen(pid_path, "w") ) == NULL )
        goto error;

    if ( fprintf(pid_file, "%d\n", getpid()) < 1 )
        goto error_fprintf;

    if ( fclose(pid_file) != 0 )
        goto error;

    return 0;

 error_fprintf:
    fclose(pid_file);
 error:
    return -1;
}

static int upcase(char *str, size_t size)
{
    size_t i;

    for ( i = 0; i < size && str[i] != '\0'; i++ ) {
        if ( str[i] >= 0x61 && str[i] <= 0x7a )
            str[i] -= 0x20;
    }

    return i;
}

static int sig_num(const char *sig_name)
{
    char name[SIG_NAME_MAX];
    int result = -1;
    char *endptr = name;

    strlcpy(name, sig_name, SIG_NAME_MAX);
    upcase(name, SIG_NAME_MAX);

    result = strtol(name, &endptr, 10);
    if ( *endptr == '\0' && name[0] != '\0' )
        return result;

    /* wannabe lisp */
#define CHECK_SIG(NAME) do {                                            \
        if ( strncmp(#NAME, name, SIG_NAME_MAX) == 0 )                  \
            return SIG ## NAME;                                         \
    } while ( 0 )

    CHECK_SIG(HUP);
    CHECK_SIG(INT);
    CHECK_SIG(KILL);
    CHECK_SIG(TERM);
#ifdef SIGINFO
    CHECK_SIG(INFO);
#endif
    CHECK_SIG(USR1);
    CHECK_SIG(USR2);

    return -1;
}

int main(int argc, char* argv[])
{
    nulltty_t nulltty;
    int longindex, c = 0;
    const char *options = "hdvp:s:";
    const struct option long_options[] = {
        {"help",          no_argument,       NULL, 'h'},
        {"daemonize",     no_argument,       NULL, 'd'},
        {"verbose",       no_argument,       NULL, 'v'},
        {"pid-file",      required_argument, NULL, 'p'},
        {"signal-parent", required_argument, NULL, 's'},
    };
    bool daemonize = false;
    char *pid_path = NULL;
    const char *link_a, *link_b;
    struct sigaction action;
    int status = 0;
    int signum = -1;

    /*** Establish signal handlers ***/

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);

    action.sa_handler = sigterm_handler;
    if ( sigaction(SIGINT, &action, NULL) < 0 ) {
        perror("Unable to establish SIGINT handler");
        return 1;
    }
    if ( sigaction(SIGTERM, &action, NULL) < 0 ) {
        perror("Unable to establish SIGTERM handler");
        return 1;
    }
    if ( sigaction(SIGHUP, &action, NULL) < 0 ) {
        perror("Unable to establish SIGHUP handler");
        return 1;
    }

    /*** Process command-line arguments ***/

    while ( ( c = getopt_long(argc, argv, options,
                              long_options, &longindex) ) != -1 ) {
        switch ( c ) {
        case 'h':
            print_usage(0);
            break;

        case 'd':
            daemonize = true;
            break;

        case 'p':
            pid_path = optarg;
            break;

        case 's':
            signum = sig_num(optarg);
            if ( signum < 0 ) {
                fprintf(stderr, "Invalid signal name: %s", optarg);
                exit(1);
            }
            break;
        }
    }

    /* We should have exactly two remaining arguments for the
     * pseudoterminal slave symlink names... */
    if ( optind != argc - 2 )
        print_usage(1);

    link_a = argv[argc-2];
    link_b = argv[argc-1];

    /*** Daemonization ***/

    if ( daemonize ) {
        if ( daemon(0, 0) != 0 ) {
            perror("Error daemonizing");
            status = 1;
            goto end;
        }
    }

    if ( pid_path != NULL ) {
        if ( write_pid(pid_path) < 0 ) {
            perror("Error writing pid file");
            status = 1;
            goto end;
        }
    }

    /*** Open pseudoterminals ***/

    nulltty = nulltty_open(link_a, link_b);
    if ( nulltty == NULL ) {
        perror("Error opening requested PTYs");
        status = 1;
        goto end_pid;
    }

    if ( signum != -1 ) {
        if ( kill(getppid(), signum) < 0 ) {
            perror("Unable to signal parent");
            status = 1;
            goto end_nulltty;
        }
    }

    /*** Pseudoterminal data shuffling main loop ***/

    if ( nulltty_relay(nulltty, &exit_flag) < 0 ) {
        perror("Relaying failed");
        status = 2;
        goto end_nulltty;
    }

 end_nulltty:
    nulltty_close(nulltty);
 end_pid:
    unlink(pid_path);
 end:
    return status;
}
