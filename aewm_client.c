/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#include <stdlib.h>
#ifdef DEBUG
#include <stdio.h>
#endif
#include <string.h>
#include <X11/Xatom.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#include "aewm.h"
#include "atom.h"

static void do_map(client_t *, int);
static int init_geom(client_t *, strut_t *);
static void reparent(client_t *, strut_t *);

/*
 * Set up a client structure for the new (not-yet-mapped) window. We have to
 * ignore two unmap events if the client was already mapped but has IconicState
 * set (for instance, when we are the second window manager in a session).
 * That's because there's one for the reparent (which happens on all viewable
 * windows) and then another for the unmapping itself.
 */
client_t *
new_client(Window w)
{
	client_t *c;
	XWindowAttributes attr;
	XColor exact;
	long supplied;
	Atom win_type;

	c = malloc(sizeof *c);
	c->next = head;
	head = c;

	c->name = get_wm_name(w);
	c->win = w;
	c->frame = None;
	c->size.flags = 0;
	c->ignore_unmap = 0;
#ifdef SHAPE
	c->shaped = 0;
#endif
	c->shaded = 0;
	c->zoomed = 0;
	c->decor = 1;

	XGetWMNormalHints(dpy, c->win, &c->size, &supplied);
	XGetTransientForHint(dpy, c->win, &c->trans);

	XGetWindowAttributes(dpy, c->win, &attr);
	c->geom.x = attr.x;
	c->geom.y = attr.y;
	c->geom.w = attr.width;
	c->geom.h = attr.height;
	c->cmap = attr.colormap;
	c->old_bw = attr.border_width;

#ifdef DEBUG
	dump_name(c, "creating", 'w');
	dump_geom(c, "initial");
#endif

	XAllocNamedColor(dpy, c->cmap, opt_fg, &fg, &exact);
	XAllocNamedColor(dpy, c->cmap, opt_bg, &bg, &exact);
	XAllocNamedColor(dpy, c->cmap, opt_bd, &bd, &exact);

	if (get_atoms(c->win, net_wm_wintype, XA_ATOM, 0, &win_type, 1, NULL))
		c->decor = HAS_DECOR(win_type);

	if (get_atoms(c->win, net_wm_desk, XA_CARDINAL, 0, &c->desk, 1, NULL)) {
		if (c->desk == -1)
			c->desk = DESK_ALL;	/* FIXME */
		if (c->desk >= ndesks && c->desk != DESK_ALL)
			c->desk = cur_desk;
	} else {
		set_atoms(c->win, net_wm_desk, XA_CARDINAL, &cur_desk, 1);
		c->desk = cur_desk;
	}
#ifdef DEBUG
	dump_info(c);
#endif

	check_states(c);

	/*
	 * We are not actually keeping the stack one in order. However, every
	 * fancy panel uses it and nothing else, no matter what the spec says.
	 * (I'm not sure why, as rearranging the list every time the stacking
	 * changes would be distracting. GNOME's window list applet
	 * doesn't.)
	 */
	append_atoms(root, net_client_list, XA_WINDOW, &c->win, 1);
	append_atoms(root, net_client_stack, XA_WINDOW, &c->win, 1);

	return c;
}

client_t *
find_client(Window w, int mode)
{
	client_t *c;

	if (mode == MATCH_FRAME) {
		for (c = head; c; c = c->next)
			if (c->frame == w)
				return c;
	} else {	/* mode == MATCH_WINDOW */
		for (c = head; c; c = c->next)
			if (c->win == w)
				return c;
	}

	return NULL;
}

