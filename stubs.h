#if __APPLE__
#define daemon yes_we_know_that_daemon_is_deprecated_in_os_x_10_5_thankyou
#include <stdlib.h>
#undef daemon
extern int daemon(int, int);
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#if HAVE_LIBBSD
#include <bsd/string.h>
#endif
