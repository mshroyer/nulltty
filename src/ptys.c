#include <stubs.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ptys.h"

static int platform_openpt()
{
    int fd;

    fd = posix_openpt(O_RDWR | O_NOCTTY);
    if ( fd < 0 )
        goto error;

    if ( grantpt(fd) < 0 )
        goto error_opened;

    if ( unlockpt(fd) < 0 )
        goto error_opened;

    return fd;

 error_opened:
    close(fd);
 error:
    return -1;
}

static int platform_closept(int fd)
{
    return close(fd);
}

static int openpty(struct nulltty_endpoint *ep, const char *link)
{
    int link_len;

    ep->fd = platform_openpt();
    if ( ep->fd < 0 )
        goto error_openpt;

    /* TODO platform's max path name length? */
    link_len = strlen(link);
    ep->link = calloc(link_len+1, sizeof(char));
    if ( ep->link == NULL )
        goto error_calloc;

    strlcpy(ep->link, link, link_len+1);

    if ( symlink(ptsname(ep->fd), link) < 0 )
        goto error_symlink;

    return 0;

 error_symlink:
    free(ep->link);
 error_calloc:
    close(ep->fd);
 error_openpt:
    return -1;
}

static int closept(struct nulltty_endpoint *ep)
{
    int result = 0;

    if ( close(ep->fd) < 0 )
        result = -1;

    if ( unlink(ep->link) < 0 )
        result = -1;

    free(ep->link);
    ep->link = NULL;

    return result;
}

nulltty_t *openptys(const char *link_a, const char *link_b)
{
    nulltty_t *nulltty = NULL;

    nulltty = calloc(1, sizeof(nulltty_t));
    if ( nulltty == NULL )
        goto error_nulltty;

    if ( openpty(&nulltty->a, link_a) < 0 )
        goto error_link_a;

    if ( openpty(&nulltty->b, link_b) < 0 )
        goto error_link_b;

    return nulltty;

 error_link_b:
    closept(&nulltty->a);
 error_link_a:
    free(nulltty);
 error_nulltty:
    return NULL;
}

int closeptys(nulltty_t *nulltty)
{
    int result = 0;

    result += closept(&nulltty->a);
    result += closept(&nulltty->b);
    free(nulltty);

    return result;
}
