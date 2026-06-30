#include <proto/exec.h>

/*****************************************************************************/


/* send out an async packet to the file system. */
static void SendPacket(AsyncFile *file, APTR arg2)
{
    file->af_Packet.sp_Pkt.dp_Port = &file->af_PacketPort;
    file->af_Packet.sp_Pkt.dp_Arg2 = (LONG)arg2;
    PutMsg(file->af_Handler, &file->af_Packet.sp_Msg);
    file->af_PacketPending = TRUE;
}

