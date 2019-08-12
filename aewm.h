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

#ifndef AEWM_H
#define AEWM_H

#define VERSION "1.3.12"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef XFT
#include <X11/Xft/Xft.h>
#endif
#include "common.h"
#include "atom.h"

/* Default options you may want to change */

#define DEF_FONT "fixed"
#ifdef XFT
#define DEF_XFTFONT "Sans:size=8"
#endif

#define DEF_FG "white"
#define DEF_BG "slategray"
#define DEF_BD "black"
#define DEF_BW 1
#define DEF_PAD 3
#define DEF_IMAP 0

#define DEF_NEW1 "aemenu --switch"
#define DEF_NEW2 "xterm"
#define DEF_NEW3 "aemenu --launch"
#define DEF_NEW4 "aedesk -1"
#define DEF_NEW5 "aedesk +1"

/* End of options */

#define SubMask (SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask (ButtonPressMask|ButtonReleaseMask)
#define MouseMask (ButtonMask|PointerMotionMask)

#define BW(c) ((c)->decor ? opt_bw : 0)
#define GRAV(c) ((c->size.flags & PWinGravity) ? c->size.win_gravity : \
    NorthWestGravity)
#define CAN_PLACE_SELF(t) ((t) == net_wm_type_dock || \
    (t) == net_wm_type_menu || (t) == net_wm_type_splash || \
    (t) == net_wm_type_desk)
#define HAS_DECOR(t) (!CAN_PLACE_SELF(t))
#define IS_ON_CUR_DESK(c) IS_ON_DESK((c)->desk, cur_desk)

#ifdef XFT
#define ASCENT (xftfont->ascent)
#define DESCENT (xftfont->descent)
#else
#define ASCENT (font->ascent)
#define DESCENT (font->descent)
#endif

#ifdef DEBUG
#define SHOW_EV(name, memb) \
    case name: ev_type = #name; w = e.memb.window; break;
#define SHOW(name) \
    case name: return #name;
#endif

typedef struct geom geom_t;
struct geom {
	long x;
	long y;
	long w;
	long h;
};

typedef struct client client_t;
struct client {
	client_t *next;
	char *name;
	Window win, frame, trans;
	geom_t geom, save;
#ifdef XFT
	XftDraw *xftdraw;
#endif
	XSizeHints size;
	Colormap cmap;
	int ignore_unmap;
	unsigned int focus_order;
	unsigned long desk;
#ifdef SHAPE
	Bool shaped;
#endif
	Bool shaded;
	Bool zoomed;
	Bool decor;
	int old_bw;
};

typedef void sweep_func(client_t *, geom_t, int, int, int, int, strut_t *);

enum {
	MATCH_WINDOW,
	MATCH_FRAME,
	MATCH_ANY,
};	/* find_client */
enum {
	DEL_WITHDRAW,
	DEL_REMAP,
};	/* del_client */
enum {
	SWEEP_UP,
	SWEEP_DOWN,
};	/* sweep */

/* init.c */
extern client_t *head, *focused;
extern unsigned int focus_order;
extern int screen;
extern unsigned long cur_desk;
extern unsigned long ndesks;
#ifdef SHAPE
extern Bool shape;
extern int shape_event;
#endif
extern XFontStruct *font;
#ifdef X_HAVE_UTF8_STRING
extern XFontSet font_set;
#endif
#ifdef XFT
extern XftFont *xftfont;
extern XftColor xft_fg;
#endif
extern Colormap cmap;
extern XColor fg;
extern XColor bg;
extern XColor bd;
extern GC invert_gc;
extern GC string_gc;
extern GC border_gc;
extern Pixmap close_pm;
extern Pixmap minify_pm;
extern Cursor map_curs;
extern Cursor move_curs;
extern Cursor resize_curs;
extern char *opt_font;
#ifdef XFT
extern char *opt_xftfont;
#endif
extern char *opt_fg;
extern char *opt_bg;
extern char *opt_bd;
extern int opt_bw;
extern int opt_pad;
extern int opt_imap;
extern char *opt_new[];
extern void sig_handler(int signum);

/* event.c */
extern void event_loop(void);
extern int handle_xerror(Display *dpy, XErrorEvent *e);
extern int ignore_xerror(Display *dpy, XErrorEvent *e);
#ifdef DEBUG
extern void show_event(XEvent e);
#endif

/* client.c */
extern client_t *new_client(Window w);
extern client_t *find_client(Window w, int mode);
extern client_t *prev_focused(void);
extern void map_client(client_t *);
extern int frame_height(client_t *c);
extern int set_wm_state(client_t *c, unsigned long state);
extern void check_states(client_t *c);
extern void parse_state_atom(client_t *, Atom);
extern void send_config(client_t *c);
extern void redraw_frame(client_t *c);
extern geom_t frame_geom(client_t *c);
extern void collect_struts(client_t *, strut_t *);
#ifdef SHAPE
extern void set_shape(client_t *c);
#endif
extern void del_client(client_t *c, int mode);

static const unsigned char close_icon[] = {
	0b00000000,
	0b11000011,
	0b01100110,
	0b00111100,
	0b00111100,
	0b01100110,
	0b11000011,
	0b00000000,
};
static const int icon_size = sizeof(close_icon);

static const unsigned char minify_icon[] = {
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b11111111,
	0b11111111,
};

/* ui.c */
extern void user_action(client_t *c, int x, int y, int button, int down);
extern void focus_client(client_t *c);
extern void move_client(client_t *c);
extern void resize_client(client_t *c);
extern void iconify_client(client_t *c);
extern void uniconify_client(client_t *c);
extern void shade_client(client_t *c);
extern void unshade_client(client_t *c);
extern void zoom_client(client_t *c);
extern void unzoom_client(client_t *c);
extern void send_wm_delete(client_t *c);
extern void goto_desk(int new_desk);
extern void map_if_desk(client_t *c);
extern int sweep(client_t *c, Cursor curs, sweep_func cb, int mode,
    strut_t *s);
extern void recalc_map(client_t *c, geom_t orig, int x0, int y0, int x1,
    int y1, strut_t *s);
extern void recalc_move(client_t *c, geom_t orig, int x0, int y0, int x1,
    int y1, strut_t *s);
extern void recalc_resize(client_t *c, geom_t orig, int x0, int y0, int x1,
    int y1, strut_t *s);
#ifdef DEBUG
extern void dump_name(client_t *c, const char *label, char flag);
extern void dump_win(Window w, const char *label, char flag);
extern void dump_info(client_t *c);
extern void dump_geom(client_t *c, const char *label);
extern void dump_removal(client_t *c, int mode);
extern void dump_clients(void);
#endif
#endif	/* AEWM_H */
