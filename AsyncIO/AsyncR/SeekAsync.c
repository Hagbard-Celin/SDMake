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
    if (!file->arf_FileSize)
	goto err;

    if (file->arf_PacketPending == ASR_PKT_START)
    {
	LONG bytesArrived;

	bytesArrived = WaitAsyncRPacket(file);
	if (bytesArrived <= 0)
	    goto err;

	file->arf_BytesLeft   = bytesArrived;

    }

    current = file->arf_BufferPos;

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
	if (-position > file->arf_FileSize)
	    goto err;

	target = file->arf_FileSize + position;
    }

    /* catch seek to or past EOF, we catch both since allowing seek
     * to EOF would break single buffer mode for small files.
     * And intentionally sending a packet to fill a buffer from
     * EOF does not make sense anyway.
     */

    if (target >= file->arf_FileSize)
	goto err;

    /* if we are in single buffer mode and the handler returned a partly
     * filled buffer and the target is past what was returned, the only
     * option is to fail. This is improbable for a seekable filesystem
     * handler, but should it happen this protects us from the Guru.
     */
    if (file->arf_Buffers[1] == 0 && target >= file->arf_BytesArrived[file->arf_CurrentBuf])
	goto err;

    seekOffset = file->arf_SeekOffset;
    file->arf_SeekOffset = 0;
    file->arf_SequentialBytes = 0;

    /* we must handle pending packets here or we might get wrong data on
     * next sequential buffer fill
     */
    if (file->arf_PacketPending == ASR_PKT_PENDING)
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
	    file->arf_PacketPending = ASR_PKT_READY;
    }

    /* figure out what range of the file is in our current buffer */
    minBuf = file->arf_BufMin[file->arf_CurrentBuf];
    maxBuf = minBuf + file->arf_BytesArrived[file->arf_CurrentBuf] - 1;

    if (file->arf_BytesArrived[file->arf_CurrentBuf] && target >= minBuf && target <= maxBuf)
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

	file->arf_BytesLeft  = maxBuf + 1 - target;
	file->arf_BufferPos  = target;
	file->arf_Offset     = (APTR)((ULONG)file->arf_Buffers[file->arf_CurrentBuf] + (target - minBuf));

	/* keep ASR_PKT_READY for single buffer mode */
	if (file->arf_Buffers[1])
	    file->arf_PacketPending = ASR_PKT_IDLE;

	goto end;
    }
    else
    if (file->arf_BytesArrived[1 - file->arf_CurrentBuf])
    {
	/* the other buffer contains valid data. Figure out what range of the file is
	 * in that buffer, and check if the target location is within that range.
	 */

	minBuf = file->arf_BufMin[1 - file->arf_CurrentBuf];
	maxBuf = minBuf + file->arf_BytesArrived[1 - file->arf_CurrentBuf] - 1;

	if (target >= minBuf && target <= maxBuf)
	{
	    file->arf_CurrentBuf = 1 - file->arf_CurrentBuf;
	    file->arf_Offset     = (APTR)((ULONG)file->arf_Buffers[file->arf_CurrentBuf] + (target - minBuf));
	    file->arf_BytesLeft  = maxBuf + 1 - target;
	    file->arf_BufferPos  = target;
	    file->arf_PacketPending = ASR_PKT_IDLE;
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

    /* changed to align to arf_BufferSize, this helps avoid unnecessary
     * reads under some conditions
     */
    roundTarget = (target / file->arf_BufferSize) * file->arf_BufferSize;

    if (SendAsyncRPacket(file, file->arf_Buffers[1 - file->arf_CurrentBuf], roundTarget))
    {
	file->arf_SeekOffset = seekOffset;
	goto err_gotIoErr;
    }

    file->arf_BufferPos  = target;
    file->arf_BytesLeft  = 0;
    file->arf_SeekOffset = target - roundTarget;

end:
    SetIoErr(0);
    return((LONG)current);

err:
    SetIoErr(ERROR_SEEK_ERROR);
err_gotIoErr:
    return -1;
}
