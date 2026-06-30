#ifndef __ASYNCIO_H
#define __ASYNCIO_H


/*****************************************************************************/


#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_PORTS_H
#include <exec/ports.h>
#endif

#ifndef DOS_DOS_H
#include <dos/dos.h>
#endif


/*****************************************************************************/


/* This structure is public only by necessity, don't muck with it yourself, or
 * you're looking for trouble
 */
typedef struct AsyncFile
{
    BPTR                  af_File;
    ULONG                 af_BlockSize;
    struct MsgPort       *af_Handler;
    APTR                  af_Offset;
    LONG                  af_BytesLeft;
    ULONG                 af_BufferSize;
    APTR                  af_Buffers[2];
    struct StandardPacket af_Packet;
    struct MsgPort        af_PacketPort;
    ULONG                 af_CurrentBuf;
    ULONG                 af_SeekOffset;
    UBYTE                 af_PacketPending;
    UBYTE                 af_ReadMode;
} AsyncFile;


/*****************************************************************************/


typedef enum OpenModes
{
    MODE_READ,      /* read an existing file                             */
    MODE_WRITE,     /* create a new file, delete existing file if needed */
    MODE_APPEND     /* append to end of existing file, or create new     */
} OpenModes;

typedef enum SeekModes
{
    MODE_START = -1,   /* relative to start of file         */
    MODE_CURRENT,      /* relative to current file position */
    MODE_END           /* relative to end of file           */
} SeekModes;


/*****************************************************************************/


AsyncFile *OpenAsync(const STRPTR fileName, OpenModes mode, LONG bufferSize);
LONG CloseAsync(AsyncFile *file);
LONG ReadAsync(AsyncFile *file, APTR buffer, LONG numBytes);
LONG ReadCharAsync(AsyncFile *file);
LONG WriteAsync(AsyncFile *file, APTR buffer, LONG numBytes);
LONG WriteCharAsync(AsyncFile *file, UBYTE ch);
LONG SeekAsync(AsyncFile *file, LONG position, SeekModes mode);


/*****************************************************************************/


#endif /* __ASYNCIO_H */
