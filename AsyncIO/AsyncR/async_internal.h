
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <pragmas/exec_pragmas.h>
#include <pragmas/dos_pragmas.h>

#include "asyncio.h"


/*****************************************************************************/


extern struct Library *DOSBase;
extern struct Library *SysBase;


/*****************************************************************************/


/* this macro lets us long-align structures on the stack */
#define D_S(type,name) char a_##name[sizeof(type)+3]; \
		       type *name = (type *)((LONG)(a_##name+3) & ~3);

