#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if __APPLE__
#define daemon yes_we_know_that_daemon_is_deprecated_in_os_x_10_5_thankyou
#include <stdlib.h>
#undef daemon
extern int daemon(int, int);
#endif

#include <string.h>
#if HAVE_LIBBSD
#include <bsd/string.h>
#else
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif
#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
#endif
#endif

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX MAXPATHLEN
#endif
