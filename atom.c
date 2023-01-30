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

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <X11/Xutil.h>
#include "progman.h"

Atom kde_net_wm_window_type_override;
Atom net_active_window;
Atom net_client_list;
Atom net_client_stack;
Atom net_close_window;
Atom net_cur_desk;
Atom net_num_desks;
Atom net_supported;
Atom net_supporting_wm;
Atom net_wm_desk;
Atom net_wm_icon;
Atom net_wm_icon_name;
Atom net_wm_name;
Atom net_wm_state;
Atom net_wm_state_above;
Atom net_wm_state_add;
Atom net_wm_state_below;
Atom net_wm_state_fs;
Atom net_wm_state_mh;
Atom net_wm_state_mv;
Atom net_wm_state_rm;
Atom net_wm_state_shaded;
Atom net_wm_state_skipp;
Atom net_wm_state_skipt;
Atom net_wm_state_toggle;
Atom net_wm_strut;
Atom net_wm_strut_partial;
Atom net_wm_type_desk;
Atom net_wm_type_dock;
Atom net_wm_type_menu;
Atom net_wm_type_splash;
Atom net_wm_type_utility;
Atom net_wm_wintype;
Atom utf8_string;
Atom wm_change_state;
Atom wm_delete;
Atom wm_protos;
Atom wm_state;
Atom xrootpmap_id;

static char *get_string_atom(Window, Atom, Atom);
static char *_get_wm_name(Window, int);

void
find_supported_atoms(void)
{
	net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
	utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
	wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wm_protos = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_state = XInternAtom(dpy, "WM_STATE", False);
	net_wm_state_rm = 0;
	net_wm_state_add = 1;
	net_wm_state_toggle = 2;

	xrootpmap_id = XInternAtom(dpy, "_XROOTPMAP_ID", False);

	kde_net_wm_window_type_override = XInternAtom(dpy,
	    "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE", False);
	append_atoms(root, net_supported, XA_ATOM,
	    &kde_net_wm_window_type_override, 1);

	net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	append_atoms(root, net_supported, XA_ATOM, &net_active_window, 1);

	net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	append_atoms(root, net_supported, XA_ATOM, &net_client_list, 1);

	net_client_stack = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
	append_atoms(root, net_supported, XA_ATOM, &net_client_stack, 1);

	net_close_window = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
	append_atoms(root, net_supported, XA_ATOM, &net_close_window, 1);

	net_cur_desk = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	append_atoms(root, net_supported, XA_ATOM, &net_cur_desk, 1);

	net_num_desks = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	append_atoms(root, net_supported, XA_ATOM, &net_num_desks, 1);

	net_supporting_wm = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	append_atoms(root, net_supported, XA_ATOM, &net_supporting_wm, 1);

	net_wm_desk = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_desk, 1);

	net_wm_icon = XInternAtom(dpy, "_NET_WM_ICON", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_icon, 1);

	net_wm_icon_name = XInternAtom(dpy, "_NET_WM_ICON_NAME", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_icon_name, 1);

	net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_name, 1);

	net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state, 1);

	net_wm_state_above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_above, 1);

	net_wm_state_below = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_below, 1);

	net_wm_state_fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_fs, 1);

	net_wm_state_mh = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ",
	    False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_mh, 1);

	net_wm_state_mv = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT",
	    False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_mv, 1);

	net_wm_state_shaded = XInternAtom(dpy, "_NET_WM_STATE_SHADED", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_state_shaded, 1);

	net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_strut, 1);

	net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_strut_partial, 1);

	net_wm_type_desk = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP",
	    False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_desk, 1);

	net_wm_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_dock, 1);

	net_wm_type_menu = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_menu, 1);

	net_wm_type_splash = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH",
	    False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_splash, 1);

	net_wm_type_utility = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY",
	    False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_type_utility, 1);

	net_wm_wintype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	append_atoms(root, net_supported, XA_ATOM, &net_wm_wintype, 1);
}

