/*
 * Copyright (c) 2020 joshua stein <jcs@jcs.org>
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "progman.h"

static client_t *cycle_head = NULL;

static struct key_action *key_actions;
static int nkey_actions = 0;
static int cycle_key = 0;

void
bind_key(char *key, char *action)
{
	char *tkey, *fkey;
	char *sep;
	char *taction, *faction, *targ;
	char *sarg = NULL;
	KeySym k = 0;
	int x, mod = 0, iaction = -1, iarg, overwrite, aidx;

	tkey = fkey = strdup(key);
	for (x = 0; tkey[x] != '\0'; x++)
		tkey[x] = tolower(tkey[x]);

	/* key can be "shift+alt+f1" or "Super+Space" or just "ampersand" */
	while ((sep = strchr(tkey, '+'))) {
		*sep = '\0';
		if (strcmp(tkey, "shift") == 0)
			mod |= ShiftMask;
		else if (strcmp(tkey, "control") == 0 ||
		    strcmp(key, "ctrl") == 0 || strcmp(tkey, "ctl") == 0)
			mod |= ControlMask;
		else if (strcmp(tkey, "alt") == 0 ||
		    strcmp(key, "meta") == 0 || strcmp(key, "mod1") == 0)
			mod |= Mod1Mask;
		else if (strcmp(tkey, "mod2") == 0)
			mod |= Mod2Mask;
		else if (strcmp(tkey, "mod3") == 0)
			mod |= Mod3Mask;
		else if (strcmp(tkey, "super") == 0 ||
		    strcmp(tkey, "win") == 0 || strcmp(tkey, "mod4") == 0)
			mod |= Mod4Mask;
		else {
			warnx("failed parsing modifiers in \"%s\", skipping",
			    key);
			free(fkey);
			return;
		}

		tkey = (sep + 1);
	}

	/* modifiers have been parsed, only the key should remain */
	if (tkey[0] != '\0') {
		k = XStringToKeysym(tkey);
		if (k == 0) {
			tkey[0] = toupper(tkey[0]);
			k = XStringToKeysym(tkey);
		}

		if (k == 0) {
			warnx("failed parsing key \"%s\", skipping\n", tkey);
			free(fkey);
			return;
		}
	}

	free(fkey);

	/* action can be "cycle" or "exec xterm -g 80x50" */
	taction = faction = strdup(action);
	if ((sep = strchr(taction, ' '))) {
		*sep = '\0';
		targ = sep + 1;
	} else
		targ = NULL;

	if (strcmp(taction, "cycle") == 0)
		iaction = ACTION_CYCLE;
	else if (strcmp(taction, "reverse_cycle") == 0)
		iaction = ACTION_REVERSE_CYCLE;
	else if (strcmp(taction, "desk") == 0)
		iaction = ACTION_DESK;
	else if (strcmp(taction, "close") == 0)
		iaction = ACTION_CLOSE;
	else if (strcmp(taction, "exec") == 0)
		iaction = ACTION_EXEC;
	else if (taction[0] == '\n' || taction[0] == '\0')
		iaction = ACTION_NONE;
	else {
		warnx("unknown \"%s\" action \"%s\", skipping", key, taction);
		free(faction);
		return;
	}

	/* parse numeric or string args */
	switch (iaction) {
	case ACTION_DESK:
		if (targ == NULL) {
			warn("missing numeric argument for \"%s\"", taction);
			free(faction);
			return;
		}

     		errno = 0;
		iarg = strtol(targ, NULL, 10);
		if (errno != 0) {
			warn("failed parsing numeric argument in \"%s\"",
			    taction);
			free(faction);
			return;
		}
		break;
	case ACTION_EXEC:
		if (targ == NULL) {
			warn("missing string argument for \"%s\"", taction);
			free(faction);
			return;
		}
		sarg = strdup(targ);
		break;
	}

	free(faction);

	/* if we're overriding an existing config, replace it in key_actions */
	overwrite = 0;
	for (x = 0; x < nkey_actions; x++) {
		if (key_actions[x].key == k && key_actions[x].mod == mod) {
			overwrite = 1;
			aidx = x;
			break;
		}
	}

	if (!overwrite) {
		key_actions = realloc(key_actions,
		    (nkey_actions + 1) * sizeof(struct key_action));
		if (key_actions == NULL)
			err(1, "realloc");

		aidx = nkey_actions;
	}

	if (iaction == ACTION_NONE) {
		key_actions[aidx].key = -1;
		key_actions[aidx].mod = -1;
	} else {
		key_actions[aidx].key = k;
		key_actions[aidx].mod = mod;
	}
	key_actions[aidx].action = iaction;
	key_actions[aidx].iarg = iarg;
	if (overwrite && key_actions[aidx].sarg)
		free(key_actions[aidx].sarg);
	key_actions[aidx].sarg = sarg;

	if (!overwrite)
		nkey_actions++;