void
map_client(client_t *c)
{
	XWindowAttributes attr;
	strut_t s = {0, 0, 0, 0};
	XWMHints *hints;
	int btn, want_raise = 1;

	XGrabServer(dpy);

	XGetWindowAttributes(dpy, c->win, &attr);
	collect_struts(c, &s);

	if (attr.map_state == IsViewable) {
		c->ignore_unmap++;
		reparent(c, &s);
		if (get_wm_state(c->win) == IconicState) {
			c->ignore_unmap++;
			XUnmapWindow(dpy, c->win);
		} else {
			set_wm_state(c, NormalState);
			do_map(c, want_raise);
		}
	} else {
		if ((hints = XGetWMHints(dpy, c->win))) {
			if (hints->flags & StateHint)
				set_wm_state(c, hints->initial_state);
			XFree(hints);
		} else {
			set_wm_state(c, NormalState);
		}
		if (!init_geom(c, &s) && opt_imap) {
			btn = sweep(c, map_curs, recalc_map, SWEEP_DOWN, &s);
			if (btn == Button2)
				btn = sweep(c, resize_curs, recalc_resize,
				    SWEEP_UP, &s);
			if (btn == Button3)
				want_raise = 0;
		}
#ifdef DEBUG
		dump_geom(c, "set to");
		dump_info(c);
#endif
		reparent(c, &s);
		if (get_wm_state(c->win) == NormalState)
			do_map(c, want_raise);
	}

	XSync(dpy, False);
	c->name = get_wm_name(c->win);
	//horrible kludge
	    XUngrabServer(dpy);
}

/*
 * This is just a helper to perform the actual mapping, since there are two
 * different places we might need to do it.
 */
static void
do_map(client_t *c, int do_raise)
{
	if (IS_ON_CUR_DESK(c)) {
		XMapWindow(dpy, c->win);
		if (do_raise) {
			XMapRaised(dpy, c->frame);
		} else {
			XLowerWindow(dpy, c->frame);
			XMapWindow(dpy, c->frame);
		}
	}
}

/*
 * When we're ready to map, we have two things to consider: the literal
 * geometry of the window (what the client passed to XCreateWindow), and the
 * size hints (what they set with XSetWMSizeHints, if anything). Generally, the
 * client doesn't care, and leaves the literal geometry at +0+0. If the client
 * wants to be mapped in a particular place, though, they either set this
 * geometry to something different or set a size hint. The size hint is the
 * recommended method, and takes precedence. If there is already something in
 * c->geom, though, we just leave it.
 */
static int
init_geom(client_t * c, strut_t * s)
{
	Atom win_type, state;
	int screen_x = DisplayWidth(dpy, screen);
	int screen_y = DisplayHeight(dpy, screen);
	int wmax = screen_x - s->left - s->right;
	int hmax = screen_y - s->top - s->bottom;
	int mouse_x, mouse_y;

	/*
	 * We decide the geometry for these types of windows, so we can just
	 * ignore everything and return right away. If c->zoomed is set, that
	 * means we've already set things up, but otherwise, we do it here.
	 */
	if (c->zoomed)
		return 1;
	if (get_atoms(c->win, net_wm_state, XA_ATOM, 0, &state, 1, NULL) &&
	    state == net_wm_state_fs) {
		c->geom.x = 0;
		c->geom.y = 0;
		c->geom.w = screen_x;
		c->geom.h = screen_y;
		return 1;
	}
	/*
	 * Here, we merely set the values; they're in the same place regardless
	 * of whether the user or the program specified them. We'll distinguish
	 * between the two cases later, if we need to.
	 */
	if (c->size.flags & (USSize | PSize)) {
		if (c->size.width > 0)
			c->geom.w = c->size.width;
		if (c->size.height > 0)
			c->geom.h = c->size.height;
	}
	if (c->size.flags & (USPosition | PPosition)) {
		if (c->size.x > 0)
			c->geom.x = c->size.x;
		if (c->size.y > 0)
			c->geom.y = c->size.y;
	}
	/*
	 * Several types of windows can put themselves wherever they want, but
	 * we need to read the size hints to get that position before
	 * returning.
	 */
	if (get_atoms(c->win, net_wm_wintype, XA_ATOM, 0, &win_type, 1, NULL) &&
	    CAN_PLACE_SELF(win_type))
		return 1;

	/*
	 * At this point, maybe nothing was set, or something went horribly
	 * wrong and the values are garbage. So, make a guess, based on the
	 * pointer.
	 */
	if (c->geom.x <= 0 && c->geom.y <= 0) {
		get_pointer(&mouse_x, &mouse_y);
		recalc_map(c, c->geom, mouse_x, mouse_y, mouse_x, mouse_y, s);
	}

	/*
	 * In any case, if we got this far, we need to do a further sanity
	 * check and make sure that the window isn't overlapping any struts --
	 * except for transients, because they might be a panel-type client
	 * popping up a notification window over themselves.
	 */
	if (!c->trans) {
		if (c->geom.x + c->geom.w > screen_x - s->right)
			c->geom.x = screen_x - s->right - c->geom.w;
		if (c->geom.y + c->geom.h > screen_y - s->bottom)
			c->geom.y = screen_y - s->bottom - c->geom.h;
		if (c->geom.x < s->left || c->geom.w > wmax)
			c->geom.x = s->left;
		if (c->geom.y < s->top || c->geom.h > hmax)
			c->geom.y = s->top;
	}

	/*
	 * Finally, we decide if we were ultimately satisfied with the position
	 * given, or if we had to make something up, so that the caller can
	 * consider using some other method.
	 */
	return c->trans || c->size.flags & USPosition;
}

