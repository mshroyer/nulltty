bin_PROGRAMS = nulltty
dist_man_MANS = ../man/nulltty.1

nulltty_SOURCES = nulltty.c ptys.h ptys.c
nulltty_LDADD =

if NEED_LIBCOMPAT
nulltty_LDADD += ../lib/libcompat.a
endif
