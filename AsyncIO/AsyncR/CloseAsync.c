
/*****************************************************************************/


LONG CloseAsync(AsyncFile *file)
{
LONG result;

    if (file)
    {
	result = WaitPacket(file);
	if (result >= 0)
	{
	    if (!file->af_ReadMode)
	    {
		/* this will flush out any pending data in the write buffer */
		if (file->af_BufferSize > file->af_BytesLeft)
		    result = Write(file->af_File,file->af_Buffers[file->af_CurrentBuf],file->af_BufferSize - file->af_BytesLeft);
	    }
	}

	Close(file->af_File);
	FreeVec(file);
    }
    else
    {
	SetIoErr(ERROR_INVALID_LOCK);
	result = -1;
    }

    return(result);
}

