AM_CPPFLAGS = -I$(top_srcdir)

CHECK_LDADD =
if NEED_LIBCOMPAT
CHECK_LDADD += ../lib/libcompat.a
endif

check_PROGRAMS = check_relay

check_relay_SOURCES = check_relay.c nulltty_child.h nulltty_child.c
check_relay_LDADD = $(CHECK_LDADD)

check:
	./check_relay

.PHONY: all clean check
