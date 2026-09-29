/*
 * This file is part of AsyncR, a read-only fork of the original AsyncIO aka.
 * "Fast AmigaDOS I/O".
 *
 * AsyncR is Public Domain.
 *
 * Original code by Martin Taillefer.
 * AsyncR fork by Hagbard Celine.
 *
 * This code comes with absolutely no warranty.
 * If it breaks, you get to keep the pieces.
 *
 */

#include "asyncr_internal.h"

static void freearf(APTR arf);

/*****************************************************************************/


void CloseAsyncR(AsyncRFile *file)
{
    if (file)
    {
	if (file->arf_PacketPending == ASR_PKT_PENDING ||
	    file->arf_PacketPending == ASR_PKT_START)
	{
	    file->arf_PacketPending = ASR_PKT_CLOSE;
	    WaitAsyncRPacket(file);
	}

	Close(file->arf_File);
	freearf(file);
    }
}

static void freearf(APTR arf)
{
#if OSVERMIN < 36 && OSVERMAX >= 36
    if (DOSBase->dl_lib.lib_Version >= 36)
    {
#endif
#if OSVERMAX >= 36
	FreeVec(arf);
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
    }
    else
    {
#endif
#if OSVERMIN < 36
	ULONG *alloc = (ULONG *)arf;

	alloc--;

	FreeMem(alloc, *alloc);
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
    }
#endif
}

