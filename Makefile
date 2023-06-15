#
# Copyright 2020 joshua stein <jcs@jcs.org>
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#

PREFIX?=	/usr/local
X11BASE?=	/usr/X11R6

PKGLIBS=	x11 xft xext xpm

CC?=		cc
CFLAGS+=	-O2 -Wall -Wunused \
		-Wunused -Wmissing-prototypes -Wstrict-prototypes \
		-Wpointer-sign \
		`pkg-config --cflags ${PKGLIBS}`
LDFLAGS+=	`pkg-config --libs ${PKGLIBS}`

# use gdk-pixbuf to rescale icons; optional
PKGLIBS+=	gdk-pixbuf-xlib-2.0
CFLAGS+=	-DUSE_GDK_PIXBUF

# enable debugging; optional
CFLAGS+=	-g
#CFLAGS+=	-g -DDEBUG=1

# use 2x icons, borders, fonts for HiDPI displays; optional
CFLAGS+=	-DHIDPI=1

BINDIR=		$(PREFIX)/bin

SRC=		atom.c \
		client.c \
		events.c \
		keyboard.c \
		launcher.c \
		manage.c \
		parser.c \
		progman.c \
		util.c

OBJ=		${SRC:.c=.o}

BIN=		progman

all: $(BIN)

$(OBJ):		progman.h Makefile
parser.o:	progman.h progman_ini.h

progman_ini.h: progman.ini
	xxd -i progman.ini > $@ || (rm -f progman_ini.h; exit 1)

progman: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

install: all
	mkdir -p $(BINDIR) $(MANDIR)
	install -s $(BIN) $(BINDIR)

clean:
	rm -f $(BIN) $(OBJ) progman_ini.h

.PHONY: all install clean
