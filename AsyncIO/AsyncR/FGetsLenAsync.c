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
    if (file->af_PacketPending == ASR_PKT_START)
    {
	bytesArrived = WaitAsyncRPacket(file);
	if (bytesArrived <= 0)
	{
	    ret	= NULL;
	    goto end;
	}

	file->af_BytesLeft   = bytesArrived;
    }

    /* do we need to send packet to fill other buffer? */
    if (file->af_PacketPending == ASR_PKT_IDLE)
    {
	ULONG nextpos;

	nextpos = file->af_BufMin[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf];

	/* does the other buffer already contain the data we need */
	if (nextpos && file->af_BufMin[1 - file->af_CurrentBuf] == nextpos)
	{
	    file->af_PacketPending = ASR_PKT_READY;
	}
	else
	{
	    BOOL sequential = FALSE;

	    if (file->af_SequentialBytes < ASR_SEQBYTESTHRESH)
		file->af_SequentialBytes += numBytes;

	    if (file->af_SequentialBytes >= ASR_SEQBYTESTHRESH)
		sequential = TRUE;

	    if (sequential || numBytes > file->af_BytesLeft || file->af_BytesLeft < ASR_BYTESLEFTTHRESH)
	    {
		if (SendAsyncRPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], nextpos))
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
	if (numBytes <= file->af_BytesLeft)
	{
	    lineBytes = CopyLine((STRPTR)file->af_Offset, buffer, numBytes);

	    file->af_BytesLeft -= lineBytes;
	    file->af_BufferPos += lineBytes;
	    totalBytes         += lineBytes;
	    file->af_Offset     = (APTR)((ULONG)file->af_Offset + lineBytes);
	    buffer[lineBytes] = 0;
	    break;
	}
	else
	{
	    /* drain buffer */
	    if (file->af_BytesLeft)
	    {
		lineBytes = CopyLine((STRPTR)file->af_Offset, (STRPTR)buffer, file->af_BytesLeft);

		numBytes           -= lineBytes;
		file->af_BufferPos += lineBytes;
		buffer              = (APTR)((ULONG)buffer + lineBytes);
		totalBytes         += lineBytes;
		file->af_BytesLeft -= lineBytes;
		file->af_Offset     = (APTR)((ULONG)file->af_Offset + lineBytes);

		if (buffer[-1] == '\n')
		{
		    *buffer = 0;
		    break;
		}
	    }

	    if (file->af_PacketPending == ASR_PKT_READY)
	    {
		bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];

		/* keep ASR_PKT_READY for single buffer mode and NIL: */
		if (file->af_FileSize > file->af_BufferSize)
		    file->af_PacketPending = ASR_PKT_IDLE;
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
		if (file->af_SeekOffset >= bytesArrived)
		{
		    SetIoErr(ERROR_SEEK_ERROR);
		    ret = NULL;
		    break;
		}

		file->af_CurrentBuf = 1 - file->af_CurrentBuf;
		file->af_BytesLeft   = bytesArrived - file->af_SeekOffset;
		file->af_Offset      = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + file->af_SeekOffset);
		file->af_SeekOffset  = 0;

		/* send packet if we will exhaust the other buffer in next iteration,
		 * or if the sequential read detection heuristics has triggered
		 */
		if (numBytes > file->af_BytesLeft || reFill)
		{
		    if (SendAsyncRPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + bytesArrived))
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

