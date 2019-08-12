/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
 * This program is free software; please see LICENSE for details. */

#ifndef AEWM_PARSER_H
#define AEWM_PARSER_H

#include <stdio.h>

FILE *open_rc(char *, char *);
char *get_rc_line(char *, int, FILE *);
int get_token(char **, char *);

#endif /* AEWM_PARSER_H */
