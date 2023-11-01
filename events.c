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
#include <poll.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include "progman.h"
#include "atom.h"

#ifndef INFTIM
#define INFTIM (-1)
#endif

static void handle_button_press(XButtonEvent *);
static void handle_button_release(XButtonEvent *);
static void handle_configure_request(XConfigureRequestEvent *);
static void handle_circulate_request(XCirculateRequestEvent *);
static void handle_map_request(XMapRequestEvent *);
static void handle_destroy_event(XDestroyWindowEvent *);
static void handle_client_message(XClientMessageEvent *);
static void handle_property_change(XPropertyEvent *);
static void handle_enter_event(XCrossingEvent *);
static void handle_cmap_change(XColormapEvent *);
static void handle_expose_event(XExposeEvent *);
static void handle_shape_change(XShapeEvent *);

static XEvent ev;

void
event_loop(void)
{
	struct pollfd pfd[2];

	memset(&pfd, 0, sizeof(pfd));
	pfd[0].fd = ConnectionNumber(dpy);
	pfd[0].events = POLLIN;
	pfd[1].fd = exitmsg[0];
	pfd[1].events = POLLIN;

	for (;;) {
		if (!XPending(dpy)) {
			poll(pfd, 2, INFTIM);
			if (pfd[1].revents)
				/* exitmsg */
				break;

			if (!XPending(dpy))
				continue;
		}

		XNextEvent(dpy, &ev);
#ifdef DEBUG
		show_event(ev);
#endif
		switch (ev.type) {
		case ButtonPress:
			handle_button_press(&ev.xbutton);
			break;
		case ButtonRelease:
			handle_button_release(&ev.xbutton);
			break;
		case ConfigureRequest:
			handle_configure_request(&ev.xconfigurerequest);
			break;
		case CirculateRequest:
			handle_circulate_request(&ev.xcirculaterequest);
			break;
		case MapRequest:
			handle_map_request(&ev.xmaprequest);
			break;
		case UnmapNotify:
			handle_unmap_event(&ev.xunmap);
			break;
		case DestroyNotify:
			handle_destroy_event(&ev.xdestroywindow);
			break;
		case ClientMessage:
			handle_client_message(&ev.xclient);
			break;
		case ColormapNotify:
			handle_cmap_change(&ev.xcolormap);
			break;
		case PropertyNotify:
			handle_property_change(&ev.xproperty);
			break;
		case EnterNotify:
			handle_enter_event(&ev.xcrossing);
			break;
		case Expose:
			handle_expose_event(&ev.xexpose);
			break;
		case KeyPress:
		case KeyRelease:
			handle_key_event(&ev.xkey);
			break;
		default:
			if (shape_support && ev.type == shape_event)
				handle_shape_change((XShapeEvent *)&ev);
		}
	}
}

/*
 * Someone clicked a button. If they clicked on a window, we want the button
 * press, but if they clicked on the root, we're only interested in the button
 * release. Thus, two functions.
 *
 * If it was on the root, we get the click by default. If it's on a window
 * frame, we get it as well.
 *
 * If it's on a client window, it may still fall through to us if the client
 * doesn't select for mouse-click events. The upshot of this is that you should
 * be able to click on the blank part of a GTK window with Button2 to move
 * it.
 */
static void
handle_button_press(XButtonEvent *e)
{
	client_t *c = find_client(e->window, MATCH_ANY);
	int i;

	if (e->window == root) {
		client_t *fc;
		/*
		 * Clicking inside transparent icons may fall through to the
		 * root, so check for an iconified client here
		 */
		if ((fc = find_client_at_coords(e->window, e->x, e->y)) &&
		    (fc->state & STATE_ICONIFIED)) {
			c = fc;
			e->window = c->icon;
		}
	}

	if (c && (c->state & STATE_DOCK)) {
		/* pass button event through */
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
	} else if (c) {
		if (opt_drag_button && e->button == opt_drag_button &&
		    (e->state & opt_drag_mod) &&
		    !(c->state & (STATE_FULLSCREEN | STATE_ZOOMED |
		    STATE_ICONIFIED))) {
			/* alt+click, begin moving */
			focus_client(c, FOCUS_NORMAL);
			move_client(c);
		} else if (find_client(e->window, MATCH_FRAME)) {
			/* raising our frame will also raise the window */
			focus_client(c, FOCUS_NORMAL);
			user_action(c, e->window, e->x, e->y, e->button, 1);
		} else {
			if (e->button == 1)
				focus_client(c, FOCUS_NORMAL);

			/* pass button event through */
			XAllowEvents(dpy, ReplayPointer, CurrentTime);
		}
	} else if (e->window == root) {
		for (i = 0; i < nkey_actions; i++) {
			if (key_actions[i].type == BINDING_TYPE_DESKTOP &&
			    key_actions[i].mod == e->state &&
			    key_actions[i].button == e->button) {
				take_action(&key_actions[i]);
				break;
			}
		}
	}
}

