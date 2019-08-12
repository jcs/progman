/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#ifndef AEWM_CLIENTS_MENU_H
#define AEWM_CLIENTS_MENU_H

enum {
	LAUNCH,
	SWITCH,
};

typedef void *(*make_item_func) (void *, char *, char *);

extern void setup_switch_atoms();
extern void snprint_wm_name(char *, size_t, Window);
extern int is_on_cur_desk(Window);
extern int is_skip(Window);
extern void raise_win(Window);
extern void make_launch_menu(char *, void *, make_item_func);

#endif	/* AEWM_CLIENTS_MENU_H */
