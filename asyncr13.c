/*
 *
 *  This file contains minor modifications of the following files to make
 *  them work in read mode on 1.3:
 *  - AsyncIO/src/OpenAsyncFH.c
 *  - AsyncIO/src/CloseAsync.c
 *  - AsyncIO/src/SeekAsync.c
 *
 *  I consider these changes too trivial to be copyrightable.
 *  However, to avoid any doubt, any modifications made by me are
 *  released into the public domain.
 *
 *  - Hagbard Celine
 *
 */

#define INTERNAL_ASYNC
#include "AsyncIO/src/async.h"
#include "asyncior13.h"

extern void *DosAllocMem(long bytes);
extern void DosFree(void *vptr);
extern struct Process *WorkProc;

typedef struct CommandLineInterface CLI;

AsyncFile *OpenAsyncR13( CONST_STRPTR fileName, LONG filesize,  LONG bufferSize );
LONG CloseAsyncR13( AsyncFile *file );
LONG SeekAsyncR13( AsyncFile *filearg, LONG position, SeekModes mode );


AsyncFile *OpenAsyncR13( CONST_STRPTR fileName, LONG filesize,  LONG bufferSize )
{
	struct FileHandle	*fh;
	AsyncFileR13		*file = NULL;
	BPTR	lock = NULL;
	BPTR	handle = NULL;
	LONG	blockSize, blockSize2;
	D_S( struct InfoData, infoData );

	/* if it was possible to obtain a lock on the same device as the
	 * file we're working on, get the block size of that device and
	 * round up our buffer size to be a multiple of the block size.
	 * This maximizes DMA efficiency.
	 */

	blockSize = 512;
	blockSize2 = 1024;

	if( lock = Lock(fileName, ACCESS_READ) )
	{
		if( Info( lock, infoData ) )
		{
			blockSize = infoData->id_BytesPerBlock;
			blockSize2 = blockSize * 2;
			bufferSize = ( ( bufferSize + blockSize2 - 1 ) / blockSize2 ) * blockSize2;
		}

		UnLock(lock);
	}
	else
	    return 0;

	if (!( handle = Open( fileName, MODE_OLDFILE ) ))
	    return 0;

	/* now allocate the ASyncFile structure, as well as the read buffers.
	 * Add 15 bytes to the total size in order to allow for later
	 * quad-longword alignement of the buffers
	 */

	for( ;; )
	{
		if( file = DosAllocMem( sizeof( AsyncFileR13 ) + bufferSize + 15 ) )
		{
			break;
		}
		else
		{
			if( bufferSize > blockSize2 )
			{
				bufferSize -= blockSize2;
			}
			else
			{
				break;
			}
		}
	}

	if( file )
	{
		file->af_File		= handle;
		file->af_ReadMode	= TRUE;
		file->af_BlockSize	= blockSize;
		file->af_CloseFH	= TRUE;

		/* initialize the ASyncFile structure. We do as much as we can here,
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

		fh			= BADDR( file->af_File );
		file->af_Handler	= fh->fh_Type;
		file->af_BufferSize	= ( ULONG ) bufferSize / 2;
		file->af_Buffers[ 0 ]	= ( APTR ) ( ( ( ULONG ) file + sizeof( AsyncFileR13 ) + 15 ) & 0xfffffff0 );
		file->af_Buffers[ 1 ]	= file->af_Buffers[ 0 ] + file->af_BufferSize;
		file->af_CurrentBuf	= 0;
		file->af_SeekOffset	= 0;
		file->af_PacketPending	= FALSE;
		file->af_SeekPastEOF	= FALSE;
		file->af_FileSize	= filesize;

		/* this is the port used to get the packets we send out back.
		 * It is initialized to PA_IGNORE, which means that no signal is
		 * generated when a message comes in to the port. The signal bit
		 * number is initialized to SIGB_SINGLE, which is the special bit
		 * that can be used for one-shot signalling. The signal will never
		 * be set, since the port is of type PA_IGNORE. We'll change the
		 * type of the port later on to PA_SIGNAL whenever we need to wait
		 * for a message to come in.
		 *
		 * The trick used here avoids the need to allocate an extra signal
		 * bit for the port. It is quite efficient.
		 */

		file->af_PacketPort.mp_MsgList.lh_Head		= ( struct Node * ) &file->af_PacketPort.mp_MsgList.lh_Tail;
		file->af_PacketPort.mp_MsgList.lh_Tail		= NULL;
		file->af_PacketPort.mp_MsgList.lh_TailPred	= ( struct Node * ) &file->af_PacketPort.mp_MsgList.lh_Head;
		file->af_PacketPort.mp_Node.ln_Type		= NT_MSGPORT;
		/* MH: Avoid problems with SnoopDos */
		file->af_PacketPort.mp_Node.ln_Name		= NULL;
		file->af_PacketPort.mp_Flags			= PA_IGNORE;
		file->af_PacketPort.mp_SigBit			= SIGB_SINGLE;
		file->af_PacketPort.mp_SigTask			= WorkProc;

		file->af_Packet.sp_Pkt.dp_Link			= &file->af_Packet.sp_Msg;
		file->af_Packet.sp_Pkt.dp_Arg1			= fh->fh_Arg1;
		file->af_Packet.sp_Pkt.dp_Arg3			= file->af_BufferSize;
		file->af_Packet.sp_Pkt.dp_Res1			= 0;
		file->af_Packet.sp_Pkt.dp_Res2			= 0;
		file->af_Packet.sp_Msg.mn_Node.ln_Name		= ( STRPTR ) &file->af_Packet.sp_Pkt;
		file->af_Packet.sp_Msg.mn_Node.ln_Type		= NT_MESSAGE;
		file->af_Packet.sp_Msg.mn_Length		= sizeof( struct StandardPacket );

		/* if we are in read mode, send out the first read packet to
		 * the file system. While the application is getting ready to
		 * read data, the file system will happily fill in this buffer
		 * with DMA transfers, so that by the time the application
		 * needs the data, it will be in the buffer waiting
		 */

		file->af_Packet.sp_Pkt.dp_Type	= ACTION_READ;
		file->af_BytesLeft		= 0;

		/* MH: We set the offset to the buffer not being filled, in
		 * order to avoid special case code in SeekAsync. ReadAsync
		 * isn't affected by this, since af_BytesLeft == 0.
		 */
		file->af_Offset = file->af_Buffers[ 1 ];

		if( file->af_Handler )
		{
			AS_SendPacket( (AsyncFile *)file, file->af_Buffers[ 0 ] );
		}
	}
	else
	{
	    Close( handle );
	}

	return( (AsyncFile *)file );
}

