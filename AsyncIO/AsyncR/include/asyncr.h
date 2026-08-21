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

typedef short enum AsyncRPacketState
{
    ASR_PKT_START = -1,   /* initial buffer fill pending            */
    ASR_PKT_IDLE,         /* no packet pending                      */
    ASR_PKT_PENDING,      /* packet sent, pending reply             */
    ASR_PKT_READY         /* other buffer ready for sequential read */
} AsyncRPacketState;


/*****************************************************************************/


/* This structure is public only by necessity, don't muck with it yourself, or
 * you're looking for trouble
 */
typedef struct AsyncRFile
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
    AsyncRPacketState     af_PacketPending;
    ULONG                 af_FilesysPos;
    ULONG                 af_BufferPos;
    ULONG                 af_FileSize;
    ULONG                 af_SequentialBytes;
} AsyncRFile;


/*****************************************************************************/


typedef enum AsyncRSeekModes
{
    ASR_MODE_START = -1,   /* relative to start of file         */
    ASR_MODE_CURRENT,      /* relative to current file position */
    ASR_MODE_END           /* relative to end of file           */
} AsyncRSeekModes;


/*****************************************************************************/


AsyncRFile *OpenAsyncR(const STRPTR fileName, ULONG bufferSize);
void CloseAsyncR(AsyncRFile *file);
LONG ReadAsyncR(AsyncRFile *file, APTR buffer, LONG numBytes);
LONG ReadCharAsyncR(AsyncRFile *file);
STRPTR FGetsAsyncR(AsyncRFile *file, STRPTR buffer, ULONG numBytes);
STRPTR FGetsLenAsyncR(AsyncRFile *file, STRPTR buffer, ULONG numBytes, ULONG *len);
LONG ReadLineAsyncR(AsyncRFile *file, STRPTR buffer, LONG numBytes);
LONG SeekAsyncR(AsyncRFile *file, LONG position, AsyncRSeekModes mode);


/*****************************************************************************/


#endif /* __ASYNCR_H */
