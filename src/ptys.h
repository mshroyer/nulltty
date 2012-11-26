#ifndef _NULLTTY_PTYS_H_
#define _NULLTTY_PTYS_H_

#include <stdint.h>

/**
 * Size of the half-duplex buffer between pseudoterminals
 *
 * Two of these will be allocated for each PTY pair.
 */
#define READ_BUF_SZ 1024

struct nulltty; /* Forward declaration */
typedef struct nulltty *nulltty_t;

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
nulltty_t nulltty_open(const char *link_a, const char *link_b);

/**
 * Closes a pair of pseudoterminals and cleans up their symlinks
 *
 * @param nulltty Pointer to structure returned by openptys()
 * @return 0 on success or -1 on error
 */
int nulltty_close(nulltty_t nulltty);

/**
 * Proxy data between the pseudoterminal pair
 *
 * Implements the program's main loop behavior of ferrying data between the
 * two pseudoterminal devices.
 *
 * @param nulltty Pointer to structure returned by openptys()
 * @param exit_flag Flag to signal program termination
 * @return 0 on success (user request termination), -1 on error
 */
int nulltty_proxy(nulltty_t nulltty, volatile sig_atomic_t *exit_flag);

#endif /* ! defined _NULLTTY_PTYS_H_ */
