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

PKGLIBS=	x11 xft

CC?=		cc
CFLAGS+=	-O2 -Wall -Wunused \
		-Wunused -Wmissing-prototypes -Wstrict-prototypes \
		-Wpointer-sign \
		`pkg-config --cflags ${PKGLIBS}`
LDFLAGS+=	`pkg-config --libs ${PKGLIBS}`

BIN=		geometry \
		no-resize \
		win-type-utility

all: $(BIN)

atom.o: ../atom.c
harness.o: harness.c

$(BIN): atom.o harness.o $@.c
	$(CC) $(CFLAGS) -o $@ $@.c atom.o harness.o $(LDFLAGS)

clean:
	rm -f $(BIN) *.o

.PHONY: all install clean