static void
handle_button_release(XButtonEvent *e)
{
	client_t *c = find_client(e->window, MATCH_ANY);

	if (e->window == root) {
		client_t *fc;
		if ((fc = find_client_at_coords(e->window, e->x, e->y)) &&
		    (fc->state & STATE_ICONIFIED)) {
			c = fc;
			e->window = c->icon;
		}
	}

	if (c) {
		if (find_client(e->window, MATCH_FRAME))
			user_action(c, e->window, e->x, e->y, e->button, 0);

		XAllowEvents(dpy, ReplayPointer, CurrentTime);
	}
}

/*
 * Because we are redirecting the root window, we get ConfigureRequest events
 * from both clients we're handling and ones that we aren't. For clients we
 * manage, we need to adjust the frame and the client window, and for unmanaged
 * windows we have to pass along everything unchanged.
 *
 * Most of the assignments here are going to be garbage, but only the ones that
 * are masked in by e->value_mask will be looked at by the X server.
 */
static void
handle_configure_request(XConfigureRequestEvent *e)
{
	client_t *c = NULL;
	XWindowChanges wc;

	if ((c = find_client(e->window, MATCH_WINDOW))) {
		recalc_frame(c);

		if (e->value_mask & CWX)
			c->geom.x = e->x + (c->geom.x - c->frame_geom.x);
		if (e->value_mask & CWY)
			c->geom.y = e->y + (c->geom.y - c->frame_geom.y);
		if (e->value_mask & CWWidth)
			c->geom.w = e->width;
		if (e->value_mask & CWHeight)
			c->geom.h = e->height;

		constrain_frame(c);

		wc.x = c->frame_geom.x;
		wc.y = c->frame_geom.y;
		wc.width = c->frame_geom.w;
		wc.height = c->frame_geom.h;
		wc.border_width = 0;
		wc.sibling = e->above;
		wc.stack_mode = e->detail;
#ifdef DEBUG
		dump_geom(c, c->frame_geom, "moving frame to");
#endif
		XConfigureWindow(dpy, c->frame, e->value_mask, &wc);
		if (e->value_mask & (CWWidth | CWHeight))
			set_shape(c);
		if ((c->state & STATE_ZOOMED) &&
		    (e->value_mask & (CWX | CWY | CWWidth | CWHeight))) {
#ifdef DEBUG
			dump_name(c, __func__, NULL,
			    "unzooming from XConfigureRequest");
#endif
			unzoom_client(c);
		} else {
			redraw_frame(c, None);
			send_config(c);
		}
	}

	if (c) {
		wc.x = c->geom.x - c->frame_geom.x;
		wc.y = c->geom.y - c->frame_geom.y;
		wc.width = c->geom.w;
		wc.height = c->geom.h;
	} else {
		wc.x = e->x;
		wc.y = e->y;
		wc.width = e->width;
		wc.height = e->height;
	}
	wc.sibling = e->above;
	wc.stack_mode = e->detail;
	XConfigureWindow(dpy, e->window, e->value_mask, &wc);

	/* top client may not be the focused one now */
	if ((c = top_client()) && IS_ON_CUR_DESK(c))
		focus_client(c, FOCUS_FORCE);
}

/*
 * The only window that we will circulate children for is the root (because
 * nothing else would make sense). After a client requests that the root's
 * children be circulated, the server will determine which window needs to be
 * raised or lowered, and so all we have to do is make it so.
 */
static void
handle_circulate_request(XCirculateRequestEvent *e)
{
	client_t *c;

	if (e->parent == root) {
		c = find_client(e->window, MATCH_ANY);

		if (e->place == PlaceOnBottom) {
			if (c) {
				adjust_client_order(c, ORDER_BOTTOM);
				if (focused)
					focus_client(focused, FOCUS_FORCE);
			} else
				XLowerWindow(dpy, e->window);
		} else {
			if (c && IS_ON_CUR_DESK(c))
				focus_client(c, FOCUS_FORCE);
			else if (c)
				adjust_client_order(c, ORDER_TOP);
			else
				XRaiseWindow(dpy, e->window);
		}
	}
}

/*
 * Two possibilities if a client is asking to be mapped. One is that it's a new
 * window, so we handle that if it isn't in our clients list anywhere. The
 * other is that it already exists and wants to de-iconify, which is simple to
 * take care of. Since we iconify all of a window's transients when iconifying
 * that window, de-iconify them here.
 */
