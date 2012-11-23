#ifndef _NULLTTY_PTYS_H_
#define _NULLTTY_PTYS_H_

struct nulltty_endpoint {
    int fd;
    char *link;
};

struct nulltty {
    struct nulltty_endpoint a;
    struct nulltty_endpoint b;
};

typedef struct nulltty nulltty_t;

/**
 * Opens a pair of pseudoterminals and creates requested symlinks
 *
 * Uses the platform's pseudoterminal functions to open a pair of
 * pseudoterminals and then creates the requested symbolic links to their
 * slave devices.
 *
 * @param link_a Symlink name for tty A
 * @param link_b Symlink name for tty B
 * @return Pointer to nulltty struct with PTY info, or NULL on error
 */
nulltty_t *openptys(const char *link_a, const char *link_b);

/**
 * Closes a pair of pseudoterminals and cleans up their symlinks
 *
 * @param nulltty Pointer to structure returned by openptys()
 * @return 0 on success or -1 on error
 */
int closeptys(nulltty_t *nulltty);

#endif /* ! defined _NULLTTY_PTYS_H_ */
