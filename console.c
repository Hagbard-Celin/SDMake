
/*
 *  CONSOLE.C
 *
 *    (c)Copyright 1992-1997 Obvious Implementations Corp.  Redistribution and
 *    use is allowed under the terms of the DICE-LICENSE FILE,
 *    DICE-LICENSE.TXT.
 *
 *  OpenConsole()   - set this process's console as far as we can do such
 *		      things.
 */

#define DOSBase_DECLARED
#define Prototype extern

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <exec/alerts.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define DOS_TRUE    (-1)
#define DOS_FALSE   (0)

#ifdef DEBUG
#define dbprintf(x) fhprintf x
#else
#define dbprintf(x)
#endif

Prototype BOOL OpenConsole(const char *str);

extern struct Process *StartProc;

typedef struct FileHandle   FileHandle;
typedef struct Process	    Process;
typedef struct List	    List;
typedef struct MsgPort	    MsgPort;
typedef struct Message	    Message;
typedef struct CommandLineInterface CLI;
typedef struct Task	    Task;

static BPTR  CustomCIS;
static BPTR  CustomCOS;
static BPTR  SaveCIS;
static BPTR  SaveCOS;
static MsgPort *SaveConsoleTask;

extern struct DosLibrary *DOSBase;

void _STD_opencon_exit(void)
{
    Process *proc = (Process *)FindTask(NULL);

    if (CustomCIS || CustomCOS)
	proc->pr_ConsoleTask = SaveConsoleTask;
    if (CustomCOS) {
	Write(CustomCOS, "\nHit RETURN to Exit\n", 20);
	proc->pr_COS = SaveCOS;
	Close(CustomCOS);
	CustomCOS = 0;
    }
    if (CustomCIS) {
	char ch;
	while (Read(CustomCIS, &ch, 1) && ch != '\n');
	proc->pr_CIS = SaveCIS;
	Close(CustomCIS);
	CustomCIS = 0;
    }
}

BOOL OpenConsole(const char *str)
{
    FileHandle *fh;
    BOOL r = FALSE;

    _STD_opencon_exit();
    if (CustomCIS = Open(str, 1005)) {
	fh = BADDR(CustomCIS);
	if (fh->fh_Type) {
	    r = TRUE;

	    SaveConsoleTask = StartProc->pr_ConsoleTask;
	    SaveCOS = StartProc->pr_COS;
	    SaveCIS = StartProc->pr_CIS;
	    StartProc->pr_ConsoleTask = fh->fh_Type;
	    StartProc->pr_COS = CustomCOS = Open("*", 1005);
	    StartProc->pr_CIS = CustomCIS;
	    freopen("*", "r", stdin);
	    freopen("*", "w", stdout);
	    freopen("*", "w", stderr);
	    /*StartProc->pr_ConsoleTask = SaveConsoleTask;*/
	} else {
	    Close(CustomCIS);
	    CustomCIS = 0;
	}
    }
    return(r);
}

