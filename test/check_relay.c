#include <stdio.h>
#include <termios.h>

#include "nulltty_child.h"

int main(int argc, char *argv[])
{
    int pid, status;

    printf("Checking pty creation...\n");

    pid = nulltty_child("nullttyA", "nullttyB");
    if ( pid < 0 )
        return 1;

    printf("nulltty_child() = %d\n", pid);

    status = nulltty_kill(pid);
    if ( status >= 0 )
        printf("nulltty exited with status: %d\n", status);
    if ( status != 0 )
        return 1;

    return 0;
}
