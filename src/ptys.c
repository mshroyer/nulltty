#include <stubs.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ptys.h"


#define MAX(a, b) ((a)>(b)) ? (a) : (b)

extern volatile sig_atomic_t shutdown;

static int platform_openpt()
{
    int fd;

    fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
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

    /*
     * On Linux at least, when the last fd of the slave PTY is closed an
     * error condition is set, causing reads of the master side to result
     * in EIO.  By holding our own copy of the slave PTY open we can avoid
     * this, preventing more complicated error handling in our select()
     * loop.
     */
    ep->slave_fd = open(ptsname(ep->fd), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if ( ep->slave_fd < 0 )
        goto error_open_slave;

    link_len = strnlen(link, PATH_MAX);
    if ( link[link_len] != '\0' ) {
        errno = ENAMETOOLONG;
        goto error_link_name;
    }

    ep->link = calloc(link_len+1, sizeof(char));
    if ( ep->link == NULL )
        goto error_link_name;

    strlcpy(ep->link, link, link_len+1);

    ep->read_buf = calloc(READ_BUF_SZ, 1);
    if ( ep->read_buf == NULL )
        goto error_read_buf;
    ep->read_i = 0;

    if ( symlink(ptsname(ep->fd), link) < 0 )
        goto error_symlink;

    return 0;

 error_symlink:
    free(ep->read_buf);
 error_read_buf:
    free(ep->link);
 error_link_name:
    close(ep->slave_fd);
 error_open_slave:
    close(ep->fd);
 error_openpt:
    return -1;
}

static int closepty(struct nulltty_endpoint *ep)
{
    int result = 0;

    if ( close(ep->slave_fd) < 0 )
        result = -1;

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
    closepty(&nulltty->a);
 error_link_a:
    free(nulltty);
 error_nulltty:
    return NULL;
}

int closeptys(nulltty_t *nulltty)
{
    int result = 0;

    result += closepty(&nulltty->a);
    result += closepty(&nulltty->b);
    free(nulltty);

    return result;
}

static int writepty(struct nulltty_endpoint *ep, struct nulltty_endpoint *paired)
{
    int n;

    n = write(ep->fd, paired->read_buf, paired->read_i);
    if ( n < 0 )
        return -1;

    assert(n <= paired->read_i);

    if ( n > 0 ) {
        memmove(paired->read_buf, paired->read_buf + n, paired->read_i - n);
        paired->read_i -= n;
    }

    assert(paired->read_i >= 0);
    assert(paired->read_i <= READ_BUF_SZ);
    return 0;
}

static int readpty(struct nulltty_endpoint *ep)
{
    int n;

    n = read(ep->fd, ep->read_buf, READ_BUF_SZ - ep->read_i);
    if ( n < 0 )
        return -1;

    ep->read_i += n;

    assert(ep->read_i >= 0);
    assert(ep->read_i <= READ_BUF_SZ);
    return 0;
}

int proxyptys(nulltty_t *nulltty)
{
    int nfds = MAX(nulltty->a.fd, nulltty->b.fd) + 1;
    fd_set rfds, wfds;

    while ( shutdown == 0 ) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        if ( nulltty->a.read_i < READ_BUF_SZ - 1 )
            FD_SET(nulltty->a.fd, &rfds);
        if ( nulltty->b.read_i < READ_BUF_SZ - 1 )
            FD_SET(nulltty->b.fd, &rfds);

        if ( nulltty->b.read_i > 0 )
            FD_SET(nulltty->a.fd, &wfds);
        if ( nulltty->a.read_i > 0 )
            FD_SET(nulltty->b.fd, &wfds);

        if ( select(nfds, &rfds, &wfds, NULL, NULL) < 0 ) {
            /* TODO error (incl. signal) handling */
            perror("Select returned an error");
        }

        if ( FD_ISSET(nulltty->a.fd, &wfds) ) {
            if ( writepty(&nulltty->a, &nulltty->b) < 0 )
                return -1;
        }
        if ( FD_ISSET(nulltty->b.fd, &wfds) ) {
            if ( writepty(&nulltty->b, &nulltty->a) < 0 )
                return -1;
        }

        if ( FD_ISSET(nulltty->a.fd, &rfds) ) {
            if ( readpty(&nulltty->a) < 0 )
                return -1;
        }
        if ( FD_ISSET(nulltty->b.fd, &rfds) ) {
            if ( readpty(&nulltty->b) < 0 )
                return -1;
        }
    }

    return 0;
}
