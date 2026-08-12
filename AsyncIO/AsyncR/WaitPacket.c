#include "async_internal.h"

/*****************************************************************************/


/* this function waits for a packet to come back from the file system. 
 *
 * WARNING: This function requires file->af_PacketPending to be
 *          either PKT_START or PKT_PENDING, or it will cause a
 *          dead lock.
 *
 * This function also deals with IO errors, bringing up the needed DOS
 * requesters to let the user retry an operation or cancel it.
 */
LONG WaitPacket(AsyncFile *file)
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
		if (file->af_PacketPending == PKT_START)
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
	if (ErrorReport(file->af_Packet.sp_Pkt.dp_Res2,REPORT_STREAM,file->af_File,NULL))
	{
	    if (file->af_PacketPending == PKT_PENDING)
	    {
		file->af_BufMin[1 - file->af_CurrentBuf] = 0;
		file->af_BytesArrived[1 - file->af_CurrentBuf] = 0;
	    }
	    break;
	}

	/* user wants to try again, resend the packet */
	if (file->af_PacketPending == PKT_START)
	{
	    SendPacket(file,file->af_Buffers[file->af_CurrentBuf], file->af_FilesysPos);
	    file->af_PacketPending = PKT_START;
	}
	else
	    SendPacket(file,file->af_Buffers[1 - file->af_CurrentBuf], file->af_FilesysPos);
    }

    file->af_PacketPending = PKT_IDLE;

    return(bytes);
}

