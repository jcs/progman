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

#ifndef PROGMAN_H
#define PROGMAN_H

#define VERSION "3.11.1"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/xpm.h>
#include "common.h"
#include "atom.h"

/* Default options */

/* Title bars */
#define DEF_FG "white"
#define DEF_BG "#0000a8"
#define DEF_FG_UNFOCUSED "black"
#define DEF_BG_UNFOCUSED "white"
#define DEF_BUTTON_BG "#c0c7c8"
#define DEF_BEVEL_DARK "#87888f"
#define DEF_BEVEL_LIGHT "white"

/* Borders */
#define DEF_BD "black"

#ifdef HIDPI
#define DEF_XFTFONT "Microsoft Sans Serif:bold:size=14"
#define DEF_ICON_XFTFONT "Microsoft Sans Serif:size=11"
#define DEF_BEVEL 2
#define DEF_PAD 6
#define DEF_BW 6
#else
#define DEF_XFTFONT "system:size=13"
#define DEF_ICON_XFTFONT "Microsoft Sans Serif:size=11"
#define DEF_BEVEL 2
#define DEF_PAD 3
#define DEF_BW 3
#endif

#define DEF_NEW1 "aemenu --switch"
#define DEF_NEW2 "xterm"
#define DEF_NEW3 "aemenu --launch"
#define DEF_NEW4 "aedesk -1"
#define DEF_NEW5 "aedesk +1"

#define DOUBLE_CLICK_MSEC 250

#ifdef HIDPI
#define DEF_EDGE_RES 80
#define ICON_SIZE 64
#else
#define DEF_EDGE_RES 40
#define ICON_SIZE 32
#endif

/* End of options */

#define SubMask (SubstructureRedirectMask|SubstructureNotifyMask)
#define ButtonMask (ButtonPressMask|ButtonReleaseMask)
#define MouseMask (ButtonMask|PointerMotionMask)

#define BW(c) ((c)->decor ? (opt_bw + 2) : 0)
#define TITLEBAR_HEIGHT(c) ((c) && (c)->decor ? xftfont->ascent + \
    xftfont->descent + (2 * opt_pad) : 0)
#define GRAV(c) ((c->size.flags & PWinGravity) ? c->size.win_gravity : \
    NorthWestGravity)
#define CAN_PLACE_SELF(t) ((t) == net_wm_type_dock || \
    (t) == net_wm_type_menu || (t) == net_wm_type_splash || \
    (t) == net_wm_type_desk)
#define HAS_DECOR(t) (!CAN_PLACE_SELF(t))
#define IS_ON_CUR_DESK(c) IS_ON_DESK((c)->desk, cur_desk)
#define IS_RESIZE_WIN(c, w) (w == c->resize_nw || w == c->resize_w || \
	w == c->resize_sw || w == c->resize_s || w == c->resize_se || \
	w == c->resize_e || w == c->resize_ne || w == c->resize_n)

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

/* client_t state */
enum {
	STATE_NORMAL = 0,
	STATE_ZOOMED = (1 << 2),
	STATE_SHADED = (1 << 3),
	STATE_FULLSCREEN = (1 << 4),
	STATE_ICONIFIED = (1 << 5),
	STATE_DOCK = (1 << 6),
};

/* client_t frame_style */
enum {
	FRAME_NONE = 0,
	FRAME_BORDER = (1 << 1),
	FRAME_RESIZABLE = (1 << 2),
	FRAME_TITLEBAR = (1 << 3),
	FRAME_CLOSE = (1 << 4),
	FRAME_ICONIFY = (1 << 5),
	FRAME_ZOOM = (1 << 6),
	FRAME_ALL = FRAME_BORDER | FRAME_RESIZABLE | FRAME_TITLEBAR |
	    FRAME_CLOSE | FRAME_ICONIFY | FRAME_ZOOM,
};

typedef struct client client_t;
struct client {
	client_t *next;
	char *name;
	XftDraw *xftdraw;
	Window win, trans;
	geom_t geom, save;
	Window frame;
	geom_t frame_geom;
	unsigned int frame_style;
	Window close;
	geom_t close_geom;
	Bool close_pressed;
	Window titlebar;
	geom_t titlebar_geom;
	Window iconify;
	geom_t iconify_geom;
	Bool iconify_pressed;
	Window zoom;
	geom_t zoom_geom;
	Bool zoom_pressed;
	Window resize_nw;
	geom_t resize_nw_geom;
	Window resize_n;
	geom_t resize_n_geom;
	Window resize_ne;
	geom_t resize_ne_geom;
	Window resize_e;
	geom_t resize_e_geom;
	Window resize_se;
	geom_t resize_se_geom;
	Window resize_s;
	geom_t resize_s_geom;
	Window resize_sw;
	geom_t resize_sw_geom;
	Window resize_w;
	geom_t resize_w_geom;
	Window icon;
	geom_t icon_geom;
	Window icon_label;
	geom_t icon_label_geom;
	Pixmap icon_pixmap;
	Pixmap icon_mask;
	GC icon_gc;
	char *icon_name;
	XftDraw *icon_xftdraw;
	int icon_depth;
	XSizeHints size;
	Colormap cmap;
	int ignore_unmap;
	unsigned long desk;
	Bool placed;
	Bool shaped;
	int state;
	Bool decor;
	Atom win_type;
	int old_bw;
};

typedef struct xft_line xft_line_t;
struct xft_line_t {
	char *str;
	unsigned int len;
	unsigned int xft_width;
};

