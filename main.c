/*
 * Copyright (c) 2026 Hagbard Celine
 *
 * This file is part of SDMake.
 *
 * SDmake is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Sdmake is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SDmake. If not, see <https://www.gnu.org/licenses/>.
 *
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *    (c)Copyright 1992-1997 Obvious Implementations Corp.  Redistribution and
 *    use is allowed under the terms of the DICE-LICENSE FILE,
 *    DICE-LICENSE.TXT.
 *
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *     Copyright (c) 2003-2011,2023 The DragonFly Project.  All rights reserved.
 *
 *     This code is derived from software contributed to The DragonFly Project
 *     by Matthew Dillon <dillon@backplane.com>
 *
 *     Redistribution and use in source and binary forms, with or without
 *     modification, are permitted provided that the following conditions
 *     are met:
 *
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in
 *        the documentation and/or other materials provided with the
 *        distribution.
 *     3. Neither the name of The DragonFly Project nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific, prior written permission.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *     ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *     FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 *     COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *     INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *     BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *     AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *     OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *     OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *     SUCH DAMAGE.
 *
 */

/*
 *  MAIN.C
 */

#include "defs.h"
#include <stdarg.h>
#include <dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <proto/intuition.h>
#include <workbench/icon.h>
#include <workbench/startup.h>
#include <proto/wb.h>
#include <proto/icon.h>
#include "sdmake_rev.h"

int main(ULONG argc, char *argv[]);
void wbmain(struct WBStartup *wbs);
LONG realmain(void);
void help(int);
static void InitStuff(void);
const char *SkipAss(const char *);

struct climsg
{
    struct Message msg;
    LONG data;
};

Prototype void PrintF(CONST_STRPTR ctl, ...);
Prototype void MemErr(void);
Prototype List	DoList;
Prototype short DDebug;
Prototype short CacheLevel;
Prototype short NoRunOpt;
Prototype short QuietOpt;
Prototype short QuietCmd;
Prototype short CheckTarget;
Prototype short ExitCode;
Prototype short	DoAll;
Prototype short	TouchAll;
Prototype short DefIgnore;
Prototype short SomeWork;
Prototype short	SMakeMode;
Prototype APTR  MemPool;
Prototype WORD	  Break;
Prototype struct Process *mycli;
Prototype BPTR StdOut;

List	DoList;
STRPTR	OnError;
APTR    MemPool;
WORD	Break;
short	DDebug;
short	CacheLevel;
short	NoRunOpt;
short	TouchAll;
short	CheckTarget;
short	QuietOpt;
short	QuietCmd;
short	DoAll;
short   DefIgnore;
short   SomeWork;
short	XSaveLockValid;
short	SMakeMode;
BPTR	XSaveLock;
char     version[] = VERSTAG ", "VEREXTRA"\0 Copyright 1994, O.I.C.\n";
char	*XFileName = "SDMakefile";
short	FileSpecified = 0;
short	ExitCode;
char *console = 0;
struct Process *mycli = 0;
BPTR StdIn;
BPTR StdOut;
BPTR OrgIn;
BPTR OrgOut;
BPTR StdErr;
BPTR OrgErr;
struct MsgPort *OldPort;
struct climsg *clmsg;
struct	Library *UtilityBase = 0;

void xmyexit(void)
{
    if (XSaveLockValid) {
	CurrentDir(XSaveLock);
	XSaveLockValid = 0;
    }
}

void myexit(void)
{
    if (Break)
	PrintF("SDMAKE: ***Break\n");
    else
    {
	if (ExitCode < RETURN_FAIL)
	{
	    if (CheckTarget && QuietOpt < 2)
	    {
	        if (SomeWork)
		    PrintF("0\n");
	        else
		    PrintF("1\n");
	    }
	    else
	    if (QuietOpt == 0)
	    {
	        {
	            if (SomeWork)
	                PrintF("SDMAKE Done.\n");
	            else
	                PrintF("All Targets up to date.\n");
	        }
	    }
	}
    }

    if (OpenFiles)
    {
	struct FileList *nextfile;

	nextfile = OpenFiles->Next;

	CloseAsync(OpenFiles->File);

	while (nextfile)
	{
	    CloseAsync(nextfile->File);
	    nextfile = nextfile->Next;
	}

    }

    PDelete();
    if (UtilityBase)
	CloseLibrary(UtilityBase);

    if (StdOut)
    {
	char ch;

	PrintF("\nHit RETURN to Exit\n");
	while ((ch = FGetC(StdIn)) != '\n' && ch != -1);
	Flush(StdOut);
	Flush(StdOut);

	Close(StdOut);
	Close(StdIn);
    }
    flushall();
}

