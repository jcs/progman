This fork of jcs/progman has been gently massaged to build on Debian (PIXEL desktop x86)

Massage Instructions
* 1 - Makefile.h, PKGLIBS must include libbsd xext (and also 'apt install libbsd-dev libxext-dev' !)
* 2 - events.c, #include <bsd/string.h>
* 3 - events.c, line 61, define INFTIM