typedef void sweep_func(client_t *, geom_t, int, int, int, int, strut_t *,
    void *);

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
	ORDER_TOP,
	ORDER_ICONIFIED_TOP,
	ORDER_BOTTOM,
	ORDER_OUT,
	ORDER_INVERT,
};	/* adjust_client_order */

enum {
	FOCUS_NORMAL,
	FOCUS_FORCE,
};	/* focus_client */

/* progman.c */
extern client_t *focused;
extern int screen;
extern unsigned long cur_desk;
extern unsigned long ndesks;
extern Bool shape;
extern int shape_event;
extern Window supporting_wm_win;
extern XftFont *xftfont;
extern XftFont *icon_xftfont;
extern XftColor xft_fg;
extern XftColor xft_fg_unfocused;
extern Colormap cmap;
extern XColor fg;
extern XColor bg;
extern XColor fg_unfocused;
extern XColor bg_unfocused;
extern XColor button_bg;
extern XColor bevel_dark;
extern XColor bevel_light;
extern XColor bd;
extern GC pixmap_gc;
extern GC invert_gc;
extern Pixmap close_pm;
extern Pixmap close_pm_mask;
extern XpmAttributes close_pm_attrs;
extern Pixmap iconify_pm;
extern Pixmap iconify_pm_mask;
extern XpmAttributes iconify_pm_attrs;
extern Pixmap zoom_pm;
extern Pixmap zoom_pm_mask;
extern XpmAttributes zoom_pm_attrs;
extern Pixmap unzoom_pm;
extern Pixmap unzoom_pm_mask;
extern XpmAttributes unzoom_pm_attrs;
extern Pixmap default_icon_pm;
extern Pixmap default_icon_pm_mask;
extern XpmAttributes default_icon_pm_attrs;
extern Cursor map_curs;
extern Cursor move_curs;
extern Cursor resize_n_curs;
extern Cursor resize_s_curs;
extern Cursor resize_e_curs;
extern Cursor resize_w_curs;
extern Cursor resize_nw_curs;
extern Cursor resize_sw_curs;
extern Cursor resize_ne_curs;
extern Cursor resize_se_curs;
extern char *opt_xftfont;
extern char *opt_fg;
extern char *opt_bg;
extern char *opt_fg_unfocused;
extern char *opt_bg_unfocused;
extern char *opt_button_bg;
extern int opt_bevel;
extern char *opt_bevel_dark;
extern char *opt_bevel_light;
extern char *opt_bd;
extern int opt_bw;
extern int opt_pad;
extern char *opt_new[];
extern int opt_edge_resist;
extern void sig_handler(int signum);
extern int exitmsg[2];

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
extern client_t *find_client_at_coords(Window w, int x, int y);
extern client_t *top_client(void);
extern client_t *prev_focused(int);
extern void map_client(client_t *);
extern void recalc_frame(client_t *c);
extern int set_wm_state(client_t *c, unsigned long state);
extern void check_states(client_t *c);
extern void parse_state_atom(client_t *, Atom);
extern void send_config(client_t *c);
extern void redraw_frame(client_t *c, Window);
extern void collect_struts(client_t *, strut_t *);
extern void get_client_icon(client_t *);
extern void redraw_icon(client_t *c, Window win);
extern void set_shape(client_t *c);
extern void del_client(client_t *c, int mode);

/* manage.c */
extern void user_action(client_t *c, Window win, int x, int y, int button,
    int down);
extern int pos_in_frame(client_t *c, int x, int y);
extern Cursor cursor_for_resize_win(client_t *c, Window win);
extern void focus_client(client_t *c, int);
extern void move_client(client_t *c);
extern void resize_client(client_t *c, Window resize_pos);
extern void iconify_client(client_t *c);
extern void uniconify_client(client_t *c);
extern void place_icon(client_t *c);
extern void shade_client(client_t *c);
extern void unshade_client(client_t *c);
extern void fullscreen_client(client_t *c);
extern void unfullscreen_client(client_t *c);
extern void zoom_client(client_t *c);
extern void unzoom_client(client_t *c);
extern void send_wm_delete(client_t *c);
extern void goto_desk(int new_desk);
extern void map_if_desk(client_t *c);
extern void sweep(client_t *c, Cursor curs, sweep_func cb, void *cb_arg,
    strut_t *s);
extern void recalc_map(client_t *c, geom_t orig, int x0, int y0, int x1,
    int y1, strut_t *s, void *arg);
extern void recalc_move(client_t *c, geom_t orig, int x0, int y0, int x1,
    int y1, strut_t *s, void *arg);
extern void recalc_resize(client_t *c, geom_t orig, int x0, int y0, int x1,
    int y1, strut_t *s, void *arg);
extern void fix_size(client_t *);
extern void constrain_frame(client_t *);
extern char *state_name(client_t *c);
extern void flush_expose_client(client_t *c);
extern void flush_expose(Window win);
extern int overlapping_geom(geom_t a, geom_t b);
extern void adjust_client_order(client_t *, int);
#ifdef DEBUG
extern void dump_name(client_t *c, const char *label, const char *detail,
    const char *name);
extern void dump_info(client_t *c);
extern void dump_geom(client_t *c, geom_t g, const char *label);
extern void dump_removal(client_t *c, int mode);
extern void dump_clients(void);
extern const char *frame_name(client_t *c, Window w);
#endif

/* keyboard.c */
extern void bind_keys(void);
extern KeySym lookup_keysym(XKeyEvent *e);
extern void handle_key_event(XKeyEvent *e);

#endif	/* PROGMAN_H */
