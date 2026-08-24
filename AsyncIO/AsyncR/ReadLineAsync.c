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

static ULONG CopyLineEOL(CONST_STRPTR source, STRPTR dest, ULONG size, BOOL *got_eol);
static ULONG GetEOL(CONST_STRPTR buffer, ULONG size, BOOL *spilled_eol);
static BOOL SpillToEOL(AsyncRFile *file, BOOL *got_eol);


LONG ReadLineAsyncR(AsyncRFile *file, STRPTR buffer, LONG numBytes)
{
    LONG totalBytes;
    LONG bytesArrived;
    LONG lineBytes;
    BOOL reFill = FALSE;
    BOOL got_eol = FALSE;

    totalBytes = 0;

    SetIoErr(0);

    if (numBytes <= 0 || !--numBytes)
    {
	totalBytes = -1;
	SetIoErr(ERROR_LINE_TOO_LONG);
	goto end;
    }

    /* wait for the buffer to fill if this is the first read after open */
    if (file->af_PacketPending == ASR_PKT_START)
    {
	bytesArrived = WaitAsyncRPacket(file);
	if (bytesArrived <= 0)
	{
	    if (bytesArrived == 0)
		goto end;

	    totalBytes = -1;
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
	if (numBytes <= file->af_BytesLeft)
	{
	    lineBytes = CopyLineEOL((STRPTR)file->af_Offset, buffer, numBytes, &got_eol);

	    file->af_BufferPos += lineBytes;
	    buffer              = (APTR)((ULONG)buffer + lineBytes);
	    totalBytes         += lineBytes;
	    file->af_BytesLeft -= lineBytes;
	    file->af_Offset     = (APTR)((ULONG)file->af_Offset + lineBytes);
	    *buffer             = 0;
	    break;
	}
	else
	{
	    /* drain buffer */
	    if (file->af_BytesLeft)
	    {
		lineBytes = CopyLineEOL((STRPTR)file->af_Offset, (STRPTR)buffer, file->af_BytesLeft, &got_eol);

		numBytes           -= lineBytes;
		file->af_BufferPos += lineBytes;
		buffer              = (APTR)((ULONG)buffer + lineBytes);
		totalBytes         += lineBytes;
		file->af_BytesLeft -= lineBytes;
		file->af_Offset     = (APTR)((ULONG)file->af_Offset + lineBytes);

		if (got_eol)
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
		if (bytesArrived < 0)
		{
		    totalBytes = -1;
		    goto end;
		}

		if (totalBytes)
		    *buffer = 0;

		break;
	    }
	    else
	    {
		UWORD fillBuffer;

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
		    fillBuffer           = file->af_CurrentBuf;
		}
		else
		{
		    file->af_BytesLeft   = bytesArrived - file->af_SeekOffset;
		    file->af_Offset      = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + file->af_SeekOffset);
		    file->af_SeekOffset  = 0;
		    fillBuffer           = 1 - file->af_CurrentBuf;
		}

		/* send packet if we will exhaust the other buffer in next iteration,
		 * or if the sequential read detection heuristics has triggered
		 */
		if (numBytes > file->af_BytesLeft || reFill)
		{
		    if (SendAsyncRPacket(file, file->af_Buffers[fillBuffer], file->af_BufMin[file->af_CurrentBuf] + bytesArrived))
		    {
			totalBytes = -1;
			goto end;
		    }

		    if (fillBuffer == file->af_CurrentBuf)
			file->af_CurrentBuf = 1 - file->af_CurrentBuf;
		}
	    }
	}
    }

    if (totalBytes > 0 && !got_eol)
    {
	/* we arrive here if we copied characters but no newline to 'buffer' */
	if (SpillToEOL(file, &got_eol))
	{
	    /* if a newline was found, overwrite last character copied to 'buffer' */
	    if (got_eol)
		buffer[-1] = '\n';
	}
	else
	{
	    /* in case of error during SpillToEOL() we must fail, as we can not
	     * guarantee the read cursor is at the beginning of next line or EOF
	     */
	    totalBytes = -1;
	}
    }

end:
    return (totalBytes);
}

static ULONG CopyLineEOL(CONST_STRPTR source, STRPTR dest, ULONG size, BOOL *got_eol)
{
    ULONG i = 0;

    while (i < size)
    {
	i++;
	*dest++ = *source;

	if (*source++ == '\n')
	    break;
    }

    if (dest[-1] == '\n')
	*got_eol = TRUE;

    return i;
}

static ULONG GetEOL(CONST_STRPTR buffer, ULONG size, BOOL *spilled_eol)
{
    ULONG i = 0;

    while (i < size)
    {
	i++;
	if (*buffer++ == '\n')
	    break;
    }

    if (buffer[-1] == '\n')
	*spilled_eol = TRUE;

    return i;
}

/* this function moves the read cursor forward until it reaches EOF or the
 * first character on the next line, whichever comes first. If it finds
 * newline before EOF it sets *got_eol to TRUE.
 */
static BOOL SpillToEOL(AsyncRFile *file, BOOL *got_eol)
{
    LONG bytesArrived;
    LONG spilledBytes;
    BOOL ret = TRUE;

    if (file->af_PacketPending == ASR_PKT_IDLE)
    {
	if (SendAsyncRPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf]))
	    ret = FALSE;
    }

    while (ret)
    {
	/* spill buffer */
	if (file->af_BytesLeft)
	{
	    spilledBytes = GetEOL((STRPTR)file->af_Offset, file->af_BytesLeft, got_eol);

	    file->af_BufferPos       += spilledBytes;
	    file->af_BytesLeft       -= spilledBytes;
	    file->af_Offset           = (APTR)((ULONG)file->af_Offset + spilledBytes);
	    file->af_SequentialBytes += spilledBytes;
	    if (*got_eol)
	    {
		if (file->af_PacketPending == ASR_PKT_IDLE)
		{
		    if (file->af_SequentialBytes >= ASR_SEQBYTESTHRESH ||
			file->af_BytesLeft < ASR_BYTESLEFTTHRESH)
		    {
			if (SendAsyncRPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf]))
			{
			    ret = FALSE;
			    break;
			}
		    }
		}
		break;
	    }
	}

	if (file->af_PacketPending == ASR_PKT_READY)
	{
	    bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];

	    if (file->af_FileSize > file->af_BufferSize)
		file->af_PacketPending = ASR_PKT_IDLE;
	}
	else
	    bytesArrived = WaitAsyncRPacket(file);

	if (bytesArrived <= 0)
	{
	    if (bytesArrived < 0)
		ret = FALSE;

	    break;
	}
	else
	{
	    file->af_CurrentBuf = 1 - file->af_CurrentBuf;
	    file->af_BytesLeft  = bytesArrived;
	    file->af_Offset     = file->af_Buffers[file->af_CurrentBuf];

	    /* we have no idea where the line ends, so the packet must be sent in case we exhaust the other buffer */
	    if (SendAsyncRPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + bytesArrived))
	    {
		ret = FALSE;
		break;
	    }
	}
    }

    return (ret);
}