#if OSVERMAX >= 36
void procmsg(void)
{
    struct climsg msg;
    struct MsgPort *reply;

    reply = CreateMsgPort();

    msg.msg.mn_ReplyPort = reply;
    msg.data = getreg(REG_A4);

    PutMsg(&mycli->pr_MsgPort, (struct Message *)&msg);

    WaitPort(reply);
    GetMsg(reply);

    DeleteMsgPort(reply);
}

void cliproc(void)
{
    struct ExecBase * SysBase = *(struct ExecBase **)4;
    struct climsg *msg;
    struct Process *self;

    self = (struct Process *)FindTask((char *)NULL);

    WaitPort(&self->pr_MsgPort);
    msg	= (struct climsg *)GetMsg(&self->pr_MsgPort);

    putreg(REG_A4, (long)msg->data);
    clmsg = msg;

    {
	struct FileHandle *fh = (struct FileHandle *)BADDR(StdIn);

	DoPkt(fh->fh_Type, ACTION_CHANGE_SIGNAL, fh->fh_Arg1, (LONG)&self->pr_MsgPort, 0, 0, 0);
	fh = (struct FileHandle *)BADDR(StdOut);
	DoPkt(fh->fh_Type, ACTION_CHANGE_SIGNAL, fh->fh_Arg1, (LONG)&self->pr_MsgPort, 0, 0, 0);
    }

    SetProgramName("sdmake_cli");

    realmain();
}


LONG  __asm cliproc_exit(register __d0 LONG rc, register __d1 LONG exitdata)
{
    putreg(REG_A4, exitdata);

    Forbid();
    ReplyMsg((struct Message *)clmsg);

    return rc;
}


BOOL makecli(struct Process *sproc)
{
    struct TagItem *tags;
    BPTR path;

    if (!(tags=AllocVec(sizeof(struct TagItem)*14,MEMF_ANY)))
	return FALSE;

    if (path = stealpath(sproc))
    {
	struct MsgPort *ctask;

	if (StdIn = Open(console?console:"con:10/10/400/150/SDMake/CLOSE", MODE_OLDFILE))
	{
	    ctask = ((struct FileHandle *)BADDR(StdIn))->fh_Type;

	    {
		struct MsgPort *oldport;
		oldport	= SetConsoleTask(ctask);
		StdOut = Open("*", MODE_NEWFILE);
		SetConsoleTask(oldport);
	    }

	    if (StdOut)
	    {
		tags[0].ti_Tag=NP_Entry;
		tags[0].ti_Data=(ULONG)cliproc;
		tags[1].ti_Tag=NP_Name;
		tags[1].ti_Data=(ULONG)"sdmake_cli";
		tags[2].ti_Tag=NP_ExitCode;
		tags[2].ti_Data=(ULONG)cliproc_exit;
		tags[3].ti_Tag=NP_ExitData;
		tags[3].ti_Data=(ULONG)getreg(REG_A4);
		tags[4].ti_Tag=NP_StackSize;
		tags[4].ti_Data=8192;
		tags[5].ti_Tag=NP_Priority;
		tags[5].ti_Data=0;
		tags[6].ti_Tag=NP_Cli;
		tags[6].ti_Data=TRUE;
		tags[7].ti_Tag=NP_Path;
		tags[7].ti_Data=(ULONG)path;
		tags[8].ti_Tag=NP_Input;
		tags[8].ti_Data=(ULONG)StdIn;
		tags[9].ti_Tag=NP_CloseInput;
		tags[9].ti_Data=0;
		tags[10].ti_Tag=NP_Output;
		tags[10].ti_Data=(ULONG)StdOut;
		tags[11].ti_Tag=NP_CloseOutput;
		tags[11].ti_Data=0;
		tags[12].ti_Tag=NP_ConsoleTask;
		tags[12].ti_Data=(ULONG)ctask;
		tags[13].ti_Tag=TAG_END;

		mycli = CreateNewProc(tags);
	    }
	    else
		Close(StdIn);
	}
    }

    FreeVec(tags);

    if (!mycli)
    {
	if (path)
	    freepath(path);

	return FALSE;
    }

	return TRUE;
}
#endif

