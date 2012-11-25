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

static int openpty(nulltty_pty_t *pty, const char *link)
{
    int link_len;

    pty->fd = platform_openpt();
    if ( pty->fd < 0 )
        goto error_openpt;

    /*
     * On Linux at least, when the last fd of the slave PTY is closed an
     * error condition is set, causing reads of the master side to result
     * in EIO.  By holding our own copy of the slave PTY open we can avoid
     * this, preventing more complicated error handling in our select()
     * loop.
     */
    pty->slave_fd = open(ptsname(pty->fd), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if ( pty->slave_fd < 0 )
        goto error_open_slave;

    link_len = strnlen(link, PATH_MAX);
    if ( link[link_len] != '\0' ) {
        errno = ENAMETOOLONG;
        goto error_link_name;
    }

    pty->link = calloc(link_len+1, sizeof(char));
    if ( pty->link == NULL )
        goto error_link_name;

    strlcpy(pty->link, link, link_len+1);

    pty->read_buf = calloc(READ_BUF_SZ, 1);
    if ( pty->read_buf == NULL )
        goto error_read_buf;
    pty->read_n = 0;

    if ( symlink(ptsname(pty->fd), link) < 0 )
        goto error_symlink;

    return 0;

 error_symlink:
    free(pty->read_buf);
 error_read_buf:
    free(pty->link);
 error_link_name:
    close(pty->slave_fd);
 error_open_slave:
    close(pty->fd);
 error_openpt:
    return -1;
}

static int closepty(nulltty_pty_t *pty)
{
    int result = 0;

    if ( close(pty->slave_fd) < 0 )
        result = -1;

    if ( close(pty->fd) < 0 )
        result = -1;

    if ( unlink(pty->link) < 0 )
        result = -1;

    free(pty->link);
    pty->link = NULL;

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

static int writepty(nulltty_pty_t *pty, nulltty_pty_t *pty_src)
{
    int n;

    n = write(pty->fd, pty_src->read_buf, pty_src->read_n);
    if ( n < 0 )
        return -1;

    assert(n <= pty_src->read_n);

    if ( n > 0 ) {
        memmove(pty_src->read_buf, pty_src->read_buf + n, pty_src->read_n - n);
        pty_src->read_n -= n;
    }

    assert(pty_src->read_n >= 0);
    assert(pty_src->read_n <= READ_BUF_SZ);
    return 0;
}

static int readpty(nulltty_pty_t *pty)
{
    int n;

    n = read(pty->fd, pty->read_buf, READ_BUF_SZ - pty->read_n);
    if ( n < 0 )
        return -1;

    pty->read_n += n;

    assert(pty->read_n >= 0);
    assert(pty->read_n <= READ_BUF_SZ);
    return 0;
}

static void proxyptys_set_fds(nulltty_pty_t *pty_dst, nulltty_pty_t *pty_src,
                              fd_set *rfds, fd_set *wfds)
{
    if ( pty_src->read_n < READ_BUF_SZ - 1 )
        FD_SET(pty_src->fd, rfds);

    if ( pty_src->read_n > 0 )
        FD_SET(pty_dst->fd, wfds);
}

static int proxyptys_shuffle_data(nulltty_pty_t *pty_dst, nulltty_pty_t *pty_src,
                                  fd_set *rfds, fd_set *wfds)
{
    if ( FD_ISSET(pty_dst->fd, wfds) ) {
        if ( writepty(pty_dst, pty_src) < 0 )
            return -1;
    }

    if ( FD_ISSET(pty_src->fd, rfds) ) {
        if ( readpty(pty_src) < 0 )
            return -1;
    }

    return 0;
}

int proxyptys(nulltty_t *nulltty)
{
    int nfds = MAX(nulltty->a.fd, nulltty->b.fd) + 1;
    fd_set rfds, wfds;

    while ( shutdown == 0 ) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        proxyptys_set_fds(&nulltty->a, &nulltty->b, &rfds, &wfds);
        proxyptys_set_fds(&nulltty->b, &nulltty->a, &rfds, &wfds);

        if ( select(nfds, &rfds, &wfds, NULL, NULL) < 0 ) {
            /* TODO error (incl. signal) handling */
            perror("Select returned an error");
        }

        if ( proxyptys_shuffle_data(&nulltty->a, &nulltty->b, &rfds, &wfds) < 0 )
            return -1;
        if ( proxyptys_shuffle_data(&nulltty->b, &nulltty->a, &rfds, &wfds) < 0 )
            return -1;
    }

    return 0;
}
