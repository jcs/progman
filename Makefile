# aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
# This program is free software; see LICENSE for details.

# Set this to the location where you want to install
DESTDIR =
XROOT = /usr/X11R6

# Uncomment to enable Shape extension support
#OPT_WMFLAGS += -DSHAPE
#OPT_WMLIB += -lXext

# Uncomment to add Xft support
#OPT_WMFLAGS += -DXFT `pkg-config --cflags xft`
#OPT_WMLIB += `pkg-config --libs xft` -lXext

# Uncomment for debugging (abandon all hope, ye who enter here)
#OPT_WMFLAGS += -DDEBUG
#OPT_WMLIB += -lefence

CC = gcc
CFLAGS = -g -O2 -Wall

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
WMFLAGS = $(X11FLAGS) $(OPT_WMFLAGS)
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
WMLIB = $(X11LIB) $(OPT_WMLIB)
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