/*
 * Despite the fact that all these are 32 bits on the wire, libX11 really does
 * stuff an array of longs into *data, so you get 64 bits on 64-bit archs. So
 * we gotta be careful here.
 */

unsigned long
get_atoms(Window w, Atom a, Atom type, unsigned long off, unsigned long *ret,
    unsigned long nitems, unsigned long *left)
{
	Atom real_type;
	int i, real_format = 0;
	unsigned long items_read = 0;
	unsigned long bytes_left = 0;
	unsigned long *p;
	unsigned char *data = NULL;

	XSetErrorHandler(ignore_xerror);
	XGetWindowProperty(dpy, w, a, off, nitems, False, type, &real_type,
	    &real_format, &items_read, &bytes_left, &data);
	XSetErrorHandler(handle_xerror);

	if (real_format == 32 && items_read) {
		p = (unsigned long *)data;
		for (i = 0; i < items_read; i++)
			*ret++ = *p++;
		if (left)
			*left = bytes_left;
	} else {
		items_read = 0;
		if (left)
			*left = 0;
	}

	if (data != NULL)
		XFree(data);

	return items_read;
}

unsigned long
set_atoms(Window w, Atom a, Atom type, unsigned long *val,
    unsigned long nitems)
{
	return (XChangeProperty(dpy, w, a, type, 32, PropModeReplace,
		(unsigned char *) val, nitems) == Success);
}

unsigned long
append_atoms(Window w, Atom a, Atom type, unsigned long *val,
    unsigned long nitems)
{
	return (XChangeProperty(dpy, w, a, type, 32, PropModeAppend,
		(unsigned char *) val, nitems) == Success);
}

void
remove_atom(Window w, Atom a, Atom type, unsigned long remove)
{
	unsigned long tmp, read, left, *new;
	int i, j = 0;

	read = get_atoms(w, a, type, 0, &tmp, 1, &left);
	if (!read)
		return;

	new = malloc((read + left) * sizeof(*new));
	if (new == NULL)
		err(1, "malloc");
	if (read && tmp != remove)
		new[j++] = tmp;

	for (i = 1, read = left = 1; read && left; i += read) {
		read = get_atoms(w, a, type, i, &tmp, 1, &left);
		if (!read)
			break;
		if (tmp != remove)
			new[j++] = tmp;
	}

	if (j)
		XChangeProperty(dpy, w, a, type, 32, PropModeReplace,
		    (unsigned char *)new, j);
	else
		XDeleteProperty(dpy, w, a);

	free(new);
}

/*
 * Get the window-manager name (aka human-readable "title") for a given window.
 * There are two ways a client can set this:
 *
 * 1. _NET_WM_STRING, which has type UTF8_STRING.
 * This is preferred and is always used if available.
 *
 * 2. WM_NAME, which has type COMPOUND_STRING or STRING.
 * This is the old ICCCM way, which we fall back to in the absence of
 * _NET_WM_STRING. In this case, we ask X to convert the value of the property
 * to UTF-8 for us. N.b.: STRING is Latin-1 whatever the locale.
 * COMPOUND_STRING is the most hideous abomination ever created. Thankfully we
 * do not have to worry about any of this.
 *
 * If UTF-8 conversion is not available (XFree86 < 4.0.2, or any older X
 * implementation), only WM_NAME will be checked, and, at least for XFree86 and
 * X.Org, it will only be returned if it has type STRING. This is due to an
 * inherent limitation in their implementation of XFetchName. If you have a
 * different X vendor, YMMV.
 *
 * In all cases, this function asks X to allocate the returned string, so it
 * must be freed with XFree.
 */
