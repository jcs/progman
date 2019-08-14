/*
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
#include <fcntl.h>
#include <sys/wait.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#include "aewm.h"
#include "atom.h"
#include "parser.h"

client_t *head, *focused;
int screen;
unsigned long ndesks = 1;
unsigned long cur_desk = 0;
unsigned int focus_order = 0;
#ifdef SHAPE
Bool shape;
int shape_event;
#endif

XftFont *xftfont;
XftColor xft_fg;
XftColor xft_fg_unfocused;

Colormap def_cmap;
XColor fg;
XColor bg;
XColor fg_unfocused;
XColor bg_unfocused;
XColor bd;
GC invert_gc;
GC string_unfocused_gc;
GC string_gc;
GC border_gc;
Pixmap close_pm;
Pixmap minify_pm;
Cursor map_curs;
Cursor move_curs;
Cursor resize_curs;

int exitmsg[2];

char *opt_xftfont = DEF_XFTFONT;
char *opt_fg = DEF_FG;
char *opt_bg = DEF_BG;
char *opt_fg_unfocused = DEF_FG_UNFOCUSED;
char *opt_bg_unfocused = DEF_BG_UNFOCUSED;
char *opt_bd = DEF_BD;
int opt_bw = DEF_BW;
int opt_pad = DEF_PAD;
char *opt_new[] = { DEF_NEW1, DEF_NEW2, DEF_NEW3, DEF_NEW4, DEF_NEW5 };

static void cleanup(void);
static void read_config(char *);
static void setup_display(void);

extern char *__progname;

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
			printf("usage: %s [-c <config file>]\n", __progname);
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

	if (!(rc = open_rc(rcfile, "aewmrc"))) {
		if (rcfile)
			fprintf(stderr, "%s: rc file '%s' not found\n",
			    __progname, rcfile);
		return;
	}

	while (get_rc_line(buf, sizeof buf, rc)) {
		p = buf;
		while (get_token(&p, token)) {
			if (strcmp(token, "xftfont") == 0) {
				if (get_token(&p, token))
					opt_xftfont = strdup(token);
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
#ifdef SHAPE
	int shape_err;
#endif
	Window qroot, qparent, *wins;
	unsigned int nwins, i;
	client_t *c;

	dpy = XOpenDisplay(NULL);

	if (!dpy) {
		fprintf(stderr, "%s: can't open $DISPLAY \"%s\"\n", __progname,
		    getenv("DISPLAY"));
		exit(1);
	}

	XSetErrorHandler(handle_xerror);
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	map_curs = XCreateFontCursor(dpy, XC_dotbox);
	move_curs = XCreateFontCursor(dpy, XC_fleur);
	resize_curs = XCreateFontCursor(dpy, XC_sizing);

	def_cmap = DefaultColormap(dpy, screen);
	XAllocNamedColor(dpy, def_cmap, opt_fg, &fg, &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bg, &bg, &exact);
	XAllocNamedColor(dpy, def_cmap, opt_fg_unfocused, &fg_unfocused,
	    &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bg_unfocused, &bg_unfocused,
	    &exact);
	XAllocNamedColor(dpy, def_cmap, opt_bd, &bd, &exact);

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

	xftfont = XftFontOpenName(dpy, DefaultScreen(dpy), opt_xftfont);
	if (!xftfont) {
		fprintf(stderr, "%s: Xft font \"%s\" not found\n", __progname,
		    opt_xftfont);
		exit(1);
	}

	gv.function = GXcopy;
	gv.foreground = fg.pixel;
	string_gc = XCreateGC(dpy, root, GCFunction | GCForeground, &gv);

	gv.foreground = fg_unfocused.pixel;
	string_unfocused_gc = XCreateGC(dpy, root, GCFunction | GCForeground,
	    &gv);

	gv.foreground = bd.pixel;
	gv.line_width = opt_bw;
	border_gc = XCreateGC(dpy, root,
	    GCFunction | GCForeground | GCLineWidth, &gv);

	gv.function = GXinvert;
	gv.subwindow_mode = IncludeInferiors;
	invert_gc = XCreateGC(dpy, root,
	    GCFunction | GCSubwindowMode | GCLineWidth, &gv);

	close_pm = XCreateBitmapFromData(dpy, root, close_icon,
	    sizeof(close_icon), sizeof(close_icon));
	minify_pm = XCreateBitmapFromData(dpy, root, minify_icon,
	    sizeof(minify_icon), sizeof(minify_icon));

	utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
	wm_protos = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wm_state = XInternAtom(dpy, "WM_STATE", False);
	wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
	net_cur_desk = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	net_num_desks = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	net_client_stack = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
	net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	net_close_window = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
	net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
	net_wm_desk = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	net_wm_state_shaded = XInternAtom(dpy, "_NET_WM_STATE_SHADED", False);
	net_wm_state_mv = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT",
	    False);
	net_wm_state_mh = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ",
	    False);
	net_wm_state_fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
	net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
	net_wm_wintype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	net_wm_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	net_wm_type_menu = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
	net_wm_type_splash = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH",
	    False);
	net_wm_type_desk = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP",
	    False);

	/* The bit about _NET_CLIENT_LIST_STACKING here is an evil lie. */
	append_atoms(root, net_supported, XA_ATOM, &net_cur_desk, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_num_desks, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_client_list, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_client_stack, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_active_window, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_close_window, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_name, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_desk, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_shaded, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_mv, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_mh, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_fs, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_strut, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_strut_partial, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_wintype, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_dock, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_menu, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_splash, 1);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_desk, 1);

	get_atoms(root, net_num_desks, XA_CARDINAL, 0, &ndesks, 1, NULL);
	get_atoms(root, net_cur_desk, XA_CARDINAL, 0, &cur_desk, 1, NULL);

#ifdef SHAPE
	shape = XShapeQueryExtension(dpy, &shape_event, &shape_err);
#endif

	XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
	for (i = 0; i < nwins; i++) {
		XGetWindowAttributes(dpy, wins[i], &attr);
		if (!attr.override_redirect && attr.map_state == IsViewable) {
			c = new_client(wins[i]);
			map_client(c);
		}
	}
	XFree(wins);

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

	if (e->error_code == BadAccess && e->resourceid == root) {
		fprintf(stderr, "%s: root window unavailable\n", __progname);
		exit(1);
	}

	XGetErrorText(dpy, e->error_code, msg, sizeof msg);
	fprintf(stderr, "%s: X error (%#lx): %s\n", __progname, e->resourceid,
	    msg);
#ifdef DEBUG
	if (c) {
		dump_info(c);
		del_client(c, DEL_WITHDRAW);
	}
#endif
	return 0;
}

/* Ick. Argh. You didn't see this function. */
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
	XFreeCursor(dpy, map_curs);
	XFreeCursor(dpy, move_curs);
	XFreeCursor(dpy, resize_curs);
	XFreeGC(dpy, invert_gc);
	XFreeGC(dpy, border_gc);
	XFreeGC(dpy, string_gc);
	XFreePixmap(dpy, close_pm);
	XFreePixmap(dpy, minify_pm);

	XInstallColormap(dpy, DefaultColormap(dpy, screen));
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);

	XDeleteProperty(dpy, root, net_supported);
	XDeleteProperty(dpy, root, net_client_list);

	XCloseDisplay(dpy);
	exit(0);
}
