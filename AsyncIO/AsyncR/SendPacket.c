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


/* send out an async packet to the file system. */
LONG SendAsyncRPacket(AsyncRFile *file, APTR buffer, ULONG filesyspos)
{
    if (filesyspos != file->arf_FilesysPos)
    {
	if (filesyspos > INT_MAX)
	    Seek(file->arf_File, filesyspos - file->arf_FileSize, OFFSET_END);
	else
	    Seek(file->arf_File, filesyspos, OFFSET_BEGINNING);

	if (IoErr())
	    return(-1);

	file->arf_FilesysPos = filesyspos;
    }
    file->arf_Packet.sp_Pkt.dp_Port = &file->arf_PacketPort;
    file->arf_Packet.sp_Pkt.dp_Arg2 = (LONG)buffer;
    PutMsg(file->arf_Handler, &file->arf_Packet.sp_Msg);
    file->arf_PacketPending = ASR_PKT_PENDING;

    return(0);
}

