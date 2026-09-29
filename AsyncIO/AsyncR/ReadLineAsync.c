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
	    lineBytes = CopyLineEOL((STRPTR)file->arf_Offset, buffer, numBytes, &got_eol);

	    file->arf_BufferPos += lineBytes;
	    buffer              = (APTR)((ULONG)buffer + lineBytes);
	    totalBytes         += lineBytes;
	    file->arf_BytesLeft -= lineBytes;
	    file->arf_Offset     = (APTR)((ULONG)file->arf_Offset + lineBytes);
	    *buffer             = 0;
	    break;
	}
	else
	{
	    /* drain buffer */
	    if (file->arf_BytesLeft)
	    {
		lineBytes = CopyLineEOL((STRPTR)file->arf_Offset, (STRPTR)buffer, file->arf_BytesLeft, &got_eol);

		numBytes           -= lineBytes;
		file->arf_BufferPos += lineBytes;
		buffer              = (APTR)((ULONG)buffer + lineBytes);
		totalBytes         += lineBytes;
		file->arf_BytesLeft -= lineBytes;
		file->arf_Offset     = (APTR)((ULONG)file->arf_Offset + lineBytes);

		if (got_eol)
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
			goto end;
		    }
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

    if (file->arf_PacketPending == ASR_PKT_IDLE)
    {
	if (SendAsyncRPacket(file, file->arf_Buffers[1 - file->arf_CurrentBuf], file->arf_BufMin[file->arf_CurrentBuf] + file->arf_BytesArrived[file->arf_CurrentBuf]))
	    ret = FALSE;
    }

    while (ret)
    {
	/* spill buffer */
	if (file->arf_BytesLeft)
	{
	    spilledBytes = GetEOL((STRPTR)file->arf_Offset, file->arf_BytesLeft, got_eol);

	    file->arf_BufferPos       += spilledBytes;
	    file->arf_BytesLeft       -= spilledBytes;
	    file->arf_Offset           = (APTR)((ULONG)file->arf_Offset + spilledBytes);
	    file->arf_SequentialBytes += spilledBytes;

	    if (*got_eol)
		break;
	}

	if (file->arf_PacketPending == ASR_PKT_READY)
	{
	    bytesArrived = file->arf_BytesArrived[1 - file->arf_CurrentBuf];

	    if (file->arf_FileSize > file->arf_BufferSize)
		file->arf_PacketPending = ASR_PKT_IDLE;
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
	    file->arf_CurrentBuf = 1 - file->arf_CurrentBuf;
	    file->arf_BytesLeft  = bytesArrived;
	    file->arf_Offset     = file->arf_Buffers[file->arf_CurrentBuf];

	    /* we have no idea where the line ends, so the packet must be sent in case we exhaust the other buffer */
	    if (SendAsyncRPacket(file, file->arf_Buffers[1 - file->arf_CurrentBuf], file->arf_BufMin[file->arf_CurrentBuf] + bytesArrived))
	    {
		ret = FALSE;
		break;
	    }
	}
    }

    return (ret);
}

