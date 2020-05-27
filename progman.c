/*
 * Copyright 2020 joshua stein <jcs@jcs.org>
 * Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#ifdef USE_GDK_PIXBUF
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
#endif
#include "progman.h"
#include "atom.h"
#include "parser.h"

#ifdef HIDPI
#include "icons/close_hidpi.xpm"
#include "icons/iconify_hidpi.xpm"
#include "icons/zoom_hidpi.xpm"
#include "icons/unzoom_hidpi.xpm"
#include "icons/default_icon_hidpi.xpm"
#else
#include "icons/close.xpm"
#include "icons/iconify.xpm"
#include "icons/zoom.xpm"
#include "icons/unzoom.xpm"
#include "icons/default_icon.xpm"
#endif

client_t *focused;
int screen;
unsigned long ndesks = 1;
unsigned long cur_desk = 0;
unsigned int focus_order = 0;
Bool shape;
int shape_event;

XftFont *xftfont;
XftFont *icon_xftfont;
XftColor xft_fg;
XftColor xft_fg_unfocused;

Colormap def_cmap;
XColor fg;
XColor bg;
XColor fg_unfocused;
XColor bg_unfocused;
XColor button_bg;
XColor bevel_dark;
XColor bevel_light;
XColor bd;
GC pixmap_gc;
GC invert_gc;
Pixmap close_pm;
Pixmap close_pm_mask;
XpmAttributes close_pm_attrs;
Pixmap iconify_pm;
Pixmap iconify_pm_mask;
XpmAttributes iconify_pm_attrs;
Pixmap zoom_pm;
Pixmap zoom_pm_mask;
XpmAttributes zoom_pm_attrs;
Pixmap unzoom_pm;
Pixmap unzoom_pm_mask;
XpmAttributes unzoom_pm_attrs;
Pixmap default_icon_pm;
Pixmap default_icon_pm_mask;
XpmAttributes default_icon_pm_attrs;
Cursor map_curs;
Cursor move_curs;
Cursor resize_n_curs;
Cursor resize_s_curs;
Cursor resize_e_curs;
Cursor resize_w_curs;
Cursor resize_nw_curs;
Cursor resize_sw_curs;
Cursor resize_ne_curs;
Cursor resize_se_curs;

int exitmsg[2];

char *opt_xftfont = DEF_XFTFONT;
char *opt_icon_xftfont = DEF_ICON_XFTFONT;
char *opt_fg = DEF_FG;
char *opt_bg = DEF_BG;
char *opt_fg_unfocused = DEF_FG_UNFOCUSED;
char *opt_bg_unfocused = DEF_BG_UNFOCUSED;
char *opt_button_bg = DEF_BUTTON_BG;
char *opt_bevel_dark = DEF_BEVEL_DARK;
char *opt_bevel_light = DEF_BEVEL_LIGHT;
char *opt_bd = DEF_BD;
int opt_bw = DEF_BW;
int opt_pad = DEF_PAD;
int opt_bevel = DEF_BEVEL;
char *opt_new[] = { DEF_NEW1, DEF_NEW2, DEF_NEW3, DEF_NEW4, DEF_NEW5 };
int opt_edge_resist = DEF_EDGE_RES;

static void cleanup(void);
static void read_config(char *);
static void setup_display(void);

int
main(int argc, char **argv)
{
	struct sigaction act;
	int ch;

	setlocale(LC_ALL, "");
	read_config(NULL);

	while ((ch = getopt(argc, argv, "c:")) != -1) {
		switch (ch) {
		case 'c':
			read_config(optarg);
			break;
		default:
			printf("usage: %s [-c <config file>]\n", argv[0]);
			exit(1);
		}
	}

	pipe2(exitmsg, O_CLOEXEC);

	act.sa_handler = sig_handler;
	act.sa_flags = 0;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);

	setup_display();
	event_loop();
	cleanup();

	return 0;
}

static void
read_config(char *rcfile)
{
	FILE *rc;
	char buf[BUF_SIZE], token[BUF_SIZE], *p;

	if (!(rc = open_rc(rcfile, "progmanrc"))) {
		if (rcfile)
			warn("rc file \"%s\" not found", rcfile);
		return;
	}

	while (get_rc_line(buf, sizeof buf, rc)) {
		p = buf;
		while (get_token(&p, token)) {
			if (strcmp(token, "xftfont") == 0) {
				if (get_token(&p, token))
					opt_xftfont = strdup(token);
			} else if (strcmp(token, "icon_xftfont") == 0) {
				if (get_token(&p, token))
					opt_icon_xftfont = strdup(token);
			} else if (strcmp(token, "fgcolor") == 0) {
				if (get_token(&p, token))
					opt_fg = strdup(token);
			} else if (strcmp(token, "bgcolor") == 0) {
				if (get_token(&p, token))
					opt_bg = strdup(token);
			} else if (strcmp(token, "fgcolor_unfocused") == 0) {
				if (get_token(&p, token))
					opt_fg_unfocused = strdup(token);
			} else if (strcmp(token, "bgcolor_unfocused") == 0) {
				if (get_token(&p, token))
					opt_bg_unfocused = strdup(token);
			} else if (strcmp(token, "button_bgcolor") == 0) {
				if (get_token(&p, token))
					opt_button_bg = strdup(token);
			} else if (strcmp(token, "bdcolor") == 0) {
				if (get_token(&p, token))
					opt_bd = strdup(token);
			} else if (strcmp(token, "bdwidth") == 0) {
				if (get_token(&p, token))
					opt_bw = atoi(token);
			} else if (strcmp(token, "padding") == 0) {
				if (get_token(&p, token))
					opt_pad = atoi(token);
			} else if (strcmp(token, "button1") == 0) {
				if (get_token(&p, token))
					opt_new[0] = strdup(token);
			} else if (strcmp(token, "button2") == 0) {
				if (get_token(&p, token))
					opt_new[1] = strdup(token);
			} else if (strcmp(token, "button3") == 0) {
				if (get_token(&p, token))
					opt_new[2] = strdup(token);
			} else if (strcmp(token, "button4") == 0) {
				if (get_token(&p, token))
					opt_new[3] = strdup(token);
			} else if (strcmp(token, "button5") == 0) {
				if (get_token(&p, token))
					opt_new[4] = strdup(token);
			} else if (strcmp(token, "edgeresist") == 0) {
				if (get_token(&p, token))
					opt_edge_resist = atoi(token);
			}
		}
	}
	fclose(rc);
}

static void
setup_display(void)
{
	XGCValues gv;
	XColor exact;
	XSetWindowAttributes sattr;
	XWindowAttributes attr;
	XIconSize *xis;
	int shape_err;
	Window qroot, qparent, *wins;
	unsigned int nwins, i;
	client_t *c;

	dpy = XOpenDisplay(NULL);
	if (!dpy)
		err(1, "can't open $DISPLAY \"%s\"", getenv("DISPLAY"));

	XSetErrorHandler(handle_xerror);
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	focused = NULL;

#ifdef USE_GDK_PIXBUF
	gdk_pixbuf_xlib_init(dpy, screen);
#endif

	map_curs = XCreateFontCursor(dpy, XC_dotbox);
	move_curs = XCreateFontCursor(dpy, XC_fleur);
	resize_n_curs = XCreateFontCursor(dpy, XC_top_side);
	resize_s_curs = XCreateFontCursor(dpy, XC_bottom_side);
	resize_e_curs = XCreateFontCursor(dpy, XC_right_side);
	resize_w_curs = XCreateFontCursor(dpy, XC_left_side);
	resize_nw_curs = XCreateFontCursor(dpy, XC_top_left_corner);
	resize_sw_curs = XCreateFontCursor(dpy, XC_bottom_left_corner);
	resize_ne_curs = XCreateFontCursor(dpy, XC_top_right_corner);
	resize_se_curs = XCreateFontCursor(dpy, XC_bottom_right_corner);

	def_cmap = DefaultColormap(dpy, screen);
	XAllocNamedColor(dpy, def_cmap, opt_fg, &fg, &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bg, &bg, &exact);
	XAllocNamedColor(dpy, def_cmap, opt_fg_unfocused, &fg_unfocused,
	    &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bg_unfocused, &bg_unfocused,
	    &exact);
	XAllocNamedColor(dpy, def_cmap, opt_button_bg, &button_bg, &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bevel_dark, &bevel_dark, &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bevel_light, &bevel_light, &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bd, &bd, &exact);

	XSetLineAttributes(dpy, DefaultGC(dpy, screen), 1, LineSolid, CapButt,
	    JoinBevel);
	XSetFillStyle(dpy, DefaultGC(dpy, screen), FillSolid);

	xft_fg.color.red = fg.red;
	xft_fg.color.green = fg.green;
	xft_fg.color.blue = fg.blue;
	xft_fg.color.alpha = 0xffff;
	xft_fg.pixel = fg.pixel;

	xft_fg_unfocused.color.red = fg_unfocused.red;
	xft_fg_unfocused.color.green = fg_unfocused.green;
	xft_fg_unfocused.color.blue = fg_unfocused.blue;
	xft_fg_unfocused.color.alpha = 0xffff;
	xft_fg_unfocused.pixel = fg_unfocused.pixel;

	xftfont = XftFontOpenName(dpy, screen, opt_xftfont);
	if (!xftfont)
		errx(1, "Xft font \"%s\" not found", opt_xftfont);

	icon_xftfont = XftFontOpenName(dpy, screen, opt_icon_xftfont);
	if (!icon_xftfont)
		errx(1, "icon Xft font \"%s\" not found", opt_icon_xftfont);

	pixmap_gc = XCreateGC(dpy, root, 0, &gv);

	gv.function = GXinvert;
	gv.subwindow_mode = IncludeInferiors;
	invert_gc = XCreateGC(dpy, root,
	    GCFunction | GCSubwindowMode | GCLineWidth, &gv);

	XpmCreatePixmapFromData(dpy, root, close_xpm, &close_pm, &close_pm_mask,
	    &close_pm_attrs);
	XpmCreatePixmapFromData(dpy, root, iconify_xpm, &iconify_pm,
	    &iconify_pm_mask, &iconify_pm_attrs);
	XpmCreatePixmapFromData(dpy, root, zoom_xpm, &zoom_pm, &zoom_pm_mask,
	    &zoom_pm_attrs);
	XpmCreatePixmapFromData(dpy, root, unzoom_xpm, &unzoom_pm,
	    &unzoom_pm_mask, &unzoom_pm_attrs);
	XpmCreatePixmapFromData(dpy, root, default_icon_xpm, &default_icon_pm,
	    &default_icon_pm_mask, &default_icon_pm_attrs);

	xis = XAllocIconSize();
	xis->min_width = ICON_SIZE;
	xis->min_height = ICON_SIZE;
	xis->max_width = ICON_SIZE;
	xis->max_height = ICON_SIZE;
	xis->width_inc = 1;
	xis->height_inc = 1;
	XSetIconSizes(dpy, root, xis, 1);
	XFree(xis);

	find_supported_atoms();

	get_atoms(root, net_num_desks, XA_CARDINAL, 0, &ndesks, 1, NULL);
	get_atoms(root, net_cur_desk, XA_CARDINAL, 0, &cur_desk, 1, NULL);

	shape = XShapeQueryExtension(dpy, &shape_event, &shape_err);

	XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
	for (i = 0; i < nwins; i++) {
		XGetWindowAttributes(dpy, wins[i], &attr);
		if (!attr.override_redirect && attr.map_state == IsViewable) {
			c = new_client(wins[i]);
			c->placed = 1;
			map_client(c);
		}
	}
	XFree(wins);

	/* become "the" window manager with SubstructureRedirectMask on root */
	sattr.event_mask = SubMask | ColormapChangeMask | ButtonMask;
	XChangeWindowAttributes(dpy, root, CWEventMask, &sattr);
}

