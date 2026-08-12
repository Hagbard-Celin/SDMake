#include "async_internal.h"

/*****************************************************************************/


LONG ReadAsync(AsyncFile *file, APTR buffer, LONG numBytes)
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

    do
    {
	if (numBytes <= file->af_BytesLeft)
	{
	    CopyMem(file->af_Offset,buffer,numBytes);
	    file->af_BytesLeft -= numBytes;
	    file->af_BufferPos += numBytes;
	    totalBytes         += numBytes;
	    file->af_Offset     = (APTR)((ULONG)file->af_Offset + numBytes);
	    break;
	}
	else
	{
	    /* drain buffer */
	    if (file->af_BytesLeft)
	    {
		CopyMem(file->af_Offset,buffer,file->af_BytesLeft);

		numBytes           -= file->af_BytesLeft;
		file->af_BufferPos += file->af_BytesLeft;
		buffer              = (APTR)((ULONG)buffer + file->af_BytesLeft);
		totalBytes         += file->af_BytesLeft;
		file->af_BytesLeft  = 0;
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
		    totalBytes = -1;

		break;
	    }
	    else
	    {
		UWORD fillBuffer;

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
			totalBytes = -1;
			break;
		    }
	        }
	    }
	}
    } while (numBytes);
end:
    return (totalBytes);
}
