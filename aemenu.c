/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "common.h"
#include "atom.h"
#include "menu.h"

void *make_launchitem_cb(void *, char *, char *);
void make_clientitem(GtkWidget *, Window);
void fork_exec_cb(GtkWidget *, char *);
void raise_win_cb(GtkWidget *, Window);

int
main(int argc, char **argv)
{
	GtkWidget *main_menu;
	int i, mode = LAUNCH;
	char *opt_config = NULL;
	unsigned long read, left;
	Window w;

	setlocale(LC_ALL, "");
	gtk_init(&argc, &argv);

	for (i = 1; i < argc; i++) {
		if (ARG("config", "rc", 1)) {
			opt_config = argv[++i];
		} else if (ARG("launch", "l", 0)) {
			mode = LAUNCH;
		} else if (ARG("switch", "s", 0)) {
			mode = SWITCH;
		} else {
			fprintf(stderr,
			    "usage: aemenu [--switch|-s] "
			    "[--config|-rc <file>]\n");
			exit(2);
		}
	}

	main_menu = gtk_menu_new();

	if (mode == LAUNCH) {
		make_launch_menu(opt_config, main_menu, make_launchitem_cb);
	} else {	/* mode == SWITCH */
		dpy = GDK_DISPLAY();
		root = GDK_ROOT_WINDOW();
		setup_switch_atoms();
		for (i = 0, left = 1; left; i += read) {
			read = get_atoms(root, net_client_list, XA_WINDOW, i,
			    &w, 1, &left);
			if (!read)
				break;

			make_clientitem(main_menu, w);
		}
	}

	gtk_signal_connect_object(GTK_OBJECT(main_menu), "deactivate",
	    GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
	gtk_menu_popup(GTK_MENU(main_menu), NULL, NULL, NULL, NULL, 0, 0);

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
make_clientitem(GtkWidget *menu, Window w)
{
	GtkWidget *item;
	char buf[BUF_SIZE];

	if (is_on_cur_desk(w) && !is_skip(w)) {
		snprint_wm_name(buf, sizeof buf, w);
		item = gtk_menu_item_new_with_label(buf);
		gtk_menu_append(GTK_MENU(menu), item);
		gtk_signal_connect(GTK_OBJECT(item), "activate",
		    GTK_SIGNAL_FUNC(raise_win_cb), (gpointer) w);
		gtk_widget_show(item);
	}
}

void
fork_exec_cb(GtkWidget * widget, char *data)
{
	fork_exec(data);
	gtk_main_quit();
}

void
raise_win_cb(GtkWidget * widget, Window w)
{
	raise_win(w);
	gtk_main_quit();
}
