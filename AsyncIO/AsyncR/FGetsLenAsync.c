#include "async_internal.h"

/*****************************************************************************/

static ULONG CopyLine(CONST_STRPTR source, STRPTR dest, ULONG size);


STRPTR FGetsAsync(AsyncFile *file, STRPTR buffer, ULONG numBytes)
{
    return (FGetsLenAsync(file, buffer, numBytes, NULL));
}


STRPTR FGetsLenAsync(AsyncFile *file, STRPTR buffer, ULONG numBytes, ULONG *len)
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
    if (file->af_PacketPending == PKT_START)
    {
	bytesArrived = WaitPacket(file);
	if (bytesArrived <= 0)
	{
	    ret	= NULL;
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
	    if (file->af_SequentialBytes < SEQBYTESTHRESH)
	        file->af_SequentialBytes += numBytes;

	    if (file->af_SequentialBytes >= SEQBYTESTHRESH ||
	        numBytes > file->af_BytesLeft)
	    {
		if (SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], nextpos))
		{
		    ret	= NULL;
		    goto end;
		}

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
	    *buffer = 0;
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

		if (buffer[-1] == '\n' || !numBytes)
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
		UWORD fillBuffer = file->af_CurrentBuf;

		file->af_CurrentBuf  = 1 - file->af_CurrentBuf;

		if (file->af_SeekOffset > bytesArrived)
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

