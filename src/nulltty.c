#include <stubs.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "ptys.h"

int main(int argc, char* argv[])
{
    nulltty_t *nulltty;

    nulltty = openptys("/home/mshroyer/Desktop/foo", "/home/mshroyer/Desktop/bar");
    if ( nulltty == NULL ) {
        perror("Error opening requested PTYs");
        return 1;
    }

    getchar();

    closeptys(nulltty);

    return 0;
}
