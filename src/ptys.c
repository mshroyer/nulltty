#include <stubs.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ptys.h"


struct nulltty_pty {
    int fd;
    int slave_fd;
    char *link;
    uint8_t *read_buf;
    size_t read_n;
};

struct nulltty {
    struct nulltty_pty a;
    struct nulltty_pty b;
};

#define MAX(a, b) ((a)>(b)) ? (a) : (b)

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

/**
 * Open a single PTY nulltty endpoint
 *
 * Prepares one of the two pseudoterminals used by the nulltty process,
 * saving its file descriptor and other data in the given structure.
 *
 * @param pty Pseudoterminal descriptor structure, to which fd and other
 * information is to be written
 * @param link Name of symbolic link requested for this PTY slave
 * @return 0 on success, -1 with errno on error
 */
static int openpty(struct nulltty_pty *pty, const char *link)
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

/**
 * Close a single PTY nulltty endpoint
 *
 * Closes the given PTY and releases its resources.
 *
 * @param pty Pseudoterminal descriptor to close
 * @return 0 on success, -1 with errno on error
 */
static int closepty(struct nulltty_pty *pty)
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

nulltty_t nulltty_open(const char *link_a, const char *link_b)
{
    nulltty_t nulltty = NULL;

    nulltty = calloc(1, sizeof(struct nulltty));
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

int nulltty_close(nulltty_t nulltty)
{
    int result = 0;

    result += closepty(&nulltty->a);
    result += closepty(&nulltty->b);
    free(nulltty);

    return result;
}

/**
 * Prepare select() fd_sets for this iteration of the proxy
 *
 * Prepares read and write fd_sets by setting the appropriate file
 * descriptors in order for pty_dst to receive data from pty_src, depending
 * on the current state of pty_dst's receive buffer.
 *
 * This function is half-duplex with respect to the proxy.
 *
 * @param pty_dst Descriptor of receiving PTY
 * @param pty_src Descriptor of sending PTY
 * @param rfds Pointer to read fd_set
 * @param wfds Pointer to write fd_set
 */
static void proxy_set_fds(struct nulltty_pty *pty_dst,
                          struct nulltty_pty *pty_src,
                          fd_set *rfds, fd_set *wfds)
{
    if ( pty_src->read_n < READ_BUF_SZ - 1 )
        FD_SET(pty_src->fd, rfds);

    if ( pty_src->read_n > 0 )
        FD_SET(pty_dst->fd, wfds);
}

/**
 * Shuffle data between two PTYs
 *
 * Depending on the file descriptor states returned by select(), performs
 * non-blocking writes out of, and reads into, the read buffer in order to
 * shuffle data from pty_src to pty_dst.
 *
 * This function is half-duplex with respect to the proxy.
 *
 * @param pty_dst Descriptor of receiving PTY
 * @param pty_src Descriptor of sending PTY
 * @param rfds Pointer to read fd_set
 * @param wfds Pointer to write fd_set
 * @return 0 on success, -1 with errno on error
 */
static int proxy_shuffle_data(struct nulltty_pty *pty_dst,
                              struct nulltty_pty *pty_src,
                              fd_set *rfds, fd_set *wfds)
{
    ssize_t n;
#ifdef DEBUG
    bool buffer_touched = false;
#endif

    if ( FD_ISSET(pty_src->fd, rfds) ) {
        n = read(pty_src->fd, pty_src->read_buf, READ_BUF_SZ - pty_src->read_n);
        if ( n < 0 )
            return -1;

#ifdef DEBUG
        printf("Read from %s: %zd bytes\n", pty_src->link, n);
        buffer_touched = true;
#endif

        pty_src->read_n += n;
    }

    if ( FD_ISSET(pty_dst->fd, wfds) ) {
        n = write(pty_dst->fd, pty_src->read_buf, pty_src->read_n);
        if ( n < 0 )
            return -1;

#ifdef DEBUG
        printf("Write to %s: %zd bytes\n", pty_dst->link, n);
        buffer_touched = true;
#endif

        if ( n > 0 ) {
            memmove(pty_src->read_buf, pty_src->read_buf + n, pty_src->read_n - n);
            pty_src->read_n -= n;
        }
    }

#ifdef DEBUG
    if ( buffer_touched )
        printf("Buffer of %s: %zd bytes\n", pty_src->link, pty_src->read_n);
#endif

    assert(pty_src->read_n >= 0);
    assert(pty_src->read_n <= READ_BUF_SZ);
    return 0;
}

int nulltty_proxy(nulltty_t nulltty, volatile sig_atomic_t *exit_flag)
{
    int nfds = MAX(nulltty->a.fd, nulltty->b.fd) + 1;
    fd_set rfds, wfds;
    int saved_errno;

    while ( *exit_flag == 0 ) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        proxy_set_fds(&nulltty->a, &nulltty->b, &rfds, &wfds);
        proxy_set_fds(&nulltty->b, &nulltty->a, &rfds, &wfds);

        /* TODO eliminate race condition with pselect() */
        if ( select(nfds, &rfds, &wfds, NULL, NULL) < 0 ) {
            saved_errno = errno;

#ifdef DEBUG
            printf("Select returned with errno: (%d) %s\n", saved_errno,
                   strerror(saved_errno));
#endif

            switch ( saved_errno ) {
            case EINTR:
                continue;

            default:
                return -1;
            }
        }

        if ( proxy_shuffle_data(&nulltty->a, &nulltty->b, &rfds, &wfds) < 0 )
            return -1;
        if ( proxy_shuffle_data(&nulltty->b, &nulltty->a, &rfds, &wfds) < 0 )
            return -1;
    }

    return 0;
}
