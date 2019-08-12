/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int
main(void)
{
	/*
	 * ``I gained nothing at all from Supreme Enlightenment, and for that
	 * very reason it is called Supreme Enlightenment.'' -- Buddha
	 */
	for (;;)
		if (wait(NULL) == -1)
			sleep(1);
}
