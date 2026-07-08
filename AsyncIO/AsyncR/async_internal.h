
#include <proto/exec.h>
#include <proto/dos.h>

#include <asyncr.h>


/*****************************************************************************/

/* this macro lets us long-align structures on the stack */
#define D_S(type,name) char a_##name[sizeof(type)+3]; \
		       type *name = (type *)((ULONG)(a_##name+3) & ~3UL)


/*****************************************************************************/

/* this is tuned for the SDMake use-case */
#define SEQBYTESTHRESH 3

/* SDMake does not actually need this, as it never seeks forward.
 * It is a safety valve to ensure forward seeks do not break future code.
 */
#define BYTESLEFTTHRESH 4

LONG SendPacket(AsyncFile *file, APTR buffer, LONG filesyspos);
LONG WaitPacket(AsyncFile *file);

