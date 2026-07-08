#include "async_internal.h"

/*****************************************************************************/


LONG ReadAsync(AsyncFile *file, APTR buffer, LONG numBytes)
{
    LONG totalBytes;
    LONG bytesArrived;
    WORD reFill = FALSE;

    totalBytes = 0;

    /* wait for the buffer to fill if this is the first read after open */
    if (file->af_PacketPending == PKT_START)
    {
	bytesArrived = WaitPacket(file);
	if (bytesArrived <= 0)
	{
	    if (bytesArrived == 0)
	    {
	        SetIoErr(0);
		goto end;
	    }

	    totalBytes = -1;
	    goto end;
	}

	file->af_BytesLeft   = bytesArrived;
    }

    /* do we need to send packet to fill other buffer? */
    if (file->af_PacketPending == PKT_IDLE)
    {
	LONG nextpos;

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
		    return(-1);

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
		bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];
	    else
		bytesArrived = WaitPacket(file);

	    if (bytesArrived <= 0)
	    {
	        if (bytesArrived == 0)
		{
		    SetIoErr(0);
		    break;
		}

		totalBytes = -1;
		break;
	    }

	    file->af_BytesLeft   = bytesArrived - file->af_SeekOffset;

	    if (numBytes > file->af_BytesLeft)
		SendPacket(file, file->af_Buffers[file->af_CurrentBuf], file->af_BufMin[1 - file->af_CurrentBuf] + bytesArrived);

	    file->af_CurrentBuf  = 1 - file->af_CurrentBuf;
	    file->af_Offset      = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + file->af_SeekOffset);
	    file->af_SeekOffset  = 0;
	}
    } while (numBytes);
end:
    return (totalBytes);
}
