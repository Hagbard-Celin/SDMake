#include "async_internal.h"

/*****************************************************************************/


LONG ReadCharAsync(AsyncFile *file)
{
    unsigned char ch;
    LONG ret = -1;

    if (!file->af_BytesLeft)
    {
	LONG bytesArrived;

	/* wait for the buffer to fill if this is the first read after open */
	if (file->af_PacketPending == PKT_START)
	{
	    bytesArrived = WaitPacket(file);

	    if (bytesArrived <= 0)
	        goto end;

	    file->af_BytesLeft   = bytesArrived;
	}
	else
	{
	    if (file->af_PacketPending == PKT_READY)
	    {
		bytesArrived = file->af_BytesArrived[1 - file->af_CurrentBuf];

		file->af_PacketPending = PKT_IDLE;
	    }
	    else
		bytesArrived = WaitPacket(file);

	    if (bytesArrived <= 0)
	        goto end;

	    file->af_BytesLeft   = bytesArrived - file->af_SeekOffset;

	    file->af_CurrentBuf  = 1 - file->af_CurrentBuf;
	    file->af_Offset      = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + file->af_SeekOffset);
	    file->af_SeekOffset  = 0;

	    /* reset prefetch trigger in case next operation is a short seek backwards */
	    file->af_SequentialBytes = 0;
	}
    }

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
		file->af_SequentialBytes++;

	    if (file->af_SequentialBytes >= SEQBYTESTHRESH ||
		file->af_BytesLeft < BYTESLEFTTHRESH)
	    {
		if (SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], nextpos))
		    goto end;
	    }
	}
    }

    /* copy from buffer and also update all counters */

    ch = *(char *)file->af_Offset;
    file->af_BytesLeft--;
    file->af_BufferPos++;
    file->af_Offset = (APTR)((ULONG)file->af_Offset + 1);

    ret = (LONG)ch;

end:
    return(ret);
}

