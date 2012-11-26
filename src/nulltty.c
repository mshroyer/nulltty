#include <stubs.h>

#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

#include "ptys.h"


/*** SIGNAL HANDLERS **********************************************************/

static volatile sig_atomic_t exit_flag = 0;

static void sighup_handler(int signum)
{
}

static void sigterm_handler(int signum)
{
    exit_flag = 1;
}


/*** HELPER FUNCTIONS *********************************************************/

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
        "\t-p, --pid-file=file\n"
        "\t\tWrite PID file\n"
        "\n"
        "\t-h, --help\n"
        "\t\tShow this help message and exit\n"
        "\n";

    printf("%s", usage_info);
    exit(retval);
}


/*** MAIN PROGRAM *************************************************************/

int main(int argc, char* argv[])
{
    nulltty_t nulltty;
    int longindex, c = 0;
    const char *options = "hdp:v";
    const struct option long_options[] = {
        {"help",      no_argument,       NULL, 'h'},
        {"daemonize", no_argument,       NULL, 'd'},
        {"pid-file",  required_argument, NULL, 'p'},
        {"verbose",   required_argument, NULL, 'v'},
    };
    bool daemonize = false, verbose = false;
    char *pid_file = NULL;
    const char *link_a, *link_b;
    struct sigaction action;

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

    action.sa_handler = sighup_handler;
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
            pid_file = optarg;
            break;

        case 'v':
            verbose = true;
            break;
        }
    }

    /* We should have exactly two remaining arguments for the
     * pseudoterminal slave symlink names... */
    if ( optind != argc - 2 )
        print_usage(1);

    link_a = argv[argc-2];
    link_b = argv[argc-1];

    /*** Open pseudoterminals ***/

    nulltty = nulltty_open(link_a, link_b);
    if ( nulltty == NULL ) {
        perror("Error opening requested PTYs");
        return 1;
    }

    /*** Pseudoterminal data shuffling main loop ***/

    if ( nulltty_proxy(nulltty, &exit_flag) < 0 ) {
        perror("Proxying failed");
        nulltty_close(nulltty);
        return 2;
    }

    nulltty_close(nulltty);
    return 0;
}