char *
_get_wm_name(Window w, int icon)
{
	char *name;
	XTextProperty name_prop;
	XTextProperty name_prop_converted;
	char **name_list;
	int nitems;

	if (icon) {
		if ((name = get_string_atom(w, net_wm_icon_name, utf8_string)))
			return name;
		if (!XGetWMIconName(dpy, w, &name_prop))
			return NULL;
	} else {
		if ((name = get_string_atom(w, net_wm_name, utf8_string)))
			return name;
		if (!XGetWMName(dpy, w, &name_prop))
			return NULL;
	}

	if (Xutf8TextPropertyToTextList(dpy, &name_prop, &name_list,
	    &nitems) == Success && nitems >= 1) {
		/*
		 * Now we've got a freshly allocated XTextList. Since
		 * it might have multiple items that need to be joined,
		 * and we need to return something that can be freed by
		 * XFree, we roll it back up into an XTextProperty.
		 */
		if (Xutf8TextListToTextProperty(dpy, name_list, nitems,
			XUTF8StringStyle, &name_prop_converted) == Success) {
			XFreeStringList(name_list);
			return (char *)name_prop_converted.value;
		}

		/*
		 * Not much we can do here. This should never
		 * happen anyway. Famous last words.
		 */
		XFreeStringList(name_list);
		return NULL;
	}

	return (char *)name_prop.value;
}

char *
get_wm_name(Window w)
{
	return _get_wm_name(w, 0);
}

char *
get_wm_icon_name(Window w)
{
	return _get_wm_name(w, 1);
}

/*
 * I give up on trying to do this the right way. We'll just request as many
 * elements as possible. If that's not the entire string, we're fucked. In
 * reality this should never happen. (That's the second time I get to say "this
 * should never happen" in this file!)
 *
 * Standard gripe about casting nonsense applies.
 */
static char *
get_string_atom(Window w, Atom a, Atom type)
{
	Atom real_type;
	int real_format = 0;
	unsigned long items_read = 0;
	unsigned long bytes_left = 0;
	unsigned char *data;

	XGetWindowProperty(dpy, w, a, 0, LONG_MAX, False, type,
	    &real_type, &real_format, &items_read, &bytes_left, &data);

	/* XXX: should check bytes_left here and bail if nonzero, in case
	 * someone wants to store a >4gb string on the server */

	if (real_format == 8 && items_read >= 1)
		return (char *)data;

	return NULL;
}

void
set_string_atom(Window w, Atom a, unsigned char *str, unsigned long len)
{
	XChangeProperty(dpy, w, a, utf8_string, 8, PropModeReplace, str, len);
}

/*
 * Reads the _NET_WM_STRUT_PARTIAL or _NET_WM_STRUT hint into the args, if it
 * exists. In the case of _NET_WM_STRUT_PARTIAL we cheat and only take the
 * first 4 values, because that's all we care about. This means we can use the
 * same code for both (_NET_WM_STRUT only specifies 4 elements). Each number is
 * the margin in pixels on that side of the display where we don't want to
 * place clients. If there is no hint, we act as if it was all zeros (no
 * margin).
 */
int
get_strut(Window w, strut_t *s)
{
	Atom real_type;
	int real_format = 0;
	unsigned long items_read = 0;
	unsigned long bytes_left = 0;
	unsigned char *data;
	unsigned long *strut_data;

	XGetWindowProperty(dpy, w, net_wm_strut_partial, 0, 12, False,
	    XA_CARDINAL, &real_type, &real_format, &items_read, &bytes_left,
	    &data);

	if (!(real_format == 32 && items_read >= 12))
		XGetWindowProperty(dpy, w, net_wm_strut, 0, 4, False,
		    XA_CARDINAL, &real_type, &real_format, &items_read,
		    &bytes_left, &data);

	if (real_format == 32 && items_read >= 4) {
		strut_data = (unsigned long *) data;
		s->left = strut_data[0];
		s->right = strut_data[1];
		s->top = strut_data[2];
		s->bottom = strut_data[3];
		XFree(data);
		return 1;
	}

	s->left = 0;
	s->right = 0;
	s->top = 0;
	s->bottom = 0;

	return 0;
}

unsigned long
get_wm_state(Window w)
{
	unsigned long state;

	if (get_atoms(w, wm_state, wm_state, 0, &state, 1, NULL))
		return state;

	return WithdrawnState;
}
