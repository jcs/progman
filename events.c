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
#include <poll.h>
#include <X11/Xatom.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#include "progman.h"
#include "atom.h"

static void handle_button_press(XButtonEvent *);
static void handle_button_release(XButtonEvent *);
static void handle_configure_request(XConfigureRequestEvent *);
static void handle_circulate_request(XCirculateRequestEvent *);
static void handle_map_request(XMapRequestEvent *);
static void handle_unmap_event(XUnmapEvent *);
static void handle_destroy_event(XDestroyWindowEvent *);
static void handle_client_message(XClientMessageEvent *);
static void handle_property_change(XPropertyEvent *);
static void handle_enter_event(XCrossingEvent *);
static void handle_cmap_change(XColormapEvent *);
static void handle_expose_event(XExposeEvent *);
#ifdef SHAPE
static void handle_shape_change(XShapeEvent *);
#endif

static int root_button_pressed = 0;

/* TWM has an interesting and different way of doing this. We might also want
 * to respond to unknown events. */
void
event_loop(void)
{
	XEvent ev;
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
#ifdef SHAPE
		default:
			if (shape && ev.type == shape_event)
				handle_shape_change((XShapeEvent *) & ev);
#endif
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

	if (e->window == root)
		root_button_pressed = 1;
	else if (c) {
		focus_client(c);

		if (e->button == 1 && e->state & Mod1Mask) {
			/* alt+click, begin moving */
			XRaiseWindow(dpy, c->frame);
			move_client(c);
		} else if (find_client(e->window, MATCH_FRAME)) {
			/* raising our frame will also raise the window */
			XRaiseWindow(dpy, c->frame);
			user_action(c, e->window, e->x, e->y, e->button, 1);
		} else {
			if (e->button == 1)
				XRaiseWindow(dpy, c->frame);

			/* pass button event through */
			XAllowEvents(dpy, ReplayPointer, CurrentTime);
		}
	}
}

