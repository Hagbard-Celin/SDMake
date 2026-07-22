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
    WORD reFill = FALSE;

    totalBytes = 0;

    if (!numBytes)
    {
	buffer = 0;
	SetIoErr(0);
	goto end;
    }

    if (!--numBytes)
    {
	buffer = 0;
	SetIoErr(ERROR_LINE_TOO_LONG);
	goto end;
    }

    /* wait for the buffer to fill if this is the first read after open */
    if (file->af_PacketPending == PKT_START)
    {
	bytesArrived = WaitPacket(file);
	if (bytesArrived <= 0)
	{
	    buffer = 0;
	    goto end;
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
		    buffer = 0;
		    goto end;
		}

	        reFill = TRUE;
	    }
	}
    }

    do
    {
	if (numBytes <= file->af_BytesLeft)
	{
	    if (file->af_PacketPending == PKT_IDLE && reFill)
		SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], file->af_FilesysPos);

	    lineBytes = CopyLine((STRPTR)file->af_Offset, buffer, numBytes);
	    buffer[lineBytes]   = 0;
	    file->af_BytesLeft -= lineBytes;
	    file->af_BufferPos += lineBytes;
	    totalBytes         += lineBytes;
	    file->af_Offset     = (APTR)((ULONG)file->af_Offset + lineBytes);
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
		if (buffer[lineBytes - 1] == '\n')
		{
		    buffer[lineBytes] = 0;
		    break;
		}
	    }
	    else
		lineBytes = 0;

	    if (file->af_PacketPending == PKT_READY)
	    {
		bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];

		file->af_PacketPending = PKT_IDLE;
	    }
	    else
		bytesArrived = WaitPacket(file);

	    if (bytesArrived <= 0)
	    {
		if (totalBytes)
		{
		    buffer[lineBytes] = 0;

		    if (bytesArrived == 0)
		        break;
		}

		buffer = 0;
		break;
	    }

	    file->af_BytesLeft   = bytesArrived - file->af_SeekOffset;

	    if (numBytes > file->af_BytesLeft)
	    {
		if (SendPacket(file, file->af_Buffers[file->af_CurrentBuf], file->af_BufMin[1 - file->af_CurrentBuf] + bytesArrived))
		{
		    if (totalBytes)
			buffer[lineBytes] = 0;
		    buffer = 0;
		    break;
		}
	    }

	    file->af_CurrentBuf  = 1 - file->af_CurrentBuf;
	    file->af_Offset      = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + file->af_SeekOffset);
	    file->af_SeekOffset  = 0;
	}
    } while (numBytes);
end:
    if (len)
	*len = totalBytes;

    return (buffer);
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

