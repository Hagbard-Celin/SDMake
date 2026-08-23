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
#include <limits.h>

/*****************************************************************************/


LONG SeekAsyncR(AsyncRFile *file, LONG position, AsyncRSeekModes mode)
{
    ULONG current, target;
    ULONG minBuf, maxBuf;
    ULONG roundTarget;
    ULONG seekOffset;

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

    if (file->af_PacketPending == ASR_PKT_START)
    {
	LONG bytesArrived;

	bytesArrived = WaitAsyncRPacket(file);
	if (bytesArrived <= 0)
	    goto err;

	file->af_BytesLeft   = bytesArrived;

    }

    current = file->af_BufferPos;

    /* figure out the absolute offset within the file where we must seek to */
    if (mode == ASR_MODE_CURRENT)
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
    else if (mode == ASR_MODE_START)
    {
	/* catch seek past BOF */
	if (position < 0)
	    goto err;

	target = position;
    }
    else /* if (mode == ASR_MODE_END) */
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
    if (file->af_Buffers[1] == 0 && target >= file->af_BytesArrived[file->af_CurrentBuf])
	goto err;

    seekOffset = file->af_SeekOffset;
    file->af_SeekOffset = 0;
    file->af_SequentialBytes = 0;

    /* we must handle pending packets here or we might get wrong data on
     * next sequential buffer fill
     */
    if (file->af_PacketPending == ASR_PKT_PENDING)
    {
	LONG bytesArrived;

	bytesArrived = WaitAsyncRPacket(file);

	/* but we keep the IoErr from the read on error, since the target
	 * might not be in the buffer that failed. So IoErr other than
	 * ERROR_SEEK_ERROR indicates a retry might succeed.
	 */
	if (bytesArrived == -1)
	    goto err_gotIoErr;

	/* in case of multiple SeekAsyncR() calls back to back end in SendAsyncRPacket()
	 * and the last Seek() fails, this keeps the state consistent so a read
	 * following a failed seek will read from the position of the last
	 * successful seek
	 */
	if (bytesArrived > 0)
	    file->af_PacketPending = ASR_PKT_READY;
    }

    /* figure out what range of the file is in our current buffer */
    minBuf = file->af_BufMin[file->af_CurrentBuf];
    maxBuf = minBuf + file->af_BytesArrived[file->af_CurrentBuf] - 1;

    if (file->af_BytesArrived[file->af_CurrentBuf] && target >= minBuf && target <= maxBuf)
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

	if (file->af_Buffers[1])
	    file->af_PacketPending = ASR_PKT_IDLE;

	goto end;
    }
    else
    if (file->af_BytesArrived[1 - file->af_CurrentBuf])
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
	    file->af_PacketPending = ASR_PKT_IDLE;
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

    if (SendAsyncRPacket(file, file->af_Buffers[1 - file->af_CurrentBuf], roundTarget))
    {
	file->af_SeekOffset = seekOffset;
	goto err_gotIoErr;
    }

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