/*
 * The frame window is not created until we actually do the reparenting here,
 * and thus the Xft surface cannot exist until this runs. Anything that has to
 * manipulate the client before we are called must make sure not to attempt to
 * use either.
 */
static void
reparent(client_t *c, strut_t *s)
{
	XSetWindowAttributes pattr;
	geom_t f;

	f = frame_geom(c);
	pattr.override_redirect = True;
	pattr.background_pixel = bg.pixel;
	pattr.border_pixel = bd.pixel;
	pattr.event_mask = SubMask | ButtonPressMask | ExposureMask |
	    EnterWindowMask;
	c->frame = XCreateWindow(dpy, root, f.x, f.y, f.w, f.h, BW(c),
	    DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy,
	    screen), CWOverrideRedirect | CWBackPixel | CWBorderPixel |
	    CWEventMask, &pattr);

#ifdef SHAPE
	if (shape) {
		XShapeSelectInput(dpy, c->win, ShapeNotifyMask);
		set_shape(c);
	}
#endif

#ifdef XFT
	c->xftdraw = XftDrawCreate(dpy, (Drawable) c->frame,
	    DefaultVisual(dpy, DefaultScreen(dpy)),
	    DefaultColormap(dpy, DefaultScreen(dpy)));
#endif

	XAddToSaveSet(dpy, c->win);
	XSelectInput(dpy, c->win, ColormapChangeMask | PropertyChangeMask);
	XSetWindowBorderWidth(dpy, c->win, 0);
	XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
	XReparentWindow(dpy, c->win, c->frame, 0, frame_height(c));

	send_config(c);
}

/*
 * For a regular window, c->trans is None (false), and we include enough space
 * to draw the name. For a transient window we just make a small strip (based
 * on the font height).
 */
int
frame_height(client_t *c)
{
	if (c && c->decor)
		return (c->trans ? 0 : ASCENT) + DESCENT + 2 * opt_pad + BW(c);
	else
		return 0;
}

int
set_wm_state(client_t *c, unsigned long state)
{
	return set_atoms(c->win, wm_state, wm_state, &state, 1);
}

void
check_states(client_t *c)
{
	Atom state;
	unsigned long read, left;
	int i;

	for (i = 0, left = 1; left; i += read) {
		read = get_atoms(c->win, net_wm_state, XA_ATOM, i, &state, 1,
		    &left);
		if (read) {
			if (state == net_wm_state_shaded)
				shade_client(c);
			else if (state == net_wm_state_mh ||
			    state == net_wm_state_mv)
				zoom_client(c);
		} else {
			break;
		}
	}
}

/* If we frob the geom for some reason, we need to inform the client. */
void
send_config(client_t *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->geom.x;
	ce.y = c->geom.y;
	ce.width = c->geom.w;
	ce.height = c->geom.h;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = 0;

	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *) & ce);
}

