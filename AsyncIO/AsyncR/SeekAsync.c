#include "async_internal.h"
#include <limits.h>

/*****************************************************************************/


LONG SeekAsync(AsyncFile *file, LONG position, SeekModes mode)
{
    ULONG  current, target;
    ULONG  minBuf, maxBuf;
    LONG   bytesArrived;
    ULONG  roundTarget;

    /* we fail if one of the following is true:
     * 1. locking an open file failed
     * 2. Examine() failed
     * 3. Examine() reported fib_Size of 0
     * The first two indicates we are probably dealing with a interactive file.
     * The third can be either that or a empty file, which are treated the
     * same for simplicity.
     */
    if (!file->af_FileSize)
	goto err;

    if (file->af_PacketPending == PKT_START)
    {
	bytesArrived = WaitPacket(file);
	if (bytesArrived <= 0)
	    goto err;

	/* handle small file and single buffer modes */
	if (file->af_FileSize > file->af_BufferSize)
	{
	    if (file->af_FileSize < file->af_BufferSize << 1)
		file->af_Packet.sp_Pkt.dp_Arg3 = file->af_FileSize - file->af_Packet.sp_Pkt.dp_Arg3;
	}
	else
	    file->af_PacketPending = PKT_READY;

	file->af_BytesLeft   = bytesArrived;

    }

    bytesArrived = 0;
    current = file->af_BufferPos;

    /* figure out the absolute offset within the file where we must seek to */
    if (mode == MODE_CURRENT)
    {
	if (!position)
	    goto end;

	/* catch seek past UINT_MAX  */
	if (position > 0 && current > UINT_MAX - position)
	    goto err;

	/* catch seek past BOF */
	if (position < 0 && -position > current)
	    goto err;

	target = current + position;
    }
    else if (mode == MODE_START)
    {
	/* catch seek past BOF */
	if (position < 0)
	    goto err;

	target = position;
    }
    else /* if (mode == MODE_END) */
    {
	/* catch seek to or past EOF */
	if (position >= 0)
	    goto err;

	/* catch seek past BOF */
	if (-position > file->af_FileSize)
	    goto err;

	target = file->af_FileSize + position;
    }

    /* catch seek to or past EOF, we catch both since allowing seek
     * to EOF would break single buffer mode for small files.
     * And intentionally sending a packet to fill a buffer from
     * EOF does not make sense anyway.
     */

    if (target >= file->af_FileSize)
	goto err;

    /* if we are in single buffer mode and the handler returned a partly
     * filled buffer and the target is past what was returned, the only
     * option is to fail
     */
    if (file->af_Buffers[1] == 0 && target > file->af_BytesArrived[file->af_CurrentBuf])
	goto err;

    file->af_SequentialBytes = 0;

    /* figure out what range of the file is in our current buffer */
    minBuf = file->af_BufMin[file->af_CurrentBuf];
    maxBuf = minBuf + file->af_BytesArrived[file->af_CurrentBuf] - 1;


    if (target >= minBuf && target <= maxBuf)
    {
	/* one of the two following things is true:
	 *
	 * 1. The target seek location is within the current read buffer,
	 * but before the current location within the buffer. Move back
	 * within the buffer.
	 *
	 * 2. The target seek location is ahead within the current
	 * read buffer. Advance to that location.
	 */

	file->af_BytesLeft  = maxBuf + 1 - target;
	file->af_BufferPos  = target;
	file->af_Offset     = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + (target - minBuf));
	goto end;
    }
    else
    if (file->af_PacketPending == PKT_PENDING && (bytesArrived = WaitPacket(file)) > 0)
    {
	/* the other buffer is being filled by the filesystem. Wait for this to be done,
	 * then figure out what range of the file appeared in the buffer, and check if
	 * the target location is within that range.
	 */

	minBuf = file->af_BufMin[1 - file->af_CurrentBuf];
	maxBuf = minBuf + file->af_BytesArrived[1 - file->af_CurrentBuf] - 1;

	if (target >= minBuf && target <= maxBuf)
	{
	    file->af_CurrentBuf = 1 - file->af_CurrentBuf;
	    file->af_Offset     = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + (target - minBuf));
	    file->af_BytesLeft  = maxBuf + 1 - target;
	    file->af_BufferPos  = target;
	    goto end;
	}
    }
    else
    if (bytesArrived == -1)
    {
	goto err;
    }
    else
    if (file->af_BytesArrived[1 - file->af_CurrentBuf] &&
	(file->af_PacketPending == PKT_IDLE || file->af_PacketPending == PKT_READY))
    {
	/* the other buffer contains valid data. Figure out what range of the file is
	 * in that buffer, and check if the target location is within that range.
	 */

	minBuf = file->af_BufMin[1 - file->af_CurrentBuf];
	maxBuf = minBuf + file->af_BytesArrived[1 - file->af_CurrentBuf] - 1;

	if (target >= minBuf && target <= maxBuf)
	{
	    file->af_CurrentBuf = 1 - file->af_CurrentBuf;
	    file->af_Offset     = (APTR)((ULONG)file->af_Buffers[file->af_CurrentBuf] + (target - minBuf));
	    file->af_BytesLeft  = maxBuf + 1 - target;
	    file->af_BufferPos  = target;
	    file->af_PacketPending = PKT_IDLE;
	    goto end;
	}
    }

    /* if we arrive here the target seek location isn't currently in
     * our buffers, so move the actual file pointer to the desired
     * location, and then restart the async read thing...
     */

    /* this is to keep our file reading block-aligned on the device.
     * block-aligned reads are generally quite a bit faster, so it is
     * worth the trouble to keep things aligned
     */

    /* changed to align to af_BufferSize, this helps avoid unnecessary
     * reads under some conditions
     */
    roundTarget = (target / file->af_BufferSize) * file->af_BufferSize;

    if (SendPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], roundTarget))
	goto err_gotIoErr;

    file->af_BufferPos  = target;
    file->af_BytesLeft  = 0;
    file->af_SeekOffset = target - roundTarget;

end:
    SetIoErr(0);
    return((LONG)current);

err:
    SetIoErr(ERROR_SEEK_ERROR);
err_gotIoErr:
    return -1;
}
