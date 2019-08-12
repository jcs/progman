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

#ifndef AEWM_ATOM_H
#define AEWM_ATOM_H

#include <X11/Xlib.h>

struct strut {
	unsigned long left;
	unsigned long right;
	unsigned long top;
	unsigned long bottom;
};

typedef struct strut strut_t;

extern Atom utf8_string;
extern Atom wm_state;
extern Atom wm_change_state;
extern Atom wm_protos;
extern Atom wm_delete;
extern Atom net_supported;
extern Atom net_client_list;
extern Atom net_client_stack;
extern Atom net_active_window;
extern Atom net_close_window;
extern Atom net_cur_desk;
extern Atom net_num_desks;
extern Atom net_wm_name;
extern Atom net_wm_desk;
extern Atom net_wm_state;
extern Atom net_wm_state_shaded;
extern Atom net_wm_state_mv;
extern Atom net_wm_state_mh;
extern Atom net_wm_state_fs;
extern Atom net_wm_state_skipt;
extern Atom net_wm_state_skipp;
extern Atom net_wm_strut;
extern Atom net_wm_strut_partial;
extern Atom net_wm_wintype;
extern Atom net_wm_type_dock;
extern Atom net_wm_type_menu;
extern Atom net_wm_type_splash;
extern Atom net_wm_type_desk;

extern unsigned long get_atoms(Window, Atom, Atom, unsigned long,
    unsigned long *, unsigned long, unsigned long *);
extern unsigned long set_atoms(Window, Atom, Atom, unsigned long *,
    unsigned long);
extern unsigned long append_atoms(Window, Atom, Atom, unsigned long *,
    unsigned long);
extern void remove_atom(Window, Atom, Atom, unsigned long);
extern char *get_wm_name(Window);
int get_strut(Window, strut_t *);
extern unsigned long get_wm_state(Window);

#endif	/* AEWM_ATOM_H */
