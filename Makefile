#
# Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

# Set this to the location where you want to install
DESTDIR =
XROOT = /usr/X11R6

# Uncomment to enable Shape extension support
OPT_WMFLAGS += -DSHAPE
OPT_WMLIB += -lXext

# Uncomment for debugging (abandon all hope, ye who enter here)
#OPT_WMFLAGS += -DDEBUG
#OPT_WMLIB += -lefence

CC = gcc
CFLAGS = -g -O2 -Wall -I${XROOT}/include

BINDIR = $(DESTDIR)$(XROOT)/bin
MANDIR = $(DESTDIR)$(XROOT)/man/man1
CFGDIR = $(DESTDIR)/etc/X11/aewm

PLAINOBJ = aesession.o parser.o
CLIENTOBJ = common.o atom.o
X11OBJ = $(CLIENTOBJ) aedesk.o menu.o
WMOBJ = client.o events.o aewm.o manage.o
GTKOBJ = aemenu.o aepanel.o
ALLOBJ = $(PLAINOBJ) $(X11OBJ) $(WMOBJ) $(GTKOBJ)

PLAINBIN = aesession
X11BIN = aedesk
WMBIN = aewm
GTKBIN = aemenu aepanel
ALLBIN = $(PLAINBIN) $(X11BIN) $(WMBIN) $(GTKBIN)

all: $(ALLBIN)
aesession:
aedesk: $(CLIENTOBJ)
aewm: $(CLIENTOBJ) $(WMOBJ) parser.o
aemenu: $(CLIENTOBJ) menu.o parser.o
aepanel: $(CLIENTOBJ) menu.o parser.o

X11FLAGS = -I$(XROOT)/include
WMFLAGS = $(X11FLAGS) `pkg-config --cflags xft` $(OPT_WMFLAGS)
GTKFLAGS = `pkg-config --cflags gtk+-2.0`

$(PLAINOBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(X11OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(X11FLAGS) -c $< -o $@

$(WMOBJ): %.o: %.c
	$(CC) $(CFLAGS) $(WMFLAGS) -c $< -o $@

$(GTKOBJ): %.o: %.c
	$(CC) $(CFLAGS) $(GTKFLAGS) -c $< -o $@

X11LIB = -L$(XROOT)/lib -lX11
WMLIB = $(X11LIB) `pkg-config --libs xft` -lXext $(OPT_WMLIB)
GTKLIB = `pkg-config --libs gtk+-2.0`

$(PLAINBIN): %: %.o
	$(CC) $^ -o $@

$(X11BIN): %: %.o
	$(CC) $^ $(X11LIB) -o $@

$(WMBIN): %:
	$(CC) $^ $(WMLIB) -o $@

$(GTKBIN): %: %.o
	$(CC) $^ $(GTKLIB) -o $@

AEMAN = aewm.1x aeclients.1x
AERC = aewmrc clientsrc

install: all
	mkdir -p $(BINDIR) $(MANDIR) $(CFGDIR)
	install -s $(ALLBIN) $(BINDIR)
	for i in $(AEMAN); do \
	    install -m 644 doc/$$i $(MANDIR); \
	    gzip -9 $(MANDIR)/$$i; \
	done
	for i in $(AERC); do \
	    install -m 644 doc/$$i.ex $(CFGDIR)/$$i; \
	done
	for i in $(PLAINBIN) $(X11BIN) $(GTKBIN); do \
	    ln -sf aeclients.1x.gz $(MANDIR)/$$i.1x.gz; \
	done

clean:
	rm -f $(ALLBIN) $(ALLOBJ)

.PHONY: all install clean
