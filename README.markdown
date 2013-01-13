nulltty — A simple Unix pseudoterminal relay
=============================================

Mark Shroyer &lt;<code@markshroyer.com>&gt;<br/>
Sun Jan 13 18:14:33 EST 2013


## Description ##

This program simply creates two Unix pseudoterminals and relays data
between them; think of it as the software equivalent of connecting a null
modem cable between two serial ports on your computer.  It is meant for use
developing or testing serial communications programs, or in other
situations where Unix-domain sockets or even FIFOs would suffice, but for
the fact that whatever program you're running expects termios calls to
succeed.

nulltty requires an operating system supporting either UNIX98 PTYs or
OpenBSD's pseudoterminal interface.  It has been tested on OpenBSD 5.2,
Linux 3.2, FreeBSD 9.0, NetBSD 6, and Mac OS X 10.8.


## Installation ##

This is a typical Autotools project, which can be built and installed in
the usual ways.  Refer to the INSTALL file for an overview of building
Autotools projects.  Briefly, the following will usually suffice:

<pre>
$ ./configure
$ make
$ sudo make install
</pre>

Pass `--enable-debug` to the configure script to build debug symbols and
enable verbose runtime output.


## Usage ##

Invoke nulltty as follows:

<pre>
$ nulltty /path/to/ptyA /path/to/ptyB
</pre>

Now you can e.g. open `/path/to/ptyA` in that fancy serial communication
program you're developing, and `/path/to/ptyB` in minicom or
[CuteCom](http://cutecom.sourceforge.net/) in order to simulate input to
and/or output from your program.

Refer to the program's man page for detailed information about additional
options and behaviors.


## See also ##

[socat](http://www.dest-unreach.org/socat/), if you need a more
full-featured relay supporting TCP sockets and more.
