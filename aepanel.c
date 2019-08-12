/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "common.h"
#include "atom.h"
#include "menu.h"

typedef struct client client_t;
struct client {
	client_t *next;
	Window win;
	void *widget;
	int save;
};

void *make_launchitem_cb(void *, char *, char *);
void update_clientitem(GtkWidget *, Window);
void cleanup_clientitem(client_t *);
GdkFilterReturn check_event(GdkXEvent *, GdkEvent *, gpointer);
void update_client_list(GtkWidget *);
void show_menu_cb(GtkWidget *, gpointer);
void raise_win_cb(GtkWidget *, Window);
void fork_exec_cb(GtkWidget *, char *);
void setup_panel_atoms();
void sig_handler(int);
void set_strut(Window, strut_t *);

Atom net_wm_strut;
Atom net_wm_strut_partial;
Atom net_wm_wintype;
Atom net_wm_wintype_dock;

#define NAME_SIZE 48

client_t *head = NULL;

GtkWidget *menu_button = NULL;
int opt_bottom;

int
main(int argc, char **argv)
{
	GtkWidget *toplevel, *hbox, *launch_menu;
	GtkWidget *clients_box;
	struct sigaction act;
	char *opt_config = NULL;
	strut_t s = { 0, 0, 0, 0 };
	client_t *c;
	int i, rw, rh;
	unsigned long read, left;
	Window w;

	setlocale(LC_ALL, "");
	gtk_init(&argc, &argv);
	gdk_error_trap_push();

	for (i = 1; i < argc; i++) {
		if (ARG("config", "rc", 1)) {
			opt_config = argv[++i];
		} else if (ARG("bottom", "b", 0)) {
			opt_bottom = 1;
		} else {
			fprintf(stderr,
			    "usage: aepanel [--bottom|-b] "
			    "[--config|-rc <file>]\n");
			exit(2);
		}
	}

	act.sa_handler = sig_handler;
	act.sa_flags = 0;
	sigaction(SIGCHLD, &act, NULL);

	dpy = GDK_DISPLAY();
	root = GDK_ROOT_WINDOW();

	setup_switch_atoms();
	setup_panel_atoms();

	toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(toplevel), "delete_event",
	    G_CALLBACK(gtk_main_quit), NULL);

	launch_menu = gtk_menu_new();

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
	gtk_container_add(GTK_CONTAINER(toplevel), hbox);

	menu_button = gtk_button_new_with_label("Launch");
	gtk_signal_connect(GTK_OBJECT(menu_button), "clicked",
	    GTK_SIGNAL_FUNC(show_menu_cb), launch_menu);
	gtk_box_pack_start(GTK_BOX(hbox), menu_button, FALSE, FALSE, 0);

	clients_box = gtk_hbox_new(TRUE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), clients_box);
	gtk_container_set_resize_mode(GTK_CONTAINER(clients_box),
	    GTK_RESIZE_QUEUE);

	XSelectInput(dpy, GDK_ROOT_WINDOW(), PropertyChangeMask);
	gdk_window_add_filter(gdk_get_default_root_window(),
	    check_event, clients_box);

	make_launch_menu(opt_config, launch_menu, make_launchitem_cb);

	for (i = 0, left = 1; left; i += read) {
		read = get_atoms(root, net_client_list, XA_WINDOW, i, &w, 1,
		    &left);
		if (!read)
			break;

		update_clientitem(clients_box, w);
	}

	for (c = head; c; c = c->next)
		cleanup_clientitem(c);

	gtk_widget_show_all(hbox);
	gtk_widget_realize(toplevel);

	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(toplevel), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(toplevel), TRUE);
	gtk_window_set_decorated(GTK_WINDOW(toplevel), FALSE);
	gtk_window_stick(GTK_WINDOW(toplevel));

	/*
	 * This last call is not working for some reason, so we'll just have to
	 * kludge it for now.
	 */
#ifdef _HEY_LOOK_AT_THAT_SOMEBODY_FIXED_THE_GTKWINDOW_TYPE_HINT_CRAP
	gtk_window_set_type_hint(GTK_WINDOW(toplevel),
	    GDK_WINDOW_TYPE_HINT_DOCK);
#else
	set_atoms(GDK_WINDOW_XID(toplevel->window), net_wm_wintype, XA_ATOM,
	    &net_wm_type_dock, 1);
#endif

	gdk_window_get_size(gdk_get_default_root_window(), &rw, &rh);
	gtk_widget_set_size_request(toplevel, rw, -1);
	gtk_window_move(GTK_WINDOW(toplevel), 0,
	    opt_bottom ? rh - toplevel->allocation.height : 0);

	gtk_widget_show_all(toplevel);

	if (opt_bottom)
		s.bottom = toplevel->allocation.height;
	else
		s.top = toplevel->allocation.height;

	set_strut(GDK_WINDOW_XID(toplevel->window), &s);

	gtk_main();
	return 0;
}

