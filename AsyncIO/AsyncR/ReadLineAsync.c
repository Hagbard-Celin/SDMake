#include "async_internal.h"

/*****************************************************************************/

static ULONG CopyLineEOL(CONST_STRPTR source, STRPTR dest, ULONG size, BOOL *got_eol);
static ULONG GetEOL(CONST_STRPTR buffer, ULONG size, BOOL *spilled_eol);
static BOOL SpillToEOL(AsyncFile *file, BOOL *got_eol);


LONG ReadLineAsync(AsyncFile *file, STRPTR buffer, LONG numBytes)
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
    if (file->af_PacketPending == PKT_START)
    {
	bytesArrived = WaitPacket(file);
	if (bytesArrived <= 0)
	{
	    if (bytesArrived == 0)
		goto end;

	    totalBytes = -1;
	    goto end;
	}

	if (file->af_FileSize)
	{
	    /* handle small file and single buffer modes */
	    if (file->af_FileSize > file->af_BufferSize)
	    {
		if (file->af_FileSize < file->af_BufferSize << 1)
		    file->af_Packet.sp_Pkt.dp_Arg3 = file->af_FileSize - file->af_Packet.sp_Pkt.dp_Arg3;
	    }
	    else
		file->af_PacketPending = PKT_READY;
	}

	file->af_BytesLeft   = bytesArrived;
    }

    /* do we need to send packet to fill other buffer? */
    if (file->af_PacketPending == PKT_IDLE)
    {
	ULONG nextpos;

	nextpos = file->af_BufMin[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf];

	/* do the other buffer already contain the data we need */
	if (file->af_BufMin[1 - file->af_CurrentBuf] == nextpos)
	{
	    file->af_PacketPending = PKT_READY;
	}
	else
	{
	    BOOL sequential = FALSE;

	    if (file->af_SequentialBytes < SEQBYTESTHRESH)
		file->af_SequentialBytes += numBytes;

	    if (file->af_SequentialBytes >= SEQBYTESTHRESH)
		sequential = TRUE;

	    if (sequential || numBytes > file->af_BytesLeft || file->af_BytesLeft < BYTESLEFTTHRESH)
	    {
		if (SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], nextpos))
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

		if (got_eol || !numBytes)
		{
		    *buffer = 0;
		    break;
		}
	    }

	    if (file->af_PacketPending == PKT_READY)
	    {
		bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];

		if (file->af_FileSize > file->af_BufferSize)
		    file->af_PacketPending = PKT_IDLE;
	    }
	    else
		bytesArrived = WaitPacket(file);

	    if (bytesArrived <= 0)
	    {
		if (bytesArrived < 0)
		{
		    totalBytes = -1;
		    goto end;
		}

		*buffer = 0;
		break;
	    }
	    else
	    {
		UWORD fillBuffer;

		file->af_CurrentBuf  = 1 - file->af_CurrentBuf;

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
		    if (SendPacket(file, file->af_Buffers[fillBuffer], file->af_BufMin[file->af_CurrentBuf] + bytesArrived))
		    {
			totalBytes = -1;
			goto end;
		    }

		    if (fillBuffer == file->af_CurrentBuf)
			file->af_CurrentBuf  = 1 - file->af_CurrentBuf;
		}
	    }
	}
    }

    if (totalBytes > 0 && !got_eol)
    {
	if (SpillToEOL(file, &got_eol))
	{
	    if (got_eol)
		buffer[-1] = '\n';
	}
	else
	    totalBytes = -1;
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

static BOOL SpillToEOL(AsyncFile *file, BOOL *got_eol)
{
    LONG bytesArrived;
    LONG spilledBytes;
    BOOL ret = TRUE;

    if (file->af_PacketPending == PKT_IDLE)
    {
	if (SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf]))
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
		if (file->af_PacketPending == PKT_IDLE)
		{
		    if (file->af_SequentialBytes >= SEQBYTESTHRESH ||
			file->af_BytesLeft < BYTESLEFTTHRESH)
		    {
			if (SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + file->af_BytesArrived[file->af_CurrentBuf]))
			{
			    ret = FALSE;
			    break;
			}
		    }
		}
		break;
	    }
	}

	if (file->af_PacketPending == PKT_READY)
	{
	    bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];

	    if (file->af_FileSize > file->af_BufferSize)
		file->af_PacketPending = PKT_IDLE;
	}
	else
	    bytesArrived = WaitPacket(file);

	if (bytesArrived <= 0)
	{
	    if (bytesArrived < 0)
		ret = FALSE;

	    break;
	}
	else
	{
	    file->af_CurrentBuf  = 1 - file->af_CurrentBuf;
	    file->af_BytesLeft   = bytesArrived;
	    file->af_Offset      = file->af_Buffers[file->af_CurrentBuf];

	    /* we have no idea where the line ends, so the packet must be sent in case we exhaust the other buffer */
	    if (SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_BufMin[file->af_CurrentBuf] + bytesArrived))
	    {
		ret = FALSE;
		break;
	    }
	}
    }

    return (ret);
}

