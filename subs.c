/*
 *    (c)Copyright 1992-1997 Obvious Implementations Corp.  Redistribution and
 *    use is allowed under the terms of the DICE-LICENSE FILE,
 *    DICE-LICENSE.TXT.
 */

/*
 *  Misc. support routines
 */

#include "defs.h"
#include <exec/libraries.h>

Prototype int align(int);

int align(int n)
{
    if (n & 3)
	return(4 - (n & 3));
    return(0);
}