void *
make_launchitem_cb(void *menu, char *label, char *cmd)
{
	GtkWidget *item, *sub_menu = NULL;

	item = gtk_menu_item_new_with_label(label);
	gtk_menu_append(GTK_MENU(menu), item);

	if (cmd) {
		gtk_signal_connect(GTK_OBJECT(item), "activate",
		    GTK_SIGNAL_FUNC(fork_exec_cb), cmd);
	} else {
		sub_menu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), sub_menu);
	}

	gtk_widget_show(item);
	return sub_menu;
}

void
update_clientitem(GtkWidget *container, Window w)
{
	char buf[NAME_SIZE];
	client_t *c;

	for (c = head; c; c = c->next) {
		if (c->win == w) {
			if (is_on_cur_desk(w))
				gtk_widget_show(GTK_WIDGET(c->widget));
			else
				gtk_widget_hide(GTK_WIDGET(c->widget));
			c->save = 1;
			return;
		}
	}

	if (!is_skip(w) && is_on_cur_desk(w)) {
		c = malloc(sizeof *c);
		c->next = head;
		head = c;
		c->win = w;
		c->save = 1;

		snprint_wm_name(buf, sizeof buf, w);
		c->widget = gtk_button_new_with_label(buf);
		gtk_signal_connect(GTK_OBJECT(c->widget), "clicked",
		    GTK_SIGNAL_FUNC(raise_win_cb), (gpointer) w);
		gtk_box_pack_start(GTK_BOX(container), c->widget, TRUE, TRUE,
		    0);
		gtk_misc_set_alignment(GTK_MISC(GTK_BIN(c->widget)->child), 0,
		    0.5);
		gtk_widget_show(c->widget);

		XSelectInput(dpy, c->win, PropertyChangeMask);
		gdk_window_add_filter(gdk_window_lookup(c->win),
		    check_event, container);
	}
}

void
cleanup_clientitem(client_t *c)
{
	client_t *p;

	if (c->save) {
		c->save = 0;
		return;
	}

	gtk_widget_destroy(GTK_WIDGET(c->widget));
	if (head == c) {
		head = c->next;
	} else {
		for (p = head; p && p->next; p = p->next)
			if (p->next == c)
				p->next = c->next;
	}
	free(c);
}

GdkFilterReturn
check_event(GdkXEvent * gdk_xevent, GdkEvent * event,
    gpointer container)
{
	XEvent *e = gdk_xevent;
	client_t *c;
	char buf[NAME_SIZE];

	if (e->type == PropertyNotify) {
		if (e->xproperty.window == root) {
			if (e->xproperty.atom == net_cur_desk ||
			    e->xproperty.atom == net_client_list)
				update_client_list(GTK_WIDGET(container));
		} else {
			if (e->xproperty.atom == net_wm_desk) {
				update_client_list(GTK_WIDGET(container));
			} else {
				/* don't really care which atom changed; just
				 * redo it */
				for (c = head; c; c = c->next) {
					if (c->win == e->xproperty.window) {
						snprint_wm_name(buf,
						    sizeof buf, c->win);
						gtk_label_set_text(
						    GTK_LABEL(GTK_BIN(c->widget)->child),
						    buf);
					}
				}
			}
		}
	}
	return GDK_FILTER_CONTINUE;
}

void
update_client_list(GtkWidget * container)
{
	client_t *c, *save_next;
	unsigned long read, left;
	int i;
	Window w;

	for (i = 0, left = 1; left; i += read) {
		read = get_atoms(root, net_client_list, XA_WINDOW, i, &w, 1,
		    &left);
		if (!read)
			break;

		update_clientitem(container, w);
	}

	for (c = head; c; c = save_next) {
		save_next = c->next;
		cleanup_clientitem(c);
	}
}

void
menu_position(GtkMenu * menu, gint * x, gint * y, gboolean * push_in,
    gpointer data)
{
	GtkWidget *button = GTK_WIDGET(data);
	GtkRequisition req;
	gint wx, wy;

	gdk_window_get_root_origin(button->window, &wx, &wy);
	*x = wx + button->allocation.x;
	*y = wy + button->allocation.y;

	if (opt_bottom) {
		gtk_widget_size_request(GTK_WIDGET(menu), &req);
		*y -= req.height;
	} else {
		*y += button->allocation.height;
	}
}

void
show_menu_cb(GtkWidget * widget, gpointer menu)
{
	gtk_menu_popup(menu, NULL, NULL, menu_position, menu_button, 0, 0);
}

void
raise_win_cb(GtkWidget * widget, Window w)
{
	raise_win(w);
}

void
fork_exec_cb(GtkWidget * widget, char *data)
{
	fork_exec(data);
}

void
setup_panel_atoms()
{
	net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
	net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
	net_wm_wintype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	net_wm_wintype_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK",
	    False);
}

void
sig_handler(int signal)
{
	if (signal == SIGCHLD)
		wait(NULL);
}

void
set_strut(Window w, strut_t *s)
{
	unsigned long data[4];

	data[0] = s->left;
	data[1] = s->right;
	data[2] = s->top;
	data[3] = s->bottom;

	XChangeProperty(dpy, w, net_wm_strut, XA_CARDINAL, 32, PropModeReplace,
	    (unsigned char *)data, 4);
}