static void
handle_map_request(XMapRequestEvent *e)
{
	client_t *c, *p;

	if ((c = find_client(e->window, MATCH_WINDOW))) {
		uniconify_client(c);
		for (p = focused; p; p = p->next)
			if (p->trans == c->win)
				uniconify_client(p);
	} else {
		c = new_client(e->window);
		map_client(c);
	}
}

/*
 * We don't get to intercept Unmap events, so this is post mortem. If we caused
 * the unmap ourselves earlier (explictly or by remapping), we will have
 * incremented c->ignore_unmap. If not, time to destroy the client.
 *
 * Because most clients unmap and destroy themselves at once, they're gone
 * before we even get the Unmap event, never mind the Destroy one. Therefore we
 * must be extra careful in del_client.
 */
void
handle_unmap_event(XUnmapEvent *e)
{
	client_t *c;

	if ((c = find_client(e->window, MATCH_WINDOW))) {
		if (c->ignore_unmap)
			c->ignore_unmap--;
		else
			del_client(c, DEL_WITHDRAW);
	}
}

/*
 * But a window can also go away when it's not mapped, in which case there is
 * no Unmap event.
 */
static void
handle_destroy_event(XDestroyWindowEvent *e)
{
	client_t *c;

	if ((c = find_client(e->window, MATCH_WINDOW)))
		del_client(c, DEL_WITHDRAW);
}

static void
handle_client_message(XClientMessageEvent *e)
{
	client_t *c;

	if (e->window == root) {
		if (e->message_type == net_cur_desk && e->format == 32)
			goto_desk(e->data.l[0]);
		else if (e->message_type == net_num_desks && e->format == 32) {
			if (e->data.l[0] < ndesks)
				/* TODO: move clients from deleted desks */
				return;
			ndesks = e->data.l[0];
		}
		return;
	}

	c = find_client(e->window, MATCH_WINDOW);
	if (!c)
		return;
	if (e->format != 32)
		return;

	if (e->message_type == wm_change_state && e->data.l[0] == IconicState)
		iconify_client(c);
	else if (e->message_type == net_close_window)
		send_wm_delete(c);
	else if (e->message_type == net_active_window) {
		c->desk = cur_desk;
		map_if_desk(c);
		if (c->state == STATE_ICONIFIED)
			uniconify_client(c);
		focus_client(c, FOCUS_NORMAL);
	} else if (e->message_type == net_wm_state &&
	    e->data.l[1] == net_wm_state_fs) {
		if (e->data.l[0] == net_wm_state_add ||
		    (e->data.l[0] == net_wm_state_toggle &&
		    c->state != STATE_FULLSCREEN))
			fullscreen_client(c);
		else
			unfullscreen_client(c);
	}
}

/*
 * If we have something copied to a variable, or displayed on the screen, make
 * sure it is up to date. If redrawing the name is necessary, clear the window
 * because Xft uses alpha rendering.
 */
static void
handle_property_change(XPropertyEvent *e)
{
	client_t *c;
#ifdef DEBUG
	char *atom;
#endif

	if (!(c = find_client(e->window, MATCH_WINDOW)))
		return;

#ifdef DEBUG
	atom = XGetAtomName(dpy, e->atom);
	dump_name(c, __func__, "", atom);
	XFree(atom);
#endif

	if (e->atom == XA_WM_NAME || e->atom == net_wm_name) {
		if (c->name)
			XFree(c->name);
		c->name = get_wm_name(c->win);
		if (c->frame_style & FRAME_TITLEBAR)
			redraw_frame(c, c->titlebar);
	} else if (e->atom == XA_WM_ICON_NAME || e->atom == net_wm_icon_name) {
		if (c->icon_name)
			XFree(c->icon_name);
		c->icon_name = get_wm_icon_name(c->win);
		if (c->state & STATE_ICONIFIED)
			redraw_icon(c, c->icon_label);
	} else if (e->atom == XA_WM_NORMAL_HINTS) {
		update_size_hints(c);
		fix_size(c);
		redraw_frame(c, None);
		send_config(c);
	} else if (e->atom == XA_WM_HINTS) {
		if (c->wm_hints)
			XFree(c->wm_hints);
		c->wm_hints = XGetWMHints(dpy, c->win);
		if (c->wm_hints &&
		    c->wm_hints->flags & (IconPixmapHint | IconMaskHint)) {
			get_client_icon(c);
			if (c->state & STATE_ICONIFIED)
				redraw_icon(c, c->icon);
		}
	} else if (e->atom == net_wm_state || e->atom == wm_state) {
		int was_state = c->state;
		check_states(c);
		if (was_state != c->state) {
			if (c->state & STATE_ICONIFIED)
				iconify_client(c);
			else if (c->state & STATE_ZOOMED)
				zoom_client(c);
			else if (c->state & STATE_FULLSCREEN)
				fullscreen_client(c);
			else {
				if (was_state & STATE_ZOOMED)
					unzoom_client(c);
				else if (was_state & STATE_ICONIFIED)
					uniconify_client(c);
				else if (was_state & STATE_FULLSCREEN)
					unfullscreen_client(c);
			}
		}
	} else if (e->atom == net_wm_desk) {
		if (get_atoms(c->win, net_wm_desk, XA_CARDINAL, 0,
			&c->desk, 1, NULL)) {
			if (c->desk == -1)
				c->desk = DESK_ALL;	/* FIXME */
			map_if_desk(c);
		}
	}
#ifdef DEBUG
	else {
		printf("%s: unknown atom %ld (%s)\n", __func__, (long)e->atom,
		    XGetAtomName(dpy, e->atom));
	}
#endif
}