static void
handle_button_release(XButtonEvent *e)
{
	client_t *c = find_client(e->window, MATCH_ANY);

	if (e->window == root && root_button_pressed) {
#ifdef DEBUG
		dump_clients();
#endif
		switch (e->button) {
		case Button1:
			fork_exec(opt_new[0]);
			break;
		case Button2:
			fork_exec(opt_new[1]);
			break;
		case Button3:
			fork_exec(opt_new[2]);
			break;
		case Button4:
			fork_exec(opt_new[3]);
			break;
		case Button5:
			fork_exec(opt_new[4]);
			break;
		}

		root_button_pressed = 0;
	} else if (c) {
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
			c->geom.x = e->x +
			    (c->decor ? c->geom.x - c->frame_geom.x : 0);
		if (e->value_mask & CWY)
			c->geom.y = e->y +
			    (c->decor ? c->geom.y - c->frame_geom.y : 0);
		if (e->value_mask & CWWidth)
			c->geom.w = e->width;
		if (e->value_mask & CWHeight)
			c->geom.h = e->height;

		recalc_frame(c);

		if (c->decor && (c->frame_geom.x < 0 || c->frame_geom.y < 0)) {
			if (c->frame_geom.x < 0)
				c->geom.x = (c->geom.x - c->frame_geom.x);
			if (c->frame_geom.y < 0)
				c->geom.y = (c->geom.y - c->frame_geom.y);

			recalc_frame(c);
		}

		wc.x = c->frame_geom.x;
		wc.y = c->frame_geom.y;
		wc.width = c->frame_geom.w;
		wc.height = c->frame_geom.h;
		wc.border_width = 0;
		wc.sibling = e->above;
		wc.stack_mode = e->detail;
#ifdef DEBUG
		dump_geom(c, "moving to");
#endif
		XConfigureWindow(dpy, c->frame, e->value_mask, &wc);
#ifdef SHAPE
		if (e->value_mask & (CWWidth | CWHeight))
			set_shape(c);
#endif
		if (c->zoomed && e->value_mask & (CWX | CWY | CWWidth |
		    CWHeight)) {
			c->zoomed = 0;
			remove_atom(c->win, net_wm_state, XA_ATOM,
			    net_wm_state_mv);
			remove_atom(c->win, net_wm_state, XA_ATOM,
			    net_wm_state_mh);
		}
		send_config(c);
	}

	if (c) {
		wc.x = c->geom.x - c->frame_geom.x;
		wc.y = c->geom.y - c->frame_geom.y;
	} else {
		wc.x = e->x;
		wc.y = e->y;
	}
	wc.width = e->width;
	wc.height = e->height;
	wc.sibling = e->above;
	wc.stack_mode = e->detail;
	XConfigureWindow(dpy, e->window, e->value_mask, &wc);

	if ((c = top_client()))
		focus_client(c);
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
		if (e->place == PlaceOnBottom) {
			XLowerWindow(dpy, e->window);
			focus_client(prev_focused());
		} else {
			XRaiseWindow(dpy, e->window);
			if ((c = find_client(e->window, MATCH_ANY)))
				focus_client(c);
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
		for (p = head; p; p = p->next)
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
static void
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

/*
 * If a client wants to manipulate itself or another window it must send a
 * special kind of ClientMessage. As of right now, this only responds to the
 * ICCCM iconify message, but there are more in the EWMH that will be added
 * later.
 */
static void
handle_client_message(XClientMessageEvent *e)
{
	client_t *c;

	if (e->window == root) {
		if (e->message_type == net_cur_desk && e->format == 32)
			goto_desk(e->data.l[0]);
		else if (e->message_type == net_num_desks && e->format == 32)
			ndesks = e->data.l[0];
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
		uniconify_client(c);
		XRaiseWindow(dpy, c->frame);
		focus_client(c);
	} else if (e->message_type == net_wm_state &&
	    e->data.l[1] == net_wm_state_fs) {
		if (e->data.l[0] == net_wm_state_add ||
		    (e->data.l[0] == net_wm_state_toggle && !c->fullscreen))
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
	long supplied;

	if (!(c = find_client(e->window, MATCH_WINDOW)))
		return;

	if (e->atom == XA_WM_NAME || e->atom == net_wm_name) {
		if (c->name)
			XFree(c->name);
		c->name = get_wm_name(c->win);
		redraw_frame(c);
	} else if (e->atom == XA_WM_NORMAL_HINTS) {
		XGetWMNormalHints(dpy, c->win, &c->size, &supplied);
	} else if (e->atom == net_wm_state) {
		check_states(c);
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
		redraw_frame(c);
}

#ifdef SHAPE
static void
handle_shape_change(XShapeEvent *e)
{
	client_t *c;

	if ((c = find_client(e->window, MATCH_WINDOW)))
		set_shape(c);
}
#endif

#ifdef DEBUG
void
show_event(XEvent e)
{
	char *ev_type;
	Window w;
	client_t *c;
	int m = 0;

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
	SHOW_EV(MapNotify, xmap)
	SHOW_EV(MapRequest, xmaprequest)
	SHOW_EV(MappingNotify, xmapping)
	SHOW_EV(PropertyNotify, xproperty)
	SHOW_EV(ReparentNotify, xreparent)
	SHOW_EV(ResizeRequest, xresizerequest)
	SHOW_EV(UnmapNotify, xunmap)
	default:
#ifdef SHAPE
		if (shape && e.type == shape_event) {
			ev_type = "ShapeNotify";
			w = ((XShapeEvent *) & e)->window;
			break;
		}
#endif
		ev_type = malloc(128);
		snprintf(ev_type, 128, "unknown event %d", e.type);
		m = 1;
		w = None;
		break;
	}

	if ((c = find_client(w, MATCH_WINDOW)))
		dump_name(c, ev_type, 'w');
	else if ((c = find_client(w, MATCH_FRAME)))
		dump_name(c, ev_type, 'f');
	else if (w == root)
		dump_win(w, ev_type, 'r');
#if 0
	else
		/* something we are not managing */
		dump_win(w, ev_type, 'u');
#endif

	if (m)
		free(ev_type);
}
#endif
