/*
 * Copyright (c) 2021 joshua stein <jcs@jcs.org>
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
#include <err.h>
#include "parser.h"
#include "progman.h"

struct program {
	char *name;
	action_t *action;
	struct program *next;
};

Window launcher_win;
XftDraw *launcher_xftdraw;
struct program *program_head = NULL, *program_tail = NULL;
int launcher_width = 0, launcher_height = 0, launcher_highlighted = 0,
    launcher_item_height = 0, launcher_item_padding = 0;

void launcher_reload(void);
void launcher_redraw(void);

void
launcher_setup(void)
{
	XTextProperty name;
	char *title = "Programs";

	launcher_reload();

	launcher_win = XCreateWindow(dpy, root, 0, 0, launcher_width,
	    launcher_height, 0, DefaultDepth(dpy, screen), CopyFromParent,
	    DefaultVisual(dpy, screen), 0, NULL);
	if (!launcher_win)
		err(1, "XCreateWindow");

	if (!XStringListToTextProperty(&title, 1, &name))
		err(1, "XStringListToTextProperty");
	XSetWMName(dpy, launcher_win, &name);

	XSetWindowBackground(dpy, launcher_win, launcher_bg.pixel);

	set_atoms(launcher_win, net_wm_state, XA_ATOM, &net_wm_state_above, 1);
	set_atoms(launcher_win, net_wm_wintype, XA_ATOM, &net_wm_type_utility,
	    1);

	launcher_xftdraw = XftDrawCreate(dpy, launcher_win,
	    DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
}

void
launcher_reload(void)
{
	FILE *ini;
	struct program *program = NULL;
	XSizeHints *hints;
	XGlyphInfo extents;
	action_t *action;
	char *key, *val;
	int tw;

	launcher_programs_free();

	launcher_width = 0;
	launcher_height = 0;
	launcher_item_padding = opt_pad * 2;
	launcher_item_height = font->ascent + (launcher_item_padding * 2);

	ini = open_ini(opt_config_file);

	if (!find_ini_section(ini, "launcher"))
		goto done;

	while (get_ini_kv(ini, &key, &val)) {
		action = parse_action(key, val);
		if (action == NULL)
			continue;

		program = malloc(sizeof(struct program));
		if (!program)
			err(1, "malloc");

		program->next = NULL;

		if (program_tail) {
			program_tail->next = program;
			program_tail = program;
		} else {
			program_head = program;
			program_tail = program;
		}

		XftTextExtentsUtf8(dpy, font, (FcChar8 *)key, strlen(key),
		    &extents);
		tw = extents.xOff + (launcher_item_padding * 2);
		if (tw > launcher_width)
			launcher_width = tw;
		launcher_height += launcher_item_height;

		program->name = strdup(key);
		program->action = action;

		free(key);
		free(val);
	}

done:
	fclose(ini);

	hints = XAllocSizeHints();
	if (!hints)
		err(1, "XAllocSizeHints");

	hints->flags = PMinSize | PMaxSize;
	hints->min_width = launcher_width;
	hints->min_height = launcher_height;
	hints->max_width = launcher_width;
	hints->max_height = launcher_height;

	XSetWMNormalHints(dpy, launcher_win, hints);

	XFree(hints);
}

void
launcher_show(XButtonEvent *e)
{
	client_t *c;
	XEvent ev;
	struct program *program;
	int x, y, mx, my, prev_highlighted;

	x = e->x_root;
	y = e->y_root;
	XMoveResizeWindow(dpy, launcher_win, x, y, launcher_width,
	    launcher_height);

	c = new_client(launcher_win);
	c->placed = 1;
	c->desk = cur_desk;
	map_client(c);
	map_if_desk(c);

	launcher_highlighted = prev_highlighted = 0;
	launcher_redraw();

	if (XGrabPointer(dpy, root, False, MouseMask, GrabModeAsync,
	    GrabModeAsync, root, None, CurrentTime) != GrabSuccess) {
		warnx("failed grabbing pointer");
		goto close_launcher;
	}

	for (;;) {
		XMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask, &ev);

		switch (ev.type) {
		case MotionNotify: {
			XMotionEvent *xmv = (XMotionEvent *)&ev;
			mx = xmv->x - x;
			my = xmv->y - y;

			if (mx < 0 || mx > launcher_width ||
			    my < 0 || my > launcher_height)
				launcher_highlighted = -1;
			else
				launcher_highlighted = (my /
				    launcher_item_height);

			if (launcher_highlighted != prev_highlighted)
				launcher_redraw();

			prev_highlighted = launcher_highlighted;
			break;
		}
		case ButtonRelease:
			goto close_launcher;
			break;
		}
	}

close_launcher:
	XUngrabPointer(dpy, CurrentTime);
	XUnmapWindow(dpy, launcher_win);
	del_client(c, DEL_WITHDRAW);

	if (launcher_highlighted < 0)
		return;

	for (x = 0, program = program_head; program;
	    program = program->next, x++) {
		if (x != launcher_highlighted)
			continue;

		take_action(program->action);
		break;
	}
}

void
launcher_programs_free(void)
{
	struct program *program;

	for (program = program_head; program;) {
		struct program *t = program;

		if (program->name)
			free(program->name);
		if (program->action) {
			if (program->action->sarg)
				free(program->action->sarg);
			free(program->action);
		}
		program = program->next;
		free(t);
	}

	program_head = program_tail = NULL;
}

void
launcher_redraw(void)
{
	struct program *program;
	XftColor *color;
	int i;

	XClearWindow(dpy, launcher_win);

	for (i = 0, program = program_head; program;
	    program = program->next, i++) {
		if (launcher_highlighted == i) {
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    launcher_fg.pixel);
			XFillRectangle(dpy, launcher_win,
			    DefaultGC(dpy, screen),
			    0, launcher_item_height * i,
			    launcher_width, launcher_item_height);
			color = &xft_launcher_highlighted;
		} else {
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    launcher_fg.pixel);
			color = &xft_launcher;
		}


		XftDrawStringUtf8(launcher_xftdraw, color, font,
		    launcher_item_padding,
		    (launcher_item_height * (i + 1)) - launcher_item_padding,
		    (unsigned char *)program->name,
		    strlen(program->name));
	}
}
