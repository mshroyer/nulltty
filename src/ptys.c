#include <stubs.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#ifdef HAVE_UTIL_H
#include <util.h>
#endif

#include "ptys.h"


/*** DATA STRUCTURES **********************************************************/

struct nulltty_pty {
    int fd;
    int slave_fd;
    char *link;
    uint8_t *read_buf;
    size_t read_n;
#ifdef DEBUG
    size_t read_total;
    size_t write_total;
#endif
};

struct nulltty {
    struct nulltty_pty a;
    struct nulltty_pty b;
};


/*** DEBUGGING INSTRUMENTATION ************************************************/

#ifdef DEBUG

static unsigned long nsyscalls = 0;
static unsigned long nreads = 0;
static unsigned long nwrites = 0;
static unsigned long nselects = 0;

static inline ssize_t debug_read(int fd, void *buf, size_t count) {
    nsyscalls++;
    nreads++;
    return read(fd, buf, count);
}
#define read(...) debug_read(__VA_ARGS__)

static inline ssize_t debug_write(int fd, const void *buf, size_t count) {
    nsyscalls++;
    nwrites++;
    return write(fd, buf, count);
}
#define write(...) debug_write(__VA_ARGS__)

#ifdef HAVE_PSELECT

static inline ssize_t debug_pselect(int nfds, fd_set *readfds, fd_set *writefds,
                                    fd_set *exceptfds, struct timespec *timeout,
                                    const sigset_t *sigmask)
{
    nsyscalls++;
    nselects++;
    return pselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}
#define pselect(...) debug_pselect(__VA_ARGS__)

#else /* defined HAVE_PSELECT */

static inline ssize_t debug_select(int nfds, fd_set *readfds, fd_set *writefds,
                                   fd_set *exceptfds, struct timeval *timeout)
{
    nsyscalls++;
    nselects++;
    return select(nfds, readfds, writefds, exceptfds, timeout);
}
#define select(...) debug_select(__VA_ARGS__)

#endif /* ! defined HAVE_PSELECT */

#endif /* DEBUG */


/*** HELPER FUNCTIONS *********************************************************/