/* Support click-to-focus policy. */
static void
handle_enter_event(XCrossingEvent *e)
{
	client_t *c;

	if ((c = find_client(e->window, MATCH_FRAME)))
		XGrabButton(dpy, Button1, AnyModifier, c->win, False,
		    ButtonMask, GrabModeSync, GrabModeSync, None, None);
}

/*
 * Colormap policy: when a client installs a new colormap on itself, set the
 * display's colormap to that. We do this even if it's not focused.
 */
static void
handle_cmap_change(XColormapEvent *e)
{
	client_t *c;

	if ((c = find_client(e->window, MATCH_WINDOW)) && e->new) {
		c->cmap = e->colormap;
		XInstallColormap(dpy, c->cmap);
	}
}

/*
 * If we were covered by multiple windows, we will usually get multiple expose
 * events, so ignore them unless e->count (the number of outstanding exposes)
 * is zero.
 */
static void
handle_expose_event(XExposeEvent *e)
{
	client_t *c;

	if ((c = find_client(e->window, MATCH_FRAME)) && e->count == 0)
		redraw_frame(c, e->window);
}

static void
handle_shape_change(XShapeEvent *e)
{
	client_t *c;

	if ((c = find_client(e->window, MATCH_WINDOW)))
		set_shape(c);
}

#ifdef DEBUG
void
show_event(XEvent e)
{
	char ev_type[128];
	Window w;
	client_t *c;

	switch (e.type) {
	SHOW_EV(ButtonPress, xbutton)
	SHOW_EV(ButtonRelease, xbutton)
	SHOW_EV(ClientMessage, xclient)
	SHOW_EV(ColormapNotify, xcolormap)
	SHOW_EV(ConfigureNotify, xconfigure)
	SHOW_EV(ConfigureRequest, xconfigurerequest)
	SHOW_EV(CirculateRequest, xcirculaterequest)
	SHOW_EV(CreateNotify, xcreatewindow)
	SHOW_EV(DestroyNotify, xdestroywindow)
	SHOW_EV(EnterNotify, xcrossing)
	SHOW_EV(Expose, xexpose)
	SHOW_EV(KeyPress, xkey)
	SHOW_EV(KeyRelease, xkey)
	SHOW_EV(NoExpose, xexpose)
	SHOW_EV(MapNotify, xmap)
	SHOW_EV(MapRequest, xmaprequest)
	SHOW_EV(MappingNotify, xmapping)
	SHOW_EV(PropertyNotify, xproperty)
	SHOW_EV(ReparentNotify, xreparent)
	SHOW_EV(ResizeRequest, xresizerequest)
	SHOW_EV(UnmapNotify, xunmap)
	SHOW_EV(MotionNotify, xmotion)
	default:
		if (shape_support && e.type == shape_event) {
			snprintf(ev_type, sizeof(ev_type), "ShapeNotify");
			w = ((XShapeEvent *) & e)->window;
			break;
		}
		snprintf(ev_type, sizeof(ev_type), "unknown event %d", e.type);
		w = None;
		break;
	}

	if ((c = find_client(w, MATCH_WINDOW)))
		dump_name(c, ev_type, "window", c->name);
	else if ((c = find_client(w, MATCH_FRAME))) {
		/*
		 * ConfigureNotify can only come from us (otherwise it'd be a
		 * ConfigureRequest) and NoExpose events are just not useful
		 */
		if (e.type != ConfigureNotify && e.type != NoExpose)
			dump_name(c, ev_type, frame_name(c, w), c->name);
	} else if (w == root)
		dump_name(NULL, ev_type, "root", "(root)");
}
#endif
