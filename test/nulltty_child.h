/**
 * Helper functions for testing nulltty child processes
 */

#ifndef _NULLTTY_CHILD_H_
#define _NULLTTY_CHILD_H_

#define NULLTTY "../src/nulltty"

int nulltty_child(const char *pty_a, const char *pty_b);
int nulltty_kill(int pid);

#endif /* ! defined _NULLTTY_CHILD_H_ */
