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

#define MAX(a, b) ((a)>(b)) ? (a) : (b)

int main(int argc, char *argv[])
{
    const uint8_t ma[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    size_t ma_sz = sizeof(ma);
    const uint8_t mb[] = { 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa };
    size_t mb_sz = sizeof(mb);
    uint8_t *ra, *rb;
    size_t oa = 0, ob = 0, ia = 0, ib = 0;
    int pid, status;
    int fd_a, fd_b;
    fd_set rfds, wfds;
    int nfds, n;

    printf("Checking pty creation...\n");

    /* Launch nulltty */

    pid = nulltty_child(TTY_A_PATH, TTY_B_PATH);
    if ( pid < 0 )
        return 1;

    /* Open PTY slaves */

    fd_a = open_pty_slave(TTY_A_PATH);
    if ( fd_a < 0 )
        return 1;

    fd_b = open_pty_slave(TTY_B_PATH);
    if ( fd_b < 0 )
        return 1;

    ra = malloc(ma_sz);
    if ( ra == NULL )
        return 1;

    rb = malloc(mb_sz);
    if ( rb == NULL )
        return 1;

    nfds = MAX(fd_a, fd_b) + 1;
    while ( oa < ma_sz || ia < ma_sz || ob < mb_sz || ib < mb_sz ) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        if ( oa < ma_sz )
            FD_SET(fd_a, &wfds);
        if ( ob < mb_sz )
            FD_SET(fd_b, &wfds);

        if ( ia < ma_sz )
            FD_SET(fd_b, &rfds);
        if ( ib < mb_sz )
            FD_SET(fd_a, &rfds);

        if ( select(nfds, &rfds, &wfds, NULL, NULL) < 0 )
            return 1;

        if ( FD_ISSET(fd_a, &wfds) ) {
            n = write(fd_a, ma, ma_sz - oa);
            if ( n < 0 )
                return 1;

            oa += n;
        }
        if ( FD_ISSET(fd_b, &wfds) ) {
            n = write(fd_b, mb, mb_sz - ob);
            if ( n < 0 )
                return 1;

            ob += n;
        }

        if ( FD_ISSET(fd_a, &rfds) ) {
            n = read(fd_a, rb + ib, mb_sz - ib);
            if ( n < 0 )
                return 1;

            ib += n;
        }
        if ( FD_ISSET(fd_b, &rfds) ) {
            read(fd_b, ra + ia, ma_sz - ia);
            if ( n < 0 )
                return 1;

            ia += n;
        }
    }

    close(fd_a);
    close(fd_b);

    if ( memcmp(ma, ra, ma_sz) != 0 )
        return 1;

    if ( memcmp(mb, rb, mb_sz) != 0 )
        return 1;

    free(ra);
    free(rb);

    /* Kill nulltty and get exit status */

    status = nulltty_kill(pid);
    if ( status >= 0 )
        printf("nulltty exited with status: %d\n", status);
    if ( status != 0 )
        return 1;

    return 0;
}