void
sig_handler(int signum)
{
	pid_t pid;
	int status;

	switch (signum) {
	case SIGINT:
	case SIGTERM:
	case SIGHUP:
		write(exitmsg[1], &exitmsg, 1);
		break;
	case SIGCHLD:
		while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0 ||
		    (pid < 0 && errno == EINTR))
			;
		break;
	}
}

int
handle_xerror(Display * dpy, XErrorEvent * e)
{
	char msg[255];

#ifdef DEBUG
	client_t *c = find_client(e->resourceid, MATCH_WINDOW);
#endif

	if (e->error_code == BadAccess && e->resourceid == root)
		errx(1, "root window unavailable");

	XGetErrorText(dpy, e->error_code, msg, sizeof msg);
	warnx("X error (%#lx): %s", e->resourceid, msg);
#ifdef DEBUG
	if (c) {
#if 0 /* these can introduce additional xerrors so don't call them from here */
		dump_info(c);
		del_client(c, DEL_WITHDRAW);
#endif
	}
#endif

	return 0;
}

int
ignore_xerror(Display * dpy, XErrorEvent * e)
{
	return 0;
}

/*
 * We use XQueryTree here to preserve the window stacking order, since the
 * order in our linked list is different.
 */
static void
cleanup(void)
{
	unsigned int nwins, i;
	Window qroot, qparent, *wins;
	client_t *c;

	XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
	for (i = 0; i < nwins; i++) {
		c = find_client(wins[i], MATCH_FRAME);
		if (c)
			del_client(c, DEL_REMAP);
	}
	XFree(wins);

	XftFontClose(dpy, xftfont);
	XftFontClose(dpy, icon_xftfont);
	XFreeCursor(dpy, map_curs);
	XFreeCursor(dpy, move_curs);
	XFreeCursor(dpy, resize_n_curs);
	XFreeCursor(dpy, resize_s_curs);
	XFreeCursor(dpy, resize_e_curs);
	XFreeCursor(dpy, resize_w_curs);
	XFreeCursor(dpy, resize_nw_curs);
	XFreeCursor(dpy, resize_sw_curs);
	XFreeCursor(dpy, resize_ne_curs);
	XFreeCursor(dpy, resize_se_curs);
	XFreeGC(dpy, pixmap_gc);
	XFreeGC(dpy, invert_gc);
	XFreePixmap(dpy, close_pm);
	XFreePixmap(dpy, close_pm_mask);
	XFreePixmap(dpy, iconify_pm);
	XFreePixmap(dpy, iconify_pm_mask);
	XFreePixmap(dpy, zoom_pm);
	XFreePixmap(dpy, zoom_pm_mask);
	XFreePixmap(dpy, unzoom_pm);
	XFreePixmap(dpy, unzoom_pm_mask);
	XFreePixmap(dpy, default_icon_pm);
	XFreePixmap(dpy, default_icon_pm_mask);

	XInstallColormap(dpy, DefaultColormap(dpy, screen));
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);

	XDeleteProperty(dpy, root, net_supported);
	XDeleteProperty(dpy, root, net_client_list);

	XCloseDisplay(dpy);
	exit(0);
}