LONG realmain(void)
{
    int r;

    /*
     *	add built-inn variables
     *
     */

    MakeVariable("TOPDIR", '$');

    if (!FindVariable("HOST_OSVER", '$'))
    {
	Var *var;
	TEXT osver[5];

	sprintf(osver, "%d", SysBase->LibNode.lib_Version);

	var = MakeVariable("HOST_OSVER", '$');
	AppendVariable(var, osver, strlen(osver));
    }

    {
	Var *datevar = FindVariable("BUILDDATE", '$');
	Var *timevar = FindVariable("BUILDTIME", '$');

	if (!datevar || !timevar)
#if OSVERMIN < 36 && OSVERMAX >= 36
	if (DOSBase->dl_lib.lib_Version >= 36)
#endif
	{
#if OSVERMAX >= 36
	    struct DateTime buildtime;
	    TEXT date[LEN_DATSTRING], time[LEN_DATSTRING];

	    buildtime.dat_Format = FORMAT_CDN;
	    buildtime.dat_Flags = 0;
	    buildtime.dat_StrDay = 0;
	    buildtime.dat_StrDate = date;
	    buildtime.dat_StrTime = time;

	    DateStamp(&buildtime.dat_Stamp);
	    DateToStr(&buildtime);
	    {
		if (!datevar)
		{
		    datevar = MakeVariable("BUILDDATE", '$');
		    AppendVariable(datevar, date, strlen(date));
		}
		if (!timevar)
		{
		    timevar = MakeVariable("BUILDTIME", '$');
		    AppendVariable(timevar, time, strlen(time));
		}
	    }
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
	}
	else
	{
#endif
#if OSVERMIN < 36
	    time_t buildt;
	    struct tm *buildtime;
	    TEXT datestr[9], timestr[9];

	    time(&buildt);
	    buildtime = localtime(&buildt);
	    strftime(datestr, sizeof(datestr), "%d-%m-%y", buildtime);
	    strftime(timestr, sizeof(timestr), "%H:%M:%S", buildtime);

	    {
		if (!datevar)
		{
		    datevar = MakeVariable("BUILDDATE", '$');
		    AppendVariable(datevar, datestr, strlen(datestr));
		}
		if (!timevar)
		{
		    timevar = MakeVariable("BUILDTIME", '$');
		    AppendVariable(timevar, timestr, strlen(timestr));
		}
	    }
#endif
	}
    }

    /*
     *	resolve dependancies requested by the user.  If none requested
     *	the resolve the first one
     */
    {
	DepRef *node;

	if (!FileSpecified)
	{
	    BPTR lock;
	    WORD i;

	    for (i = 0; !(lock = Lock(XFileName, ACCESS_READ)); i++)
	    {
		if (i == 1)
		    *XFileName = 'S';
		else
		if (i < 3)
		    XFileName++;
		else
		    break;
	    }

	    if (lock)
		UnLock(lock);
	    else
	    {
		error(FATAL, "Unable to to open (SDM|DM|SM|M)akefile");

	    }
	}

	if (!(strnicmp(XFileName, "smakefile", 10)))
	{
	    BPTR deffile;

	    if (deffile = Lock("ENV:SDMake/SDMake-SMake.def", ACCESS_READ))
	    {
		UnLock(deffile);
		ParseFile("ENV:SDMake/SDMake-SMake.def");
	    }

	    SMakeMode = 1;
	}

	ParseFile(XFileName);

	if (GetHead(&DoList) == NULL) {
	    if ((node = (DepRef *)GetHead(&DepList)) != NULL)
		CreateDepRef(&DoList, ((DepNode *)node)->dn_Node.ln_Name);
	}

	while ((node = (DepRef *)RemHead(&DoList)) != NULL) {
	    if ((node->rn_Dep->dn_Node.ln_Type != NT_RESOLVED) &&
	       (GetHead(&node->rn_Dep->dn_DepCmdList) == NULL))
	    {
		error(FATAL, "Unable to find %s", node->rn_Node.ln_Name);
		break;
	    }

	    if ((r = ExecuteDependency(NULL, node)) < DN_CHANGED)
	    {
		if (r < DN_ERROR)
		    ExitCode = RETURN_FAIL;
		else
		    ExitCode = RETURN_ERROR;

		if (OnError)
		{
		    node = CreateDepRef(NULL, OnError);
		    ExecuteDependency(NULL, node);
		}
		break;
	    }
	    else
	    if (CheckTarget)
	    {
		if (r < DN_NOCHANGE)
		    ExitCode = RETURN_WARN;
	    }
	}
    }

    return(ExitCode);
}

