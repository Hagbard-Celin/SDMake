typedef struct AsyncFileR13
{
	BPTR			af_File;
	ULONG			af_BlockSize;
	struct MsgPort		*af_Handler;
	UBYTE			*af_Offset;
	LONG			af_BytesLeft;
	ULONG			af_BufferSize;
	UBYTE			*af_Buffers[2];
	struct StandardPacket	af_Packet;
	struct MsgPort		af_PacketPort;
	ULONG			af_CurrentBuf;
	ULONG			af_SeekOffset;
#ifdef ASIO_NOEXTERNALS
	struct ExecBase		*af_SysBase;
	struct DosLibrary	*af_DOSBase;
#endif
	UBYTE			af_PacketPending;
	UBYTE			af_ReadMode;
	UBYTE			af_CloseFH;
	UBYTE			af_SeekPastEOF;
	ULONG			af_LastRes1;
	ULONG			af_LastBytesLeft;
	LONG			af_FileSize;
} AsyncFileR13;

