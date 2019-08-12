/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"
#include "parser.h"

/*
 * If the user specifies an rc file, return NULL immediately if it's not found;
 * otherwise, search for the usual suspects.
 */

FILE *
open_rc(char *rcfile, char *def)
{
	FILE *rc;
	char buf[BUF_SIZE];

	if (rcfile) {
		return fopen(rcfile, "r");
	} else {
		snprintf(buf, sizeof buf, "%s/.aewm/%s", getenv("HOME"), def);
		if ((rc = fopen(buf, "r"))) {
			return rc;
		} else {
			snprintf(buf, sizeof buf, "%s/%s", SYS_RC_DIR, def);
			return fopen(buf, "r");
		}
	}
}

char *
get_rc_line(char *s, int size, FILE * stream)
{
	while (fgets(s, size, stream)) {
		if (s[0] == '#' || s[0] == '\n')
			continue;
		else
			return s;
	}
	return NULL;
}

/*
 * Our crappy parser. A token is either a whitespace-delimited word, or a bunch
 * of words in double quotes (backslashes are permitted in either case).  src
 * points to somewhere in a buffer -- the caller must save the location of this
 * buffer, because we update src to point past all the tokens found so far. If
 * we find a token, we write it into dest (caller is responsible for allocating
 * storage) and return 1. Otherwise return 0.
 */

int
get_token(char **src, char *dest)
{
	int quoted = 0, nchars = 0;

	while (**src && isspace(**src))
		(*src)++;

	if (**src == '"') {
		quoted = 1;
		(*src)++;
	}
	while (**src) {
		if (quoted) {
			if (**src == '"') {
				(*src)++;
				break;
			}
		} else {
			if (isspace(**src))
				break;
		}
		if (**src == '\\')
			(*src)++;
		*dest++ = *(*src)++;
		nchars++;
	}

	*dest = '\0';
	return nchars || quoted;
}