int main(ULONG argc, char *argv[])
{
    InitStuff();

    if (argc == 0)
    {
	struct WBStartup *wbs = (struct WBStartup *)argv;
	struct DiskObject *dob;
	short i;
	short j;
	WORD restart;

#if OSVERMIN < 36 && OSVERMAX >= 36
	if (DOSBase->dl_lib.lib_Version >= 36)
#endif
#if OSVERMAX >= 36
	    restart = 1;
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
	else
#endif
#if OSVERMIN < 36
	    restart = 0;
#endif

	/*
	 *  Search for options, set current directory to last valid
	 *  disk object
	 */


	for (i = 0; i < wbs->sm_NumArgs; ++i) {
	    BPTR saveLock = CurrentDir((BPTR)wbs->sm_ArgList[i].wa_Lock);

	    if (i == wbs->sm_NumArgs - 1 && FileSpecified == 0)
	    {
		BPTR testlock;

		if (testlock = Lock(wbs->sm_ArgList[i].wa_Name,ACCESS_READ))
		{
		    UnLock(testlock);

	            if (!(XFileName = PAlloc(strlen(wbs->sm_ArgList[i].wa_Name) + 1)))
		        MemErr();

	            strcpy(XFileName, wbs->sm_ArgList[i].wa_Name);
		    FileSpecified = 1;
		}
	    }
	    if (dob = GetDiskObject(wbs->sm_ArgList[i].wa_Name)) {
	        if (dob->do_ToolTypes)
		{
		    List tmpList;

		    NewList(&tmpList);

		    for (j = 0; dob->do_ToolTypes[j]; ++j) {
			char *ptr = dob->do_ToolTypes[j];

			if (strnicmp(ptr, "FILE=", 5) == 0) {
			    const char *xptr = SkipAss(ptr);
			     if (!(XFileName = PAlloc(strlen(xptr) + 1)))
				MemErr();
			    strcpy(XFileName, xptr);
			    FileSpecified = 1;
			} else if (strnicmp(ptr, "DRYRUN=", 7) == 0) {
			    NoRunOpt = atol(SkipAss(ptr));
			} else if (strnicmp(ptr, "TARGET=", 7) == 0) {
			    CreateDepRef(&DoList, SkipAss(ptr));
			} else if (strnicmp(ptr, "DEFINE=", 7) == 0) {
			    const char *p1 = SkipAss(ptr);
			    char *p2;
			    Var *var;
			    if ((p2 = strchr(p1, '=')) != NULL)
			        *p2++ = 0;
			    var = MakeVariable(p1, '$');
			    if (*p2) {
				ExpandVariable(p2, &tmpList);
			        AppendCmdList(&tmpList, &var->var_CmdList);
		            }
			} else if (strnicmp(ptr, "DOALL=", 6) == 0) {
			    DoAll = atol(SkipAss(ptr));
			} else if (strnicmp(ptr, "CACHE=", 6) == 0) {
			    CacheLevel = atol(SkipAss(ptr)) - 1;
			} else if (strnicmp(ptr, "FAILAT=", 7) == 0) {
			    DefFailat = atol(SkipAss(ptr));
			} else if (strnicmp(ptr, "IGNORE=", 7) == 0) {
			    DefIgnore = atol(SkipAss(ptr));
			} else if (strnicmp(ptr, "SILENT=", 7) == 0) {
			    QuietCmd = atol(SkipAss(ptr));
			} else if (strnicmp(ptr, "QUIET=", 6) == 0) {
			    QuietOpt = atol(SkipAss(ptr));
			} else if (strnicmp(ptr, "DEBUG=", 6) == 0) {
			    DDebug = atol(SkipAss(ptr));
			} else if (strnicmp(ptr, "CONSOLE=", 8) == 0) {
			    const char *cptr = SkipAss(ptr);
			    if (!(console = PAlloc(strlen(cptr) + 1)))
				MemErr();
			    strcpy(console, cptr);
			}
		    }
		}
	        FreeDiskObject(dob);
	    }
	    CurrentDir(saveLock);
	}

	XSaveLock = CurrentDir((BPTR)wbs->sm_ArgList[wbs->sm_NumArgs-1].wa_Lock);
	XSaveLockValid = 1;
	atexit(xmyexit);

#if OSVERMIN < 36 && OSVERMAX >= 36
	if (restart)
	{
#endif
#if OSVERMAX >= 36
	    if (makecli(wbs->sm_Message.mn_ReplyPort->mp_SigTask))
	        procmsg();
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
	}
	else
	{
#endif
#if OSVERMIN < 36
	    OpenConsole(console?console:"con:10/10/400/150/SDMake/CLOSE");
	    realmain();
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
	}
#endif
    }
    else
    {
	short i;
	List tmpList;
	Var *var;
	NewList(&tmpList);

	for (i = 1; i < argc; ++i) {
	    char *ptr = argv[i];
	    char *p2;

	    if (*ptr != '-') {
		if (ptr[0] == '?' && ptr[1] == 0)
		{
		    QuietOpt = 1;
		    help(1);
		}
		else
		{
		    CreateDepRef(&DoList, ptr);
		    continue;
		}
	    }
	    ptr += 2;
	    switch(ptr[-1]) {
	    case 'f':
	        XFileName = (*ptr) ? ptr : argv[++i];
		FileSpecified = 1;
	        break;
	    case 'F':
		DefFailat = (*ptr) ? atoi(ptr) : RETURN_WARN + 1;
	        break;
	    case 'q':
		CheckTarget = 1;
		QuietCmd = 1;
		QuietOpt += 1;
	    case 'n':
		NoRunOpt = 1;
	        break;
	    case 't':
		TouchAll = 1;
		NoRunOpt = 1;
		QuietCmd = 1;
	        break;

	    case 'D':
	        ptr = (*ptr) ? ptr : argv[++i];
	        if ((p2 = strchr(ptr, '=')) != NULL)
		    *p2++ = 0;
	        var = MakeVariable(ptr, '$');
	        if (p2) {
		    ExpandVariable(p2, &tmpList);
		    AppendCmdList(&tmpList, &var->var_CmdList);
	        }
	        break;

	    case 'd':
	        DDebug = (*ptr) ? atoi(ptr) : 1;
	        break;
	    case 'c':
	        CacheLevel = (*ptr) ? atoi(ptr) - 1 : 0;
	        break;
	    case 'i':
		DefIgnore = 1;
	    case 's':
	        QuietCmd = 1;
		break;
	    case 'a':
	        DoAll = 1;
		break;
	    case 'S':
		QuietOpt += 1;
	        break;
	    case 'h':
	    default:
	        QuietOpt = 1;
	        help(1);
	    }
	}

	if (QuietOpt == 0)
	    puts(VERS " (" DATE ")");

	if (i > argc)
	    error(FATAL, "Expected argument to command line option");

	return realmain();
    }
}

