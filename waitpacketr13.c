/*
 *
 *  This file is a wrapper for "AsyncIO/src/WaitPacket.c", with minor
 *  modifications to make it work in read mode on 1.3.
 *
 *  I consider this file too trivial to be copyrightable.
 *  However, to avoid any doubt, I hereby release it into the public domain.
 *
 *  - Hagbard Celine
 *
 */

#include <stdio.h>
#include <clib/dos_protos.h>
#include <pragmas/dos_pragmas.h>
#include <proto/intuition.h>

#define  SetIoErr setioerr
#define  ErrorReport errorreport

extern struct IntuiText *ITextOf(char *ptr);

LONG errorreport( LONG code, LONG type, ULONG arg1, struct MsgPort *device );
LONG setioerr(LONG code);


#include "AsyncIO/src/WaitPacket.c"

LONG setioerr(LONG code)
{
    LONG ret;
    struct Process *self = (struct Process *)FindTask(NULL);

    ret = self->pr_Result2;
    self->pr_Result2 = code;
    return ret;
}

#undef  ErrorReport

LONG errorreport( LONG code, LONG type, ULONG arg1, struct MsgPort *device )
{
#if OSVERMAX >= 36
    if (DOSBase->dl_lib.lib_Version >= 36)
	return ErrorReport(code, type, arg1, device);
    else
#endif
    {
	char buf[32];

	sprintf(buf, "I/O error: %ld", code);
	return (!(AutoRequest(NULL, ITextOf(buf), ITextOf("Retry"), ITextOf("Cancel"), 0, 0, 300, 40)));
    }
}

