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

PKGLIBS=	x11 xft xext xpm gtk+-2.0

CC?=		cc
CFLAGS+=	-O2 -Wall -Wunused \
		-Wunused -Wmissing-prototypes -Wstrict-prototypes \
		-Wpointer-sign \
		`pkg-config --cflags ${PKGLIBS}`
LDFLAGS+=	`pkg-config --libs ${PKGLIBS}`

# uncomment to enable debugging
#CFLAGS+=	-g -DDEBUG=1

# uncomment for HiDPI displays
CFLAGS+=	-DHIDPI=1

# uncomment to use gdk-pixbuf to rescale icons
PKGLIBS+=	gdk-pixbuf-xlib-2.0
CFLAGS+=	-DUSE_GDK_PIXBUF

BINDIR=		$(PREFIX)/bin

PROGMAN_SRC=	atom.c \
		client.c \
		common.c \
		events.c \
		keyboard.c \
		manage.c \
		menu.c \
		parser.c \
		progman.c

AEMENU_SRC=	aemenu.c \
		atom.c \
		common.c \
		menu.c \
		parser.c

PROGMAN_OBJ=	${PROGMAN_SRC:.c=.o}
AEMENU_OBJ=	${AEMENU_SRC:.c=.o}

OBJ=		$(PROGMAN_OBJ) \
		$(AEMENU_OBJ)

BIN=		progman aemenu

all: $(BIN)

$(PROGMAN_OBJ): progman.h progman_ini.h

progman_ini.h:	progman.ini
	xxd -i progman.ini > $@

progman: $(PROGMAN_OBJ)
	$(CC) -o $@ $(PROGMAN_OBJ) $(LDFLAGS)

aemenu: $(AEMENU_OBJ) progman.h
	$(CC) -o $@ $(AEMENU_OBJ) $(LDFLAGS)

install: all
	mkdir -p $(BINDIR) $(MANDIR)
	install -s $(BIN) $(BINDIR)

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: all install clean