void PrintF(CONST_STRPTR ctl, ...)
{
    if (StdOut)
    {
	VFPrintf(StdOut, ctl, (LONG *)(&ctl + 1));
	Flush(StdOut);
    }
    else
    {
	va_list va;

	va_start(va, ctl);
	vprintf(ctl, va);
	va_end(va);
	fflush(stdout);
    }
}

void MemErr(void)
{
    PrintF("Fatal error: memory allocation failed");

    ExitCode = RETURN_FAIL;
#if OSVERMAX >= 36
    if (mycli == (struct Process *)FindTask(NULL))
	Exit(RETURN_FAIL);
    else
#endif
	exit(RETURN_FAIL);
}

static void InitStuff(void)
{
    static int Initialized;

    if (Initialized == 0) {
	Initialized = 1;
	if (!(MemPool = PCreate(8192, 384)))
	    MemErr();
	atexit(myexit);
	UtilityBase = OpenLibrary("utility.library", 37);
#if OSVERMIN >= 37
	if (!UtilityBase)
	{
	    PrintF("Fatal error: opening utility.library failed");

	    ExitCode = RETURN_FAIL;
	    exit(RETURN_FAIL);
	}
#endif
	NewList(&DoList);
	InitCommand();
	InitCmdList();
	InitVariable();
	InitDep();
	InitParser();
    }
}

void help(int code)
{
    puts("SDMake V0.1 © 1991-2003 Matthew Dillon, © 2026 Hagbard Celine");
    puts("Distributed WITHOUT ANY WARRANTY, under terms of GNU General Public License version 2");
    puts("SDMmake [-f file] [-n] [-Dvariable[=value]] [-d[N]] [-F[N]] [-i] [-a] [-c[N]] [-s] [-S] [-t] [-q] [-h] [targets...]");
    exit(code);
}

const char *SkipAss(const char *ptr)
{
    while (*ptr && *ptr != '=')
	++ptr;
    if (*ptr == '=') {
	for (++ptr; *ptr == ' ' || *ptr == '\t'; ++ptr)
	    ;
    }
    return(ptr);
}

#if OSVERMIN < 36
struct IntuiText *ITextOf(char *ptr)
{
    static struct IntuiText ITAry[8];
    static short ITIdx;
    struct IntuiText *it = ITAry + ITIdx;

    ITIdx = (ITIdx + 1) & 7;
    it->FrontPen = 1;
    it->BackPen  = 0;
    it->DrawMode = JAM2;
    it->LeftEdge = 2;
    it->TopEdge = 6;
    it->IText = (unsigned char *)ptr;
    return(it);
}
#endif

