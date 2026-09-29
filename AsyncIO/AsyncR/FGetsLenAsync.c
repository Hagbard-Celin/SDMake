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

static ULONG CopyLine(CONST_STRPTR source, STRPTR dest, ULONG size);


STRPTR FGetsAsyncR(AsyncRFile *file, STRPTR buffer, ULONG numBytes)
{
    return (FGetsLenAsyncR(file, buffer, numBytes, NULL));
}


STRPTR FGetsLenAsyncR(AsyncRFile *file, STRPTR buffer, ULONG numBytes, ULONG *len)
{
    LONG totalBytes;
    LONG bytesArrived;
    LONG lineBytes;
    STRPTR ret = buffer;
    BOOL reFill = FALSE;

    totalBytes = 0;

    SetIoErr(0);

    if (!numBytes)
    {
	ret = NULL;
	goto end;
    }

    if (!--numBytes)
    {
	ret = NULL;
	SetIoErr(ERROR_LINE_TOO_LONG);
	goto end;
    }

    /* wait for the buffer to fill if this is the first read after open */
    if (file->arf_PacketPending == ASR_PKT_START)
    {
	bytesArrived = WaitAsyncRPacket(file);
	if (bytesArrived <= 0)
	{
	    ret	= NULL;
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
		    ret	= NULL;
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
	    lineBytes = CopyLine((STRPTR)file->arf_Offset, buffer, numBytes);

	    file->arf_BytesLeft -= lineBytes;
	    file->arf_BufferPos += lineBytes;
	    totalBytes         += lineBytes;
	    file->arf_Offset     = (APTR)((ULONG)file->arf_Offset + lineBytes);
	    buffer[lineBytes] = 0;
	    break;
	}
	else
	{
	    /* drain buffer */
	    if (file->arf_BytesLeft)
	    {
		lineBytes = CopyLine((STRPTR)file->arf_Offset, (STRPTR)buffer, file->arf_BytesLeft);

		numBytes           -= lineBytes;
		file->arf_BufferPos += lineBytes;
		buffer              = (APTR)((ULONG)buffer + lineBytes);
		totalBytes         += lineBytes;
		file->arf_BytesLeft -= lineBytes;
		file->arf_Offset     = (APTR)((ULONG)file->arf_Offset + lineBytes);

		if (buffer[-1] == '\n')
		{
		    *buffer = 0;
		    break;
		}
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
		if (totalBytes)
		{
		    *buffer = 0;

		    if (bytesArrived == 0)
			break;
		}

		ret = NULL;
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
		    ret = NULL;
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
			if (totalBytes)
			    *buffer = 0;

			ret = NULL;
			break;
		    }
		}
	    }
	}
    }
end:
    if (len)
	*len = totalBytes;

    return (ret);
}

static ULONG CopyLine(CONST_STRPTR source, STRPTR dest, ULONG size)
{
    ULONG i = 0;

    do
    {
	*dest++ = *source;
    } while (++i < size && *source++ != '\n');

    return i;
}

