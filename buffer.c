/*
 *    (c)Copyright 1992-1997 Obvious Implementations Corp.  Redistribution and
 *    use is allowed under the terms of the DICE-LICENSE FILE,
 *    DICE-LICENSE.TXT.
 */

/*
 *
 */

#include "defs.h"

Prototype char *AllocPathBuffer(void);
Prototype void FreePathBuffer(char *);


List PathBufList = { (Node *)&PathBufList.lh_Tail, NULL, (Node *)&PathBufList.lh_Head };

char *AllocPathBuffer()
{
    Node *node;

    if ((node = RemHead(&PathBufList)) == NULL)
    {
	if (!(node = PAlloc(PBUFSIZE)))
	    error(FATAL, IoErr(), "memory allocation failed");
    }
    return((char *)node);
}

void FreePathBuffer(char *buf)
{
    AddTail(&PathBufList, (Node *)buf);
}