/*
 * I've changed this to just clear the window every time. The amount of
 * "flicker" is basically imperceptable. Also, we might be drawing an
 * anti-aliased font with Xft, in which case we always have to clear to draw
 * the text properly. This allows us to simplify handle_property_change as
 * well.
 *
 * Unfortunately some fussing with pixels is always necessary. The integer
 * division here should match X's line algorithms so that proportions are
 * correct at all border widths. For text, I have subjectively chosen a
 * horizontal space of 1/2 the descender. Vertically, the decender is part of
 * the font; it is in addition to opt_pad.
 */
void
redraw_frame(client_t *c)
{
	int x, y;

	if (c && c->decor) {
		XClearWindow(dpy, c->frame);
		if (!c->shaded)
			XDrawLine(dpy, c->frame, border_gc,
			    0, frame_height(c) - BW(c) + BW(c) / 2,
			    c->geom.w, frame_height(c) - BW(c) + BW(c) / 2);
		XDrawLine(dpy, c->frame, border_gc,
		    c->geom.w - frame_height(c) + BW(c) / 2, 0,
		    c->geom.w - frame_height(c) + BW(c) / 2, frame_height(c));

		if (!c->trans && c->name) {
			x = opt_pad + DESCENT / 2;
			y = opt_pad + ASCENT;
#ifdef XFT
#ifdef X_HAVE_UTF8_STRING
			XftDrawStringUtf8(c->xftdraw, &xft_fg, xftfont, x, y,
			    (unsigned char *) c->name, strlen(c->name));
#else
			XftDrawString8(c->xftdraw, &xft_fg, xftfont, x, y,
			    (unsigned char *) c->name, strlen(c->name));
#endif
#else
#ifdef X_HAVE_UTF8_STRING
			Xutf8DrawString(dpy, c->frame, font_set, string_gc, x, y,
			    c->name, strlen(c->name));
#else
			XDrawString(dpy, c->frame, string_gc, x, y,
			    c->name, strlen(c->name));
#endif
#endif
		}
	}
}

/*
 * The frame is bigger than the client window. Which direction it extends
 * outside of the theoretical client geom is determined by the window gravity.
 * The default is NorthWest, which means that the top left corner of the frame
 * stays where the top left corner of the client window would have been, and
 * the client window moves down. For SouthEast, etc, the frame moves up. For
 * Static the client window must not move (same result as South), and for
 * Center the center point of the frame goes where the center point of the
 * unmanaged client window was.
 */
geom_t
frame_geom(client_t *c)
{
	geom_t f = c->geom;

	/* everything else is the same as c->geom was */
	f.h = frame_height(c) + (c->shaded ? -BW(c) : c->geom.h);

	/* X, in its perpetual helpfulness, always does native borders
	 * NorthWest style. This, as usual, ruins everything. So we
	 * compensate. */
	switch (GRAV(c)) {
	case NorthWestGravity:
		break;
	case NorthGravity:
		f.x -= BW(c);
		break;
	case NorthEastGravity:
		f.x -= 2 * BW(c);
		break;
	case EastGravity:
		f.x -= 2 * BW(c);
		f.y -= frame_height(c) / 2 + BW(c);
		break;
	case SouthEastGravity:
		f.x -= 2 * BW(c);
		f.y -= frame_height(c) + 2 * BW(c);
		break;
	case SouthGravity:
		f.x -= BW(c);
		f.y -= frame_height(c) + 2 * BW(c);
		break;
	case SouthWestGravity:
		f.y -= frame_height(c) + 2 * BW(c);
		break;
	case WestGravity:
		f.y -= frame_height(c) / 2 + BW(c);
		break;
	case StaticGravity:
		f.y -= frame_height(c) + BW(c);
		f.x -= BW(c);
		break;
	case CenterGravity:
		f.x -= BW(c);
		f.y -= frame_height(c) / 2 + BW(c);
		break;
	}

	return f;
}