#ifdef DEBUG
	if (iaction == ACTION_NONE)
		printf("%s(%s): unbinding key %ld with mod mask 0x%x\n",
		    __func__, key, k, mod);
	else
		printf("%s(%s): binding key %ld with mod mask 0x%x to action "
		    "\"%s\"\n", __func__, key, k, mod, action);
#endif

	if (overwrite && iaction == ACTION_NONE)
		XUngrabKey(dpy, XKeysymToKeycode(dpy, k), mod, root);
	else if (!overwrite)
		XGrabKey(dpy, XKeysymToKeycode(dpy, k), mod, root, False,
		    GrabModeAsync, GrabModeAsync);
}

void
handle_key_event(XKeyEvent *e)
{
#ifdef DEBUG
	char buf[64];
#endif
	KeySym kc;
	struct key_action *action = NULL;
	client_t *p, *next;
	int i;

	kc = XLookupKeysym(e, 0);

#ifdef DEBUG
	snprintf(buf, sizeof(buf), "%c:%ld", e->type == KeyRelease ? 'U' : 'D',
	    kc);
	dump_name(focused, __func__, buf, NULL);
#endif

	if (cycle_key && kc != cycle_key && e->type == KeyRelease) {
		/*
		 * If any key other than the non-modifier(s) of our cycle
		 * binding was released, consider the cycle over.
		 */
		cycle_key = 0;
		XUngrabKeyboard(dpy, CurrentTime);
		XAllowEvents(dpy, ReplayKeyboard, e->time);
		XFlush(dpy);

		if (cycle_head) {
			cycle_head = NULL;
			if (focused && focused->state & STATE_ICONIFIED)
				uniconify_client(focused);
		}

		return;
	}

	for (i = 0; i < nkey_actions; i++) {
		if (key_actions[i].key == kc && key_actions[i].mod == e->state)
		{
			action = &key_actions[i];
			break;
		}
	}

	if (!action)
		return;

	switch (key_actions[i].action) {
	case ACTION_CYCLE:
	case ACTION_REVERSE_CYCLE:
		if (e->type != KeyPress)
			break;

		/*
		 * Keep watching input until the modifier is released, but the
		 * keycode will be the modifier key
		 */
		XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync,
		    e->time);
		cycle_key = key_actions[i].key;

		if (!cycle_head) {
			if (!focused)
				return;

			cycle_head = focused;
		}

		if ((next = next_client_for_focus(cycle_head)))
			focus_client(next, FOCUS_FORCE);
		else {
			/* probably at the end of the list, invert it */
			p = focused;
			adjust_client_order(NULL, ORDER_INVERT);

			if (p)
				/* p should now not be focused */
				redraw_frame(p, None);

			focus_client(cycle_head, FOCUS_FORCE);
		}
		break;
	case ACTION_DESK:
		if (e->type == KeyPress)
			goto_desk(key_actions[i].iarg);
		break;
	case ACTION_CLOSE:
		if (e->type == KeyPress && focused)
			send_wm_delete(focused);
		break;
	case ACTION_EXEC:
		if (e->type == KeyPress)
			fork_exec(key_actions[i].sarg);
		break;
	default:
		warnx("unknown key action %d (index %d)\n",
		    key_actions[i].action, i);
	}
}
