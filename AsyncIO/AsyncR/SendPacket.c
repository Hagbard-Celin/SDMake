#include "async_internal.h"
#include <limits.h>

/*****************************************************************************/


/* send out an async packet to the file system. */
LONG SendPacket(AsyncFile *file, APTR buffer, LONG filesyspos)
{
    if (filesyspos != file->af_FilesysPos)
    {
	if (filesyspos > INT_MAX)
	    Seek(file->af_File, filesyspos - file->af_FileSize, OFFSET_END);
	else
	    Seek(file->af_File, filesyspos, OFFSET_BEGINNING);

	if (IoErr())
	    return(-1);

	file->af_FilesysPos = filesyspos;
    }
    file->af_Packet.sp_Pkt.dp_Port = &file->af_PacketPort;
    file->af_Packet.sp_Pkt.dp_Arg2 = (LONG)buffer;
    PutMsg(file->af_Handler, &file->af_Packet.sp_Msg);
    file->af_PacketPending = PKT_PENDING;

    return(0);
}

