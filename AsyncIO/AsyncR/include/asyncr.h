#ifndef __ASYNCR_H
#define __ASYNCR_H


/*****************************************************************************/


#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_PORTS_H
#include <exec/ports.h>
#endif

#ifndef DOS_DOSEXTENS_H
#include <dos/dosextens.h>
#endif


/*****************************************************************************/

typedef short enum PacketState
{
    PKT_START = -1,   /* initial buffer fill pending            */
    PKT_IDLE,         /* no packet pending                      */
    PKT_PENDING,      /* packet sent, pending reply             */
    PKT_READY         /* other buffer ready for sequential read */
} PacketState;


/*****************************************************************************/


/* This structure is public only by necessity, don't muck with it yourself, or
 * you're looking for trouble
 */
typedef struct AsyncFile
{
    BPTR                  af_File;
    struct MsgPort       *af_Handler;
    APTR                  af_Offset;
    LONG                  af_BytesLeft;
    ULONG                 af_BufferSize;
    APTR                  af_Buffers[2];
    ULONG                 af_BufMin[2];
    ULONG                 af_BytesArrived[2];
    struct StandardPacket af_Packet;
    struct MsgPort        af_PacketPort;
    ULONG                 af_SeekOffset;
    UWORD                 af_CurrentBuf;
    PacketState		  af_PacketPending;
    ULONG                 af_FilesysPos;
    ULONG                 af_BufferPos;
    ULONG                 af_FileSize;
    ULONG                 af_SequentialBytes;
} AsyncFile;


/*****************************************************************************/


typedef enum SeekModes
{
    MODE_START = -1,   /* relative to start of file         */
    MODE_CURRENT,      /* relative to current file position */
    MODE_END           /* relative to end of file           */
} SeekModes;


/*****************************************************************************/


AsyncFile *OpenAsync(const STRPTR fileName, ULONG bufferSize);
void CloseAsync(AsyncFile *file);
LONG ReadAsync(AsyncFile *file, APTR buffer, LONG numBytes);
LONG ReadCharAsync(AsyncFile *file);
STRPTR FGetsAsync(AsyncFile *file, STRPTR buffer, ULONG numBytes);
STRPTR FGetsLenAsync(AsyncFile *file, STRPTR buffer, ULONG numBytes, ULONG *len);
LONG SeekAsync(AsyncFile *file, LONG position, SeekModes mode);


/*****************************************************************************/


#endif /* __ASYNCR_H */
