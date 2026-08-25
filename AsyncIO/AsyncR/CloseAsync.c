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

/*****************************************************************************/


void CloseAsyncR(AsyncRFile *file)
{
    if (file)
    {
	if (file->af_PacketPending == ASR_PKT_PENDING ||
	    file->af_PacketPending == ASR_PKT_START)
	{
	    file->af_PacketPending = ASR_PKT_CLOSE;
	    WaitAsyncRPacket(file);
	}

	Close(file->af_File);
	FreeVec(file);
    }
}

