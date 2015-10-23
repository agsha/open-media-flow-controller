/* $Id: xstrdup.c,v 1.1.1.1 2003/09/19 17:05:33 gregs Exp $ */

#include <string.h>

#include "config.h"

/*
 * Duplicate a string.
 */
char *(xstrdup)(const char *s)
{
	return strcpy(xmalloc(strlen(s) + 1), s);
}