#define MAX(a, b) ((a)>(b)) ? (a) : (b)

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
static int endpoint_open(struct nulltty_pty *pty, const char *link)
{
    int link_len;
    struct termios t = { 0 };

#ifdef HAVE_POSIX_OPENPT

    /*
     * We don't specify the O_NONBLOCK flag here, because it is a
     * nonstandard flag to the posix_openpt() system call and results in an
     * error on FreeBSD 9.0. And we do not set O_NONBLOCK with fcntl()
     * after the fact because this results in an error, with errno = ENOTTY
     * on OS X 10.8.
     *
     * So instead of opening our PTY masters in non-blocking mode, we rely
     * on the behavior of the main select loop that we never attempt to
     * read from a master unless we can do so without blocking.
     */
    pty->fd = posix_openpt(O_RDWR | O_NOCTTY);
    if ( pty->fd < 0 )
        goto error_openpt;

    if ( grantpt(fd) < 0 )
        goto error_opened;

    if ( unlockpt(fd) < 0 )
        goto error_opened;

    /*
     * On Linux at least, when the last fd of the slave PTY is closed an
     * error condition is set, causing reads of the master side to result
     * in EIO.  By holding our own copy of the slave PTY open we can avoid
     * this, preventing more complicated error handling in our select()
     * loop.
     */
    pty->slave_fd = open(ptsname(pty->fd), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if ( pty->slave_fd < 0 )
        goto error_opened;

#else /* defined POSIX_OPENPT */

    /* The name parameter to openpty() needs to contain "at least sixteen
     * characters" according to the man page in OpenBSD 5.2... */
    char pty_slave_name[16];
    if ( openpty(&pty->fd, &pty->slave_fd, pty_slave_name, NULL, NULL) < 0 )
        goto error_openpt;

#endif /* ! defined POSIX_OPENPT */

    /*
     * Put the slave pty fd into raw mode.
     */
    if ( tcgetattr(pty->slave_fd, &t) == -1 )
        goto error_termios;
    cfmakeraw(&t);
    if ( tcsetattr(pty->slave_fd, TCSAFLUSH, &t) == -1 )
        goto error_termios;

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

#ifdef HAVE_PTSNAME
    if ( symlink(ptsname(pty->fd), link) < 0 )
        goto error_symlink;
#else
    if ( symlink(pty_slave_name, link) < 0 )
        goto error_symlink;
#endif

    return 0;

 error_symlink:
    free(pty->read_buf);
 error_read_buf:
    free(pty->link);
 error_termios:
 error_link_name:
    close(pty->slave_fd);
#ifdef HAVE_POSIX_OPENPT
 error_opened:
#endif
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
static int endpoint_close(struct nulltty_pty *pty)
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

/**
 * Prepare select() fd_sets for this iteration of the relay
 *
 * Prepares read and write fd_sets by setting the appropriate file
 * descriptors in order for pty_dst to receive data from pty_src, depending
 * on the current state of pty_dst's receive buffer.
 *
 * This function is half-duplex with respect to the relay.
 *
 * @param pty_dst Descriptor of receiving PTY
 * @param pty_src Descriptor of sending PTY
 * @param rfds Pointer to read fd_set
 * @param wfds Pointer to write fd_set
 */
static void relay_set_fds(struct nulltty_pty *pty_dst,
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
 * This function is half-duplex with respect to the relay.
 *
 * @param pty_dst Descriptor of receiving PTY
 * @param pty_src Descriptor of sending PTY
 * @param rfds Pointer to read fd_set
 * @param wfds Pointer to write fd_set
 * @return 0 on success, -1 with errno on error
 */
static int relay_shuffle_data(struct nulltty_pty *pty_dst,
                              struct nulltty_pty *pty_src,
                              fd_set *rfds, fd_set *wfds)
{
    ssize_t n;

    if ( FD_ISSET(pty_src->fd, rfds) ) {
        n = read(pty_src->fd, pty_src->read_buf + pty_src->read_n,
                 READ_BUF_SZ - pty_src->read_n);
        if ( n < 0 )
            return -1;

        pty_src->read_n += n;

#ifdef DEBUG
        pty_src->read_total += n;
#endif
    }

    if ( FD_ISSET(pty_dst->fd, wfds) ) {
        n = write(pty_dst->fd, pty_src->read_buf, pty_src->read_n);
        if ( n < 0 )
            return -1;

        if ( n > 0 ) {
            memmove(pty_src->read_buf, pty_src->read_buf + n, pty_src->read_n - n);
            pty_src->read_n -= n;
        }

#ifdef DEBUG
        pty_dst->write_total += n;
#endif
    }

    assert(pty_src->read_n >= 0);
    assert(pty_src->read_n <= READ_BUF_SZ);
    return 0;
}


/*** INTERFACE FUNCTIONS ******************************************************/

nulltty_t nulltty_open(const char *link_a, const char *link_b)
{
    nulltty_t nulltty = NULL;

    nulltty = calloc(1, sizeof(struct nulltty));
    if ( nulltty == NULL )
        goto error_nulltty;

    if ( endpoint_open(&nulltty->a, link_a) < 0 )
        goto error_link_a;

    if ( endpoint_open(&nulltty->b, link_b) < 0 )
        goto error_link_b;

    return nulltty;

 error_link_b:
    endpoint_close(&nulltty->a);
 error_link_a:
    free(nulltty);
 error_nulltty:
    return NULL;
}

int nulltty_close(nulltty_t nulltty)
{
    int result = 0;

    result += endpoint_close(&nulltty->a);
    result += endpoint_close(&nulltty->b);
    free(nulltty);

    return result;
}

int nulltty_relay(nulltty_t nulltty, volatile sig_atomic_t *exit_flag)
{
    int nfds = MAX(nulltty->a.fd, nulltty->b.fd) + 1;
    fd_set rfds, wfds;
    sigset_t block_set, prev_set;
    int result = 0;

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGINT);
    sigaddset(&block_set, SIGTERM);
    sigaddset(&block_set, SIGHUP);

    while ( true ) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        relay_set_fds(&nulltty->a, &nulltty->b, &rfds, &wfds);
        relay_set_fds(&nulltty->b, &nulltty->a, &rfds, &wfds);

        if ( sigprocmask(SIG_BLOCK, &block_set, &prev_set) < 0 ) {
            result = -1;
            goto end;
        }

        if ( *exit_flag != 0 )
            goto end_masked;

#ifdef HAVE_PSELECT

        if ( pselect(nfds, &rfds, &wfds, NULL, NULL, &prev_set) < 0 ) {

#else /* defined HAVE_PSELECT */

        if ( sigprocmask(SIG_SETMASK, &prev_set, NULL) < 0 ) {
            result = -1;
            goto end;
        }
        if ( select(nfds, &rfds, &wfds, NULL, NULL) < 0 ) {

#endif /* ! defined HAVE_PSELECT */

            switch ( errno ) {
            case EINTR:
                sigprocmask(SIG_SETMASK, &prev_set, NULL);
                continue;

            default:
                result = -1;
                goto end_masked;
            }
        }

        if ( sigprocmask(SIG_SETMASK, &prev_set, NULL) < 0 ) {
            result = -1;
            goto end;
        }

        if ( relay_shuffle_data(&nulltty->a, &nulltty->b, &rfds, &wfds) < 0
             || relay_shuffle_data(&nulltty->b, &nulltty->a, &rfds, &wfds) < 0 ) {
            result = -1;
            goto end;
        }

#ifdef DEBUG
        printf("%lu\t%lu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
               nselects, nsyscalls,
               nulltty->a.read_n, nulltty->a.read_total, nulltty->a.write_total,
               nulltty->b.read_n, nulltty->b.read_total, nulltty->b.write_total);
#endif
    }

 end_masked:
    sigprocmask(SIG_SETMASK, &prev_set, NULL);
 end:

#ifdef DEBUG
    printf("\n\n"
           "========================================\n"
           "Totals\n"
           "========================================\n"
           "select()s:                  %lu\n"
           "read()s:                    %lu\n"
           "write()s:                   %lu\n"
           "All tracked syscalls:       %lu\n"
           "Bytes read from PTY A:      %zu\n"
           "Bytes written to PTY A:     %zu\n"
           "Bytes read from PTY B:      %zu\n"
           "Bytes written to PTY B:     %zu\n",
           nselects, nreads, nwrites, nsyscalls,
           nulltty->a.read_total, nulltty->a.write_total,
           nulltty->b.read_total, nulltty->b.write_total);
#endif

    return result;
}
