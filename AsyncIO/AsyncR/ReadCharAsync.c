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
    if (file->af_PacketPending == ASR_PKT_START)
    {
	bytesArrived = WaitAsyncRPacket(file);

	if (bytesArrived <= 0)
	    goto end;

	file->af_BytesLeft   = bytesArrived;
    }

    if (file->af_PacketPending == ASR_PKT_IDLE)
    {
	ULONG nextpos;

	nextpos = file->af_BufMin[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf];

	/* do the other buffer already contain the data we need */
	if (nextpos && file->af_BufMin[1 - file->af_CurrentBuf] == nextpos)
	{
	    file->af_PacketPending = ASR_PKT_READY;
	}
	else
	{
	    if (file->af_SequentialBytes < ASR_SEQBYTESTHRESH)
		file->af_SequentialBytes++;

	    if (file->af_SequentialBytes >= ASR_SEQBYTESTHRESH ||
		file->af_BytesLeft < ASR_BYTESLEFTTHRESH)
	    {
		if (SendAsyncRPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], nextpos))
		    goto end;
	    }
	}
    }

    if (!file->af_BytesLeft)
    {
	if (file->af_PacketPending == ASR_PKT_READY)
	{
	    bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];

	    if (file->af_Buffers[1])
		file->af_PacketPending = ASR_PKT_IDLE;
	}
	else
	{
bad_handler:
	    bytesArrived = WaitAsyncRPacket(file);
	}

	if (bytesArrived <= 0)
	    goto end;

	file->af_CurrentBuf = 1 - file->af_CurrentBuf;

	if (file->af_SeekOffset >= bytesArrived)
	{
	    /* we arrive here if we have been seeking and the handler we read from
	     * does not wait until the requested buffer is filled or EOF before replying.
	     * We recycle the buffer we just filled to minimize the damage.
	     * But this code is NOT suited for reading from such handlers.
	     */
	    file->af_BytesLeft   = 0;
	    file->af_Offset      = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf]);
	    file->af_SeekOffset -= bytesArrived;

	    if (SendAsyncRPacket(file, file->af_Buffers[file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + bytesArrived))
		goto end;

	    file->af_CurrentBuf = 1 - file->af_CurrentBuf;

	    goto bad_handler;
	}
	else
	{
	    file->af_BytesLeft   = bytesArrived - file->af_SeekOffset;
	    file->af_Offset      = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + file->af_SeekOffset);
	    file->af_SeekOffset  = 0;
	}

	/* reset prefetch trigger in case next operation is a short seek backwards */
	file->af_SequentialBytes = 0;
    }

    /* copy from buffer and also update all counters */
    ch = *(char *)file->af_Offset;
    file->af_BytesLeft--;
    file->af_BufferPos++;
    file->af_Offset = (APTR)((ULONG)file->af_Offset + 1);

    ret = (LONG)ch;

end:
    return(ret);
}