void
collect_struts(client_t *c, strut_t *s)
{
	client_t *p;
	XWindowAttributes attr;
	strut_t temp;

	for (p = head; p; p = p->next) {
		if (!IS_ON_CUR_DESK(p) || p == c)
			continue;
		XGetWindowAttributes(dpy, p->win, &attr);
		if (attr.map_state == IsViewable && get_strut(p->win, &temp)) {
			if (temp.left > s->left)
				s->left = temp.left;
			if (temp.right > s->right)
				s->right = temp.right;
			if (temp.top > s->top)
				s->top = temp.top;
			if (temp.bottom > s->bottom)
				s->bottom = temp.bottom;
		}
	}
}

/*
 * Well, the man pages for the shape extension say nothing, but I was able to
 * find a shape.PS.Z on the x.org FTP site. What we want to do here is make the
 * window shape be a boolean OR (or union) of the client's shape and our bar
 * for the name. The bar requires both a bound and a clip because it has a
 * border; the server will paint the border in the region between the two.
 */

#ifdef SHAPE
void
set_shape(client_t *c)
{
	int n, order;
	XRectangle temp, *rects;

	rects = XShapeGetRectangles(dpy, c->win, ShapeBounding, &n, &order);

	if (n > 1) {
		XShapeCombineShape(dpy, c->frame, ShapeBounding,
		    0, frame_height(c), c->win, ShapeBounding, ShapeSet);
		temp.x = -BW(c);
		temp.y = -BW(c);
		temp.width = c->geom.w + 2 * BW(c);
		temp.height = frame_height(c) + BW(c);
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    0, 0, &temp, 1, ShapeUnion, YXBanded);
		temp.x = 0;
		temp.y = 0;
		temp.width = c->geom.w;
		temp.height = frame_height(c) - BW(c);
		XShapeCombineRectangles(dpy, c->frame, ShapeClip,
		    0, frame_height(c), &temp, 1, ShapeUnion, YXBanded);
		c->shaped = 1;
	} else if (c->shaped) {
		/* I can't find a "remove all shaping" function... */
		temp.x = -BW(c);
		temp.y = -BW(c);
		temp.width = c->geom.w + 2 * BW(c);
		temp.height = c->geom.h + frame_height(c) + 2 * BW(c);
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    0, 0, &temp, 1, ShapeSet, YXBanded);
	}
	XFree(rects);
}
#endif

/*
 * I've decided to carefully ignore any errors raised by this function, rather
 * that attempt to determine asychronously if a window is "valid". Xlib calls
 * should only fail here if that a window has removed itself completely before
 * the Unmap and Destroy events get through the queue to us. It's not pretty.
 *
 * The 'withdrawing' argument specifes if the client is actually destroying
 * itself or being destroyed by us, or if we are merely cleaning up its data
 * structures when we exit mid-session.
 */
void
del_client(client_t *c, int mode)
{
	client_t *p;

	XGrabServer(dpy);
	XSetErrorHandler(ignore_xerror);

#ifdef DEBUG
	dump_name(c, "removing", 'r');
	dump_removal(c, mode);
#endif

	if (mode == DEL_WITHDRAW) {
		set_wm_state(c, WithdrawnState);
	} else {	/* mode == DEL_REMAP */
		if (c->zoomed) {
			c->geom.x = c->save.x;
			c->geom.y = c->save.y;
			c->geom.w = c->save.w;
			c->geom.h = c->save.h;
			XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
		}
		XMapWindow(dpy, c->win);
	}

	remove_atom(root, net_client_list, XA_WINDOW, c->win);
	remove_atom(root, net_client_stack, XA_WINDOW, c->win);

	XSetWindowBorderWidth(dpy, c->win, c->old_bw);
#ifdef XFT
	if (c->xftdraw)
		XftDrawDestroy(c->xftdraw);
#endif

	XReparentWindow(dpy, c->win, root, c->geom.x, c->geom.y);
	XRemoveFromSaveSet(dpy, c->win);
	XDestroyWindow(dpy, c->frame);

	if (head == c)
		head = c->next;
	else
		for (p = head; p && p->next; p = p->next)
			if (p->next == c)
				p->next = c->next;

	if (c->name)
		XFree(c->name);
	free(c);

	XSync(dpy, False);
	XSetErrorHandler(handle_xerror);
	XUngrabServer(dpy);
}
