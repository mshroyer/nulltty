nulltty -- A simple Unix pseudoterminal relay
=============================================

Mark Shroyer &lt;<code@markshroyer.com>&gt;<br/>
Sun Dec  2 21:08:12 EST 2012


## Description ##

This program simply creates two Unix pseudoterminals and relays data
between them; think of it as the software equivalent of connecting a null
modem cable between two serial ports on your computer.  It is meant for use
developing or testing serial communications programs, or in other
situations where Unix-domain sockets or even FIFOs would suffice, but for
the fact that the program you're running expects termios calls to succeed.


## Usage ##

Invoke nulltty as follows:

<pre>
$ nulltty /path/to/ptyA /path/to/ptyB
</pre>

Now you can e.g. open `/path/to/ptyA` in that fancy serial communication
program you're developing, and `/path/to/ptyB` in minicom or
[CuteCom](http://cutecom.sourceforge.net/) in order to simulate input to
and/or output from your program.


## System requirements and installation instructions ##

nulltty requires an operating system supporting UNIX98 PTYs -- Linux,
FreeBSD, and Mac OS X will work just fine.

This is a typical Autotools project, which can be built and installed in
the usual ways.  Refer to the INSTALL file for detailed installation
instructions.


## See also ##

[socat](http://www.dest-unreach.org/socat/), if you need a more
full-featured relay supporting TCP sockets and more.
