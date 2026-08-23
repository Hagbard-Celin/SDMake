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
#if OSVERMIN < 36
#include <stdio.h>
#include <proto/intuition.h>
#endif

static LONG errorreport(LONG code, LONG type, ULONG arg1, struct MsgPort *device);

/*****************************************************************************/


/* this function waits for a packet to come back from the file system. 
 *
 * WARNING: This function requires file->af_PacketPending to be
 *          either ASR_PKT_START or ASR_PKT_PENDING, or it will cause a
 *          dead lock.
 *
 * This function also deals with IO errors, bringing up the needed DOS
 * requesters to let the user retry an operation or cancel it.
 */
LONG WaitAsyncRPacket(AsyncRFile *file)
{
    LONG bytes = 0;

    while (TRUE)
    {
	/* This enables signaling when a packet comes back to the port */
	file->af_PacketPort.mp_Flags = PA_SIGNAL;

	/* Wait for the packet to come back, and remove it from the message
	 * list. Since we know no other packets can come in to the port, we can
	 * safely use Remove() instead of GetMsg(). If other packets could come in,
	 * we would have to use GetMsg(), which correctly arbitrates access in such
	 * a case
	 */
	Remove((struct Node *)WaitPort(&file->af_PacketPort));

	/* set the port type back to PA_IGNORE so we won't be bothered with
	 * spurious signals
	 */
	file->af_PacketPort.mp_Flags = PA_IGNORE;


	bytes = file->af_Packet.sp_Pkt.dp_Res1;
	if (bytes >= 0)
	{
	    /* if bytes == 0, we want to keep the previous buffer contents valid
	     * to avoid possible unnecessary reads later
	     */
	    if (bytes > 0)
	    {
		if (file->af_PacketPending == ASR_PKT_START)
		{
		    file->af_BufMin[file->af_CurrentBuf] = file->af_FilesysPos;
		    file->af_BytesArrived[file->af_CurrentBuf] = bytes;
		}
		else
		{
		    file->af_BufMin[1 - file->af_CurrentBuf] = file->af_FilesysPos;
		    file->af_BytesArrived[1 - file->af_CurrentBuf] = bytes;
		}

		file->af_FilesysPos += bytes;
	    }

	    SetIoErr(0);
	    break;
	}

	/* packet's error code */
	SetIoErr(file->af_Packet.sp_Pkt.dp_Res2);

	/* see if the user wants to try again... */
	if (errorreport(file->af_Packet.sp_Pkt.dp_Res2,REPORT_STREAM,file->af_File,NULL))
	{
	    if (file->af_PacketPending == ASR_PKT_PENDING)
	    {
		ULONG offset;

		file->af_BufMin[1 - file->af_CurrentBuf] = 0;
		file->af_BytesArrived[1 - file->af_CurrentBuf] = 0;

		offset = (ULONG)file->af_Offset - (ULONG)file->af_Buffers[file->af_CurrentBuf];

		/* reset state of current buffer in case the read was initated from SeekAsync() */
		file->af_BytesLeft = file->af_BytesArrived[file->af_CurrentBuf] - offset;
		file->af_BufferPos = file->af_BufMin[file->af_CurrentBuf] + offset;
		file->af_SeekOffset = 0;
	    }
	    break;
	}

	/* user wants to try again, resend the packet */
	if (file->af_PacketPending == ASR_PKT_START)
	{
	    SendAsyncRPacket(file,file->af_Buffers[file->af_CurrentBuf], file->af_FilesysPos);
	    file->af_PacketPending = ASR_PKT_START;
	}
	else
	    SendAsyncRPacket(file,file->af_Buffers[1 - file->af_CurrentBuf], file->af_FilesysPos);
    }

    if (file->af_PacketPending == ASR_PKT_START && file->af_FileSize)
    {
	/* handle small file and single buffer modes */
	if (file->af_FileSize > file->af_BufferSize)
	{
	    if (file->af_FileSize < file->af_BufferSize << 1)
		file->af_Packet.sp_Pkt.dp_Arg3 = file->af_FileSize - file->af_Packet.sp_Pkt.dp_Arg3;

	    file->af_PacketPending = ASR_PKT_IDLE;
	}
	else
	    file->af_PacketPending = ASR_PKT_READY;
    }
    else
	file->af_PacketPending = ASR_PKT_IDLE;

    return(bytes);
}

static LONG errorreport(LONG code, LONG type, ULONG arg1, struct MsgPort *device)
{
#if OSVERMIN < 36 && OSVERMAX >= 36
    if (DOSBase->dl_lib.lib_Version >= 36)
    {
#endif
#if OSVERMAX >= 36
	return ErrorReport(code, type, arg1, device);
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
    }
    else
    {
#endif
#if OSVERMIN < 36
	struct IntuiText autoText[3];
	TEXT buf[24];

	sprintf(buf, "I/O error: %ld", code);

	autoText[0].FrontPen = autoText[1].FrontPen = autoText[2].FrontPen = 0;
	autoText[0].BackPen = autoText[1].BackPen = autoText[2].BackPen = 1;
	autoText[0].DrawMode = autoText[1].DrawMode = autoText[2].DrawMode = JAM2;
	autoText[0].LeftEdge = 13;
	autoText[1].LeftEdge = autoText[2].LeftEdge = 6;
	autoText[0].TopEdge = 15;
	autoText[1].TopEdge = autoText[2].TopEdge = 3;
	autoText[0].ITextFont = autoText[1].ITextFont = autoText[2].ITextFont = 0;
	autoText[0].NextText = autoText[1].NextText = autoText[2].NextText = 0;
	autoText[0].IText = buf;
	autoText[1].IText = "Retry";
	autoText[2].IText = "Cancel";

	return (!(AutoRequest(NULL, &autoText[0], &autoText[1], &autoText[2], 0, 0, 300, 40)));
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
    }
#endif
}
