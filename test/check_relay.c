#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "nulltty_child.h"

#define TTY_A_PATH "nullttyA"
#define TTY_B_PATH "nullttyB"

#define MAX(a, b) ((a)>(b)) ? (a) : (b)

#define log_error(fmt) printf("Error " fmt "\n")
#define log_error_a(fmt, ...) printf("Error " fmt "\n", __VA_ARGS__)

struct relay_direction {
    int fd_out;
    int fd_in;
    const uint8_t *msg;
    size_t msg_sz;
    uint8_t *buf;
    size_t n_out;
    size_t n_in;
};

static int open_pty_slave(const char *path)
{
    struct termios t = { 0 };
    int fd;

    fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if ( fd < 0 )
        return -1;

    if ( tcgetattr(fd, &t) < 0 )
        return -1;
    cfmakeraw(&t);
    if ( tcsetattr(fd, TCSAFLUSH, &t) < 0 )
        return -1;

    return fd;
}

static int open_direction(struct relay_direction *dir,
                          const uint8_t *msg, size_t msg_sz,
                          const char *pty_path)
{
    dir->msg    = msg;
    dir->msg_sz = msg_sz;
    dir->n_in   = 0;
    dir->n_out  = 0;
    dir->fd_in  = -1;

    if ( ( dir->fd_out = open_pty_slave(pty_path) ) < 0 ) {
        log_error_a("opening pty slave at path %s", pty_path);
        goto error;
    }

    if ( ( dir->buf = malloc(msg_sz * sizeof(uint8_t)) ) == NULL ) {
        log_error("allocating receive buffer");
        goto error_malloc;
    }

    return 0;

 error_malloc:
    close(dir->fd_out);
 error:
    return -1;
}

static int close_direction(struct relay_direction *dir)
{
    int result = 0;

    if ( memcmp(dir->buf, dir->msg, dir->msg_sz) != 0 ) {
        log_error("checking received data against original");
        result = -1;
    }

    if ( close(dir->fd_out) < 0 ) {
        log_error("closing slave pty");
        result = -1;
    }

    free(dir->buf);
    dir->buf = NULL;

    return result;
}

static void prepare_fd_sets(fd_set *rfds, fd_set *wfds,
                            const struct relay_direction *dir)
{
    if ( dir->n_out < dir->msg_sz )
        FD_SET(dir->fd_out, wfds);

    if ( dir->n_in < dir->msg_sz )
        FD_SET(dir->fd_in, rfds);
}

static int shuffle_data(fd_set *rfds, fd_set *wfds,
                        struct relay_direction *dir)
{
    int n;

    if ( FD_ISSET(dir->fd_out, wfds) ) {
        n = write(dir->fd_out, dir->msg, dir->msg_sz - dir->n_out);
        if ( n < 0 ) {
            log_error_a("writing %zd bytes to slave pty", dir->msg_sz - dir->n_out);
            return -1;
        }

        dir->n_out += n;
    }

    if ( FD_ISSET(dir->fd_in, rfds) ) {
        n = read(dir->fd_in, dir->buf + dir->n_in, dir->msg_sz - dir->n_in);
        if ( n < 0 ) {
            log_error_a("reading %zd bytes from slave pty", dir->msg_sz - dir->n_in);
            return -1;
        }

        dir->n_in += n;
    }

    return 0;
}

static inline int shuffle_complete(const struct relay_direction *dir)
{
    return ( dir->n_out == dir->msg_sz && dir->n_in == dir->msg_sz );
}

int check_relay()
{
    const uint8_t ma[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    const uint8_t mb[] = { 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa };
    struct relay_direction dir_a, dir_b;
    int pid, nfds, status;
    fd_set rfds, wfds;

    /* Launch nulltty */

    pid = nulltty_child(TTY_A_PATH, TTY_B_PATH);
    if ( pid < 0 ) {
        log_error("forking nulltty child process");
        goto error;
    }

    /* Open PTY slaves */

    if ( open_direction(&dir_a, ma, sizeof(ma), TTY_A_PATH) < 0 )
        goto error_nulltty;

    if ( open_direction(&dir_b, mb, sizeof(mb), TTY_B_PATH) < 0 )
        goto error_open_a;

    dir_a.fd_in = dir_b.fd_out;
    dir_b.fd_in = dir_a.fd_out;

    /* Shuffle data back through the slave PTYs */

    nfds = MAX(dir_a.fd_out, dir_b.fd_out) + 1;
    while ( ! ( shuffle_complete(&dir_a) && shuffle_complete(&dir_b) ) ) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        prepare_fd_sets(&rfds, &wfds, &dir_a);
        prepare_fd_sets(&rfds, &wfds, &dir_b);

        if ( select(nfds, &rfds, &wfds, NULL, NULL) < 0 )
            goto error_open_b;

        if ( shuffle_data(&rfds, &wfds, &dir_a) < 0 )
            goto error_open_b;

        if ( shuffle_data(&rfds, &wfds, &dir_b) < 0 )
            goto error_open_b;
    }

    /* Close slaves and check data */

    if ( close_direction(&dir_b) < 0 )
        goto error_open_a;

    if ( close_direction(&dir_a) < 0 )
        goto error_nulltty;

    /* Kill nulltty and get exit status */

    status = nulltty_kill(pid);
    if ( status >= 0 ) {
        printf("nulltty exited with status: %d\n", status);
        return 0;
    } else {
        return 1;
    }

 error_open_b:
    close_direction(&dir_b);
 error_open_a:
    close_direction(&dir_a);
 error_nulltty:
    nulltty_kill(pid);
 error:
    return -1;
}

int main(int argc, char *argv[])
{
    int result;

    printf("Checking pty creation...\n");
    result = check_relay();

    if ( result < 0 )
        return -result;

    return 0;
}
