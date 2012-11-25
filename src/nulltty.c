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


/** Should the application main loop terminate due to user request? */
volatile sig_atomic_t shutdown = 0;

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

int main(int argc, char* argv[])
{
    nulltty_t *nulltty;
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

    nulltty = nulltty_open(link_a, link_b);
    if ( nulltty == NULL ) {
        perror("Error opening requested PTYs");
        return 1;
    }

    if ( nulltty_proxy(nulltty) < 0 ) {
        perror("Proxying failed");
        nulltty_close(nulltty);
        return 2;
    }

    nulltty_close(nulltty);
    return 0;
}
