#include "async_internal.h"

/*****************************************************************************/


void CloseAsync(AsyncFile *file)
{
    if (file)
    {
	if (file->af_PacketPending == PKT_PENDING ||
	    file->af_PacketPending == PKT_START)
	{
	    WaitPacket(file);
	}

	Close(file->af_File);
	FreeVec(file);
    }
}

