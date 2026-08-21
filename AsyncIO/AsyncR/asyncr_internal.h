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


#include <proto/exec.h>
#include <proto/dos.h>

#include <asyncr.h>

/*****************************************************************************/

/* this macro lets us long-align structures on the stack */
#define D_S(type,name) char a_##name[sizeof(type)+3]; \
		       type *name = (type *)((ULONG)(a_##name+3) & ~3UL)


/*****************************************************************************/

/* this is tuned for the SDMake use-case */
#define ASR_SEQBYTESTHRESH 3

/* SDMake does not actually need this, as it never seeks forward.
 * It is a safety valve to ensure forward seeks do not break future code.
 */
#define ASR_BYTESLEFTTHRESH 4

LONG SendAsyncRPacket(AsyncRFile *file, APTR buffer, ULONG filesyspos);
LONG WaitAsyncRPacket(AsyncRFile *file);

