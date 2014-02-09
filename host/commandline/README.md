Command-line Tool and C library for cstbase
===========================================

To build, see the Makefile.

The current support build products are:

- `cstbase-tool` -- command-line tool for controlling CST Base Station
- `cstbase-lib` -- C library for controlling CST Base Station


Supported platforms:

- Mac OS X 10.6.8, 10.7+
- Windows XP+ (built with MinGW & MSYS)
- Linux (most all, primary development on Ubuntu)
- FreeBSD 8.2+
- Raspberry Pi (Raspian)
- BeagleBone (Ubuntu)
- OpenWRT / DD-WRT
- ... just about anything else with Gnu Make & a C-compiler

In general, the `cstbase-tool` builds as a static binary where possible,
eliminating the need for shared library dependencies on the target.





