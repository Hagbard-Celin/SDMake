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


LONG ReadAsyncR(AsyncRFile *file, APTR buffer, LONG numBytes)
{
    LONG totalBytes;
    LONG bytesArrived;
    BOOL reFill = FALSE;

    if (numBytes <= 0)
    {
	totalBytes = -1;
	SetIoErr(ERROR_LINE_TOO_LONG);
	goto end;
    }

    totalBytes = 0;

    SetIoErr(0);

    /* wait for the buffer to fill if this is the first read after open */
    if (file->arf_PacketPending == ASR_PKT_START)
    {
	bytesArrived = WaitAsyncRPacket(file);
	if (bytesArrived <= 0)
	{
	    if (bytesArrived == 0)
		goto end;

	    totalBytes = -1;
	    goto end;
	}

	file->arf_BytesLeft   = bytesArrived;
    }

    /* do we need to send packet to fill other buffer? */
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
	    BOOL sequential = FALSE;

	    if (file->arf_SequentialBytes < ASR_SEQBYTESTHRESH)
		file->arf_SequentialBytes += numBytes;

	    if (file->arf_SequentialBytes >= ASR_SEQBYTESTHRESH)
		sequential = TRUE;

	    if (sequential || numBytes > file->arf_BytesLeft || file->arf_BytesLeft < ASR_BYTESLEFTTHRESH)
	    {
		if (SendAsyncRPacket(file, file->arf_Buffers[1 - file->arf_CurrentBuf], nextpos))
		{
		    totalBytes = -1;
		    goto end;
		}

		if (sequential)
		    reFill = TRUE;
	    }
	}
    }

    while (TRUE)
    {
	if (numBytes <= file->arf_BytesLeft)
	{
	    CopyMem(file->arf_Offset,buffer,numBytes);

	    file->arf_BytesLeft -= numBytes;
	    file->arf_BufferPos += numBytes;
	    totalBytes         += numBytes;
	    file->arf_Offset     = (APTR)((ULONG)file->arf_Offset + numBytes);
	    break;
	}
	else
	{
	    /* drain buffer */
	    if (file->arf_BytesLeft)
	    {
		CopyMem(file->arf_Offset,buffer,file->arf_BytesLeft);

		numBytes           -= file->arf_BytesLeft;
		file->arf_BufferPos += file->arf_BytesLeft;
		buffer              = (APTR)((ULONG)buffer + file->arf_BytesLeft);
		totalBytes         += file->arf_BytesLeft;
		file->arf_Offset     = (APTR)((ULONG)file->arf_Offset + file->arf_BytesLeft);
		file->arf_BytesLeft  = 0;
	    }

	    if (file->arf_PacketPending == ASR_PKT_READY)
	    {
		bytesArrived = file->arf_BytesArrived[1 - file->arf_CurrentBuf];

		/* keep ASR_PKT_READY for single buffer mode and NIL: */
		if (file->arf_FileSize > file->arf_BufferSize)
		    file->arf_PacketPending = ASR_PKT_IDLE;
	    }
	    else
		bytesArrived = WaitAsyncRPacket(file);

	    if (bytesArrived <= 0)
	    {
		if (bytesArrived < 0)
		    totalBytes = -1;

		break;
	    }
	    else
	    {
		/* if the handler returned a partly filled buffer and the target
		 * is past what was returned, the honest thing is to fail. This
		 * is improbable for a seekable filesystem handler, but should
		 * it happen this protects us from the Guru.
		 */
		if (file->arf_SeekOffset >= bytesArrived)
		{
		    SetIoErr(ERROR_SEEK_ERROR);
		    totalBytes = -1;
		    break;
		}

		file->arf_CurrentBuf = 1 - file->arf_CurrentBuf;
		file->arf_BytesLeft   = bytesArrived - file->arf_SeekOffset;
		file->arf_Offset      = (APTR)((ULONG)file->arf_Buffers[file->arf_CurrentBuf] + file->arf_SeekOffset);
		file->arf_SeekOffset  = 0;

		/* send packet if we will exhaust the other buffer in next iteration,
		 * or if the sequential read detection heuristics has triggered
		 */
		if (numBytes > file->arf_BytesLeft || reFill)
		{
		    if (SendAsyncRPacket(file, file->arf_Buffers[1 - file->arf_CurrentBuf], file->arf_BufMin[file->arf_CurrentBuf] + bytesArrived))
		    {
			totalBytes = -1;
			break;
		    }
	        }
	    }
	}
    }
end:
    return (totalBytes);
}