LONG CloseAsyncR13( AsyncFile *file )
{
	LONG	result;

	if( file )
	{
		result = AS_WaitPacket( file );

		if( result >= 0 )
		{
			if( !file->af_ReadMode )
			{
				/* this will flush out any pending data in the write buffer */
				if( file->af_BufferSize > file->af_BytesLeft )
				{
					result = Write(
						file->af_File,
						file->af_Buffers[ file->af_CurrentBuf ],
						file->af_BufferSize - file->af_BytesLeft );
				}
			}
		}

		if( file->af_CloseFH )
		{
			Close( file->af_File );
		}

		DosFree(file);
	}
	else
	{
		result = -1;
	}

	return( result );
}

LONG SeekAsyncR13( AsyncFile *filearg, LONG position, SeekModes mode )
{
	LONG	current, target, roundTarget, filePos;
	LONG	minBuf, maxBuf, bytesArrived, diff;
	//LONG	  fileSize;
	AsyncFileR13 *file = (AsyncFileR13 *)filearg;

	bytesArrived = AS_WaitPacket( (AsyncFile *)file );

	/* MH: No packets can be pending here! */

	if( bytesArrived < 0 )
	{
		/* MH: Experimental: Try to allow "resume" of seeks past EOF. */

		if( file->af_SeekPastEOF )
		{
			/* MH: Restore saved values, to make resume possible */
			bytesArrived = file->af_LastRes1;
			file->af_BytesLeft = file->af_LastBytesLeft;
		}
		else
		{
			return( -1 );
		}
	}

	if( file->af_ReadMode )
	{
		/* figure out what the actual file position is */
		filePos = Seek( file->af_File, 0, OFFSET_CURRENT );

		if( filePos < 0 )
		{
			AS_RecordSyncFailure( (AsyncFile *)file );
			return( -1 );
		}

		/* figure out what the caller's file position is */
		current = filePos - ( file->af_BytesLeft + bytesArrived ) + file->af_SeekOffset;

		/* MH: We can't clear af_SeekOffset here. If another seek is done
		 * directly after this one, it would mean that we will both return
		 * the wrong position, and start reading from the wrong position.
		 */
		/* file->af_SeekOffset = 0; */

		/* figure out the absolute offset within the file where we must seek to */
		if( mode == MODE_CURRENT )
		{
			target = current + position;
		}
		else if( mode == MODE_START )
		{
			target = position;
		}
		else /* if( mode == MODE_END ) */
		{
			target = file->af_FileSize + position;
		}

		/* MH: Here we must be able to handle two different situations:
		 * 1) A seek directly after having dropped both buffers, and started
		 *    refilling (typical case: File open).
		 * 2) Other seeks (typical case: A seek after some initial reading).
		 *
		 * We need to subtract with "af_Buffers[ 1 - file->af_CurrentBuf ]",
		 * as af_CurrentBuf refers to the *arrived* buffer, not the one we're
		 * currently reading from (and af_Offset points into the buffer we're
		 * reading from)!
		 *
		 * In case 1, there will be only one packet received. af_CurrentBuf
		 * will be zero, and refers to the newly arrived buffer (as it
		 * should). For proper behaviour in the minBuf calculation, we have
		 * set af_Offset to point to af_Buffers[ 1 ], when starting reading
		 * to empty buffers. That way wee need no special case code here.
		 * ReadAsync() can handle this, as af_BytesLeft == 0 in that case.
		 */

		/* figure out what range of the file is currently in our buffers */
		minBuf = current - ( LONG ) ( file->af_Offset - file->af_Buffers[ 1 - file->af_CurrentBuf ] );
		maxBuf = current + file->af_BytesLeft + bytesArrived;  /* WARNING: this is one too big */

		diff = target - current;

#ifdef DO_SOME_DEBUG
		Printf( "Target: %ld, minBuf: %ld, maxBuf: %ld, current: %ld, diff: %ld, bytesLeft: %ld\n",
			target, minBuf, maxBuf, current, diff, file->af_BytesLeft );
#endif

		if( ( target < minBuf ) || ( target >= maxBuf ) )
		{
			/* the target seek location isn't currently in our buffers, so
			 * move the actual file pointer to the desired location, and then
			 * restart the async read thing...
			 */

			if( target >= maxBuf )
			{
				/* MH: There's a fair chance that we really tried to seek
				 * past EOF. In order to tell for sure, we need to compare
				 * the seek target with the file size. The roundTarget may
				 * be before real EOF, so the "real" Seek() call might
				 * not notice any problems.
				 */
				if( target > file->af_FileSize )
				{
					/* MH: Experimental: Try to allow "resume" of
					 * seeks past EOF.
					 */
					file->af_SeekPastEOF = TRUE;

					WorkProc->pr_Result2 = ERROR_SEEK_ERROR;
					AS_RecordSyncFailure( (AsyncFile *)file );
					return( -1 );
				}
			}

			/* this is to keep our file reading block-aligned on the device.
			 * block-aligned reads are generally quite a bit faster, so it is
			 * worth the trouble to keep things aligned
			 */
			roundTarget = ( target / file->af_BlockSize ) * file->af_BlockSize;

			if( Seek( file->af_File, roundTarget - filePos, OFFSET_CURRENT ) < 0 )
			{
				AS_RecordSyncFailure( (AsyncFile *)file );
				return( -1 );
			}

			AS_SendPacket( (AsyncFile *)file, file->af_Buffers[ 0 ] );

			file->af_SeekOffset	= target - roundTarget;
			file->af_BytesLeft	= 0;
			file->af_CurrentBuf	= 0;

			/* MH: We set af_Offset to the buffer not being filled, to be able to
			 * handle a new seek directly after this one (see above; minBuf
			 * calculation). If we start reading after this seek, ReadAsync()
			 * will handle everything correctly, as af_BytesLeft == 0.
			 */
			file->af_Offset		= file->af_Buffers[ 1 ];
		}
		else if( ( target < current ) || ( diff <= file->af_BytesLeft ) )
		{
			/* one of the two following things is true:
			 *
			 * 1. The target seek location is within the current read buffer,
			 * but before the current location within the buffer. Move back
			 * within the buffer and pretend we never got the pending packet,
			 * just to make life easier, and faster, in the read routine.
			 *
			 * 2. The target seek location is ahead within the current
			 * read buffer. Advance to that location. As above, pretend to
			 * have never received the pending packet.
			 */

			AS_RequeuePacket( (AsyncFile *)file );

			file->af_BytesLeft	-= diff;
			file->af_Offset		+= diff;

			/* MH: We don't need to clear the seek offset here, since
			 * if we get here, we must have read some data from the current
			 * buffer, and af_SeekOffset will be zero then (done by
			 * ReadAsync()).
			 */

			/* MH: If we're recovering from seek past EOF, restore some
			 * values here.
			 */
			if( file->af_SeekPastEOF )
			{
				file->af_Packet.sp_Pkt.dp_Res1 = file->af_LastRes1;
			}
		}
		else
		{
			/* at this point, we know the target seek location is within
			 * the buffer filled in by the packet that we just received
			 * at the start of this function. Throw away all the bytes in the
			 * current buffer, send a packet out to get the async thing going
			 * again, readjust buffer pointers to the seek location, and return
			 * with a grin on your face... :-)
			 */

			/* MH: Don't refill the buffer we just got, but the other one! */
			AS_SendPacket( (AsyncFile *)file, file->af_Buffers[ 1 - file->af_CurrentBuf ] );

			/* MH: Account for bytes left in buffer we drop *and* the af_SeekOffset.
			 */
			diff -= file->af_BytesLeft - file->af_SeekOffset;

			/* MH: Set the offset into the current (newly arrived) buffer */
			file->af_Offset = file->af_Buffers[ file->af_CurrentBuf ] + diff;
			file->af_BytesLeft = bytesArrived - diff;

			/* MH: We need to clear the seek offset here, since we can't do it above.
			 */
			file->af_SeekOffset = 0;

			/* MH: This "buffer switching" is important to do. It wasn't done!
			 * This explains the errors one could encounter now and then.
			 * The AS_SendPacket() call above is not the cause, and *is* correct.
			 */
			file->af_CurrentBuf = 1 - file->af_CurrentBuf;
		}
	}
	else
	{
		/* flush the buffers */
		if( file->af_BufferSize > file->af_BytesLeft )
		{
			if( Write(
				file->af_File,
				file->af_Buffers[ file->af_CurrentBuf ],
				file->af_BufferSize - file->af_BytesLeft ) < 0 )
			{
				AS_RecordSyncFailure( (AsyncFile *)file );
				return( -1 );
			}
		}

		/* this will unfortunately generally result in non block-aligned file
		 * access. We could be sneaky and try to resync our file pos at a
		 * later time, but we won't bother. Seeking in write-only files is
		 * relatively rare (except when writing IFF files with unknown chunk
		 * sizes, where the chunk size has to be written after the chunk data)
		 */

		/* MH: Ideas on how to improve the above (not tested, since I don't need
		 * the SeekAsync for writing in any of my programs at the moment! ;):
		 *
		 * Add a new field to the AsyncFile struct. af_WriteOffset or something like
		 * that (af_SeekOffset can probably be used). Like in the read case, we
		 * calculate a roundTarget, but we don't seek to that (but rather to the
		 * "absolute" position), and save the difference in the struct. af_BytesLeft
		 * and af_Offset are adjusted to point into the "middle" of the buffer,
		 * where the write will occur. Writes then needs some minor changes:
		 * Instead of simply writing the buffer from the start, we add the offset
		 * (saved above) to the buffer base, and write the partial buffer. The
		 * offset is then cleared. Voila: The file is still block-aligned, at the
		 * price of some non-optimal buffer usage.
		 *
		 * Problem: As it is now, Arg3 in the packet is always set to the buffer size.
		 * With the above fix, this would have to be updated for each SendPacket (i.e.
		 * a new argument would be needed).
		 */

		current = Seek( file->af_File, position, mode );

		if( current < 0 )
		{
			AS_RecordSyncFailure( (AsyncFile *)file );
			return( -1 );
		}

		file->af_BytesLeft	= file->af_BufferSize;
		file->af_CurrentBuf	= 0;
		file->af_Offset		= file->af_Buffers[ 0 ];
	}

	if( file->af_SeekPastEOF )
	{
		/* MH: Clear up any error flags, and restore last Res1. */
		file->af_SeekPastEOF = FALSE;
	}

	WorkProc->pr_Result2 = 0;
	return( current );
}

