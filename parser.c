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

	if (rcfile)
		return fopen(rcfile, "r");

	snprintf(buf, sizeof buf, "%s/.aewm/%s", getenv("HOME"), def);
	if ((rc = fopen(buf, "r")))
		return rc;

	snprintf(buf, sizeof buf, "%s/%s", SYS_RC_DIR, def);
	return fopen(buf, "r");
}

char *
get_rc_line(char *s, int size, FILE * stream)
{
	while (fgets(s, size, stream)) {
		if (s[0] == '#' || s[0] == '\n')
			continue;

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
