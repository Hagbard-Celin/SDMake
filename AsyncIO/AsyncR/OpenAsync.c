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

#include <proto/exec.h>
#include <proto/dos.h>
#include "asyncr_internal.h"

/*****************************************************************************/


AsyncRFile *OpenAsyncR(const STRPTR fileName, ULONG bufferSize)
{
    AsyncRFile         *file = NULL;
    BPTR               handle;
    LONG               blockSize = 0;
    ULONG              fileSize = 0;
    LONG               err = 0;

    if (!bufferSize)
    {
	err = ERROR_LINE_TOO_LONG;
	goto end;
    }

    {
	BPTR lock;
	LONG lockerr = 0;
	LONG infoerr = 0;
	LONG examerr = 0;
	D_S(struct InfoData,infoData);
	D_S(struct FileInfoBlock,fib);

	if (handle = Open(fileName, MODE_OLDFILE))
	{
	    if (lock = Lock(fileName, ACCESS_READ))
	    {
		if (Info(lock,infoData))
		{
		    blockSize  = infoData->id_BytesPerBlock;
		}
		else
		    infoerr = IoErr();


		if (Examine(lock, fib))
		{
		    fileSize = fib->fib_Size;
		}
		else
		    examerr = IoErr();

		UnLock(lock);
	    }
	    else
		lockerr = IoErr();
	}
	else
	    err = IoErr();

	if (lockerr == ERROR_NO_FREE_STORE ||
	    infoerr == ERROR_NO_FREE_STORE ||
	    examerr == ERROR_NO_FREE_STORE)
	{
	    err = ERROR_NO_FREE_STORE;
	    Close(handle);
	    handle = 0;
	}
    }


    if (handle)
    {
	struct FileHandle *fh;
	ULONG halfbuffersize = 0;
	ULONG doubleblocksize;

	/* if it was possible to obtain a lock on the same device as the
	 * file we're working on, get the block size of that device and
	 * round up our buffer size to be a multiple of the block size.
	 * This maximizes DMA efficiency.
	 */

	if (!blockSize)
	    blockSize = 512;

	/* we do this even if Info() failed to avoid excessive overhead
	 * from too small buffers
	 */
	doubleblocksize = blockSize << 1;
	bufferSize = (((bufferSize + doubleblocksize - 1) / doubleblocksize) * doubleblocksize);

	if (fileSize)
	{
	    ULONG reduced;

	    reduced = bufferSize - doubleblocksize;

	    /* reduce buffer size for small files */
	    while (fileSize <= reduced)
	    {
		bufferSize = reduced;
		reduced	-= doubleblocksize;
	    }

	    /* in case of large blocksize and small file, reduce buffer size
	     * and degrade to single buffer mode if file is not bigger than
	     * one buffer
	     */
	    if (fileSize < bufferSize)
	    {
		halfbuffersize = bufferSize >> 1;
		bufferSize = fileSize;
	    }
	}


	/* now allocate the AsyncRFile structure, as well as the read buffers.
	 * Add 15 bytes to the total size in order to allow for later
	 * quad-longword alignment of the buffers
	 */

	if (file = AllocVec(sizeof(AsyncRFile) + bufferSize + 15,MEMF_PUBLIC | MEMF_ANY))
	{
	    file->af_File      = handle;

	    /* initialize the AsyncRFile structure. We do as much as we can here,
	     * in order to avoid doing it in more critical sections
	     *
	     * Note how the two buffers used are quad-longword aligned. This
	     * helps performance on 68040 systems with copyback cache. Aligning
	     * the data avoids a nasty side-effect of the 040 caches on DMA.
	     * Not aligning the data causes the device driver to have to do
	     * some magic to avoid the cache problem. This magic will generally
	     * involve flushing the CPU caches. This is very costly on an 040.
	     * Aligning things avoids the need for magic, at the cost of at
	     * most 15 bytes of ram.
	     */

	    fh                       = BADDR(file->af_File);
	    file->af_Handler         = fh->fh_Type;
	    file->af_Buffers[0]      = (APTR)(((ULONG)file + sizeof(AsyncRFile) + 15) & 0xfffffff0);
	    if (halfbuffersize)
	    {
		if (halfbuffersize < bufferSize)
		{
		    file->af_BufferSize  = halfbuffersize;
		    file->af_Buffers[1]  = (APTR)((ULONG)file->af_Buffers[0] + halfbuffersize);
		}
		else
		{
		    file->af_BufferSize  = bufferSize;
		    file->af_Buffers[1]  = 0;
		}
	    }
	    else
	    {
		file->af_BufferSize  = bufferSize >> 1;
		file->af_Buffers[1]  = (APTR)((ULONG)file->af_Buffers[0] + file->af_BufferSize);
	    }
	    file->af_Offset          = file->af_Buffers[0];
	    file->af_BytesLeft       = 0;
	    file->af_BufMin[0]       = 0;
	    file->af_BufMin[1]       = 0;
	    file->af_BytesArrived[0] = 0;
	    file->af_BytesArrived[1] = 0;
	    file->af_CurrentBuf      = 0;
	    file->af_SeekOffset      = 0;
	    file->af_FilesysPos      = 0;
	    file->af_BufferPos       = 0;
	    file->af_FileSize        = fileSize;
	    file->af_SequentialBytes = 0;

	    /* this is the port used to get the packets we send out back.
	     * It is initialized to PA_IGNORE, which means that no signal is
	     * generated when a message comes in to the port. The signal bit
	     * number is initialized to SIGB_SINGLE, which is the special bit
	     * that can be used for one-shot signaling. The signal will never
	     * be set, since the port is of type PA_IGNORE. We'll change the
	     * type of the port later on to PA_SIGNAL whenever we need to wait
	     * for a message to come in.
	     *
	     * The trick used here avoids the need to allocate an extra signal
	     * bit for the port. It is quite efficient.
	     */

	    file->af_PacketPort.mp_MsgList.lh_Head     = (struct Node *)&file->af_PacketPort.mp_MsgList.lh_Tail;
	    file->af_PacketPort.mp_MsgList.lh_Tail     = NULL;
	    file->af_PacketPort.mp_MsgList.lh_TailPred = (struct Node *)&file->af_PacketPort.mp_MsgList.lh_Head;
	    file->af_PacketPort.mp_Node.ln_Name        = NULL;
	    file->af_PacketPort.mp_Node.ln_Type        = NT_MSGPORT;
	    file->af_PacketPort.mp_Flags               = PA_IGNORE;
	    file->af_PacketPort.mp_SigBit              = SIGB_SINGLE;
	    file->af_PacketPort.mp_SigTask             = FindTask(NULL);

	    file->af_Packet.sp_Pkt.dp_Link          = &file->af_Packet.sp_Msg;
	    file->af_Packet.sp_Pkt.dp_Type          = ACTION_READ;
	    file->af_Packet.sp_Pkt.dp_Arg1          = fh->fh_Arg1;
	    file->af_Packet.sp_Pkt.dp_Arg3          = file->af_BufferSize;
	    file->af_Packet.sp_Pkt.dp_Res1          = 0;
	    file->af_Packet.sp_Pkt.dp_Res2          = 0;
	    file->af_Packet.sp_Msg.mn_Node.ln_Name  = (STRPTR)&file->af_Packet.sp_Pkt;
	    file->af_Packet.sp_Msg.mn_Node.ln_Type  = NT_MESSAGE;
	    file->af_Packet.sp_Msg.mn_Length        = sizeof(struct StandardPacket);

	    /* send out the first read packet to the file system. While
	     * the application is getting ready to read data, the file
	     * system will happily fill in this buffer with DMA
	     * transfers, so that by the time the application needs the
	     * data, it will be in the buffer waiting
	     */

	    if (file->af_Handler)
	    {
		SendAsyncRPacket(file,file->af_Buffers[0], 0);
		file->af_PacketPending   = ASR_PKT_START;
	    }
	    else
		file->af_PacketPending   = ASR_PKT_READY; /* this makes NIL: return EOF on every read */
	}
	else
	{
	    err = ERROR_NO_FREE_STORE;
	    Close(handle);
	}
    }
end:
    SetIoErr(err);
    return(file);
}

