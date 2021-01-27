/*
 * Copyright 2020 joshua stein <jcs@jcs.org>
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
#include "progman.h"
#include "parser.h"
#include "progman_ini.h"

/*
 * If the user specifies an ini file, return NULL immediately if it's not
 * found; otherwise, search for the usual suspects.
 */
FILE *
open_ini(char *inifile)
{
	FILE *ini = NULL;
	char buf[BUF_SIZE];

	if (inifile) {
		ini = fopen(inifile, "r");
		if (!ini)
			err(1, "can't open config file %s", inifile);

		return ini;
	}

	snprintf(buf, sizeof(buf), "%s/.config/progman/progman.ini",
	    getenv("HOME"));
	if ((ini = fopen(buf, "r")))
		return ini;

	/* load compiled-in defaults */
	ini = fmemopen(progman_ini, sizeof(progman_ini), "r");
	if (!ini || sizeof(progman_ini) == 0)
		errx(1, "no compiled-in default config file");

	return ini;
}

int
find_ini_section(FILE *stream, char *section)
{
	char buf[BUF_SIZE], marker[BUF_SIZE];

	snprintf(marker, sizeof(marker), "[%s]\n", section);

	fseek(stream, 0, SEEK_SET);
	while (fgets(buf, sizeof(buf), stream)) {
		if (buf[0] == '#' || buf[0] == '\n')
			continue;
		if (strncmp(buf, marker, strlen(marker)) == 0)
			return 1;
	}

	return 0;
}

int
get_ini_kv(FILE *stream, char **key, char **val)
{
	char buf[BUF_SIZE], *tval;
	long pos = ftell(stream);
	int len;

	buf[0] = '\0';

	/* find next non-comment, non-section line */
	while (fgets(buf, sizeof(buf), stream)) {
		if (buf[0] == '#' || buf[0] == '\n') {
			pos = ftell(stream);
			continue;
		}

		if (buf[0] == '[') {
			/* new section, rewind so find_ini_section can see it */
			fseek(stream, pos, SEEK_SET);
			return 0;
		}

		break;
	}

	if (!buf[0])
		return 0;

	tval = strchr(buf, '=');
	if (tval == NULL) {
		warnx("bad line in ini file: %s", buf);
		return 0;
	}

	tval[0] = '\0';
	tval++;

	/* trim trailing spaces from key */
	for (len = strlen(buf); len > 0 && buf[len - 1] == ' '; len--)
		;
	buf[len] = '\0';

	/* trim leading spaces from val */
	while (tval[0] == ' ')
		tval++;

	/* and trailing spaces and newlines from val */
	len = strlen(tval) - 1;
	while (len) {
		if (tval[len] == ' ' || tval[len] == '\r' || tval[len] == '\n')
			len--;
		else
			break;
	}
	tval[len + 1] = '\0';

	*key = strdup(buf);
	*val = strdup(tval);

	return 1;
}
