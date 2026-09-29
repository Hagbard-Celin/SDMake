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


LONG ReadCharAsyncR(AsyncRFile *file)
{
    LONG ret = -1;
    LONG bytesArrived;
    unsigned char ch;

    SetIoErr(0);

    /* wait for the buffer to fill if this is the first read after open */
    if (file->arf_PacketPending == ASR_PKT_START)
    {
	bytesArrived = WaitAsyncRPacket(file);

	if (bytesArrived <= 0)
	    goto end;

	file->arf_BytesLeft   = bytesArrived;
    }

    if (file->arf_PacketPending == ASR_PKT_IDLE)
    {
	ULONG nextpos;

	nextpos = file->arf_BufMin[file->arf_CurrentBuf] + file->arf_BytesArrived[file->arf_CurrentBuf];

	/* does the other buffer already contain the data we need */
	if (nextpos && file->arf_BufMin[1 - file->arf_CurrentBuf] == nextpos)
	{
	    file->arf_PacketPending = ASR_PKT_READY;
	}
	else
	{
	    if (file->arf_SequentialBytes < ASR_SEQBYTESTHRESH)
		file->arf_SequentialBytes++;

	    if (file->arf_SequentialBytes >= ASR_SEQBYTESTHRESH ||
		file->arf_BytesLeft < ASR_BYTESLEFTTHRESH)
	    {
		if (SendAsyncRPacket(file, file->arf_Buffers[1 - file->arf_CurrentBuf], nextpos))
		    goto end;
	    }
	}
    }

    if (!file->arf_BytesLeft)
    {
	if (file->arf_PacketPending == ASR_PKT_READY)
	{
	    bytesArrived = file->arf_BytesArrived[1 - file->arf_CurrentBuf];

	    /* keep ASR_PKT_READY for single buffer mode and NIL: */
	    if (file->arf_FileSize > file->arf_BufferSize)
		file->arf_PacketPending = ASR_PKT_IDLE;
	}
	else
	{
	    bytesArrived = WaitAsyncRPacket(file);
	}

	if (bytesArrived <= 0)
	    goto end;

	/* if the handler returned a partly filled buffer and the target
	 * is past what was returned, the honest thing is to fail. This
	 * is improbable for a seekable filesystem handler, but should
	 * it happen this protects us from the Guru.
	 */
	if (file->arf_SeekOffset >= bytesArrived)
	{
	    SetIoErr(ERROR_SEEK_ERROR);
	    goto end;
	}

	file->arf_CurrentBuf = 1 - file->arf_CurrentBuf;
	file->arf_BytesLeft   = bytesArrived - file->arf_SeekOffset;
	file->arf_Offset      = (APTR)((ULONG)file->arf_Buffers[file->arf_CurrentBuf] + file->arf_SeekOffset);
	file->arf_SeekOffset  = 0;

	/* reset prefetch trigger in case next operation is a short seek backwards */
	file->arf_SequentialBytes = 0;
    }

    /* copy from buffer and also update all counters */
    ch = *(char *)file->arf_Offset;
    file->arf_BytesLeft--;
    file->arf_BufferPos++;
    file->arf_Offset = (APTR)((ULONG)file->arf_Offset + 1);

    ret = (LONG)ch;

end:
    return(ret);
}

