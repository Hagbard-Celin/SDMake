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
int realmain(int ac, char **av);
void help(int);
static void InitStuff(void);
const char *SkipAss(const char *);
struct IntuiText *ITextOf(char *);

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
Prototype short SomeWork;
Prototype APTR  MemPool;

List	DoList;
STRPTR	OnError;
APTR    MemPool;
short	DDebug;
short	CacheLevel;
short	NoRunOpt;
short	CheckTarget;
short	QuietOpt;
short	QuietCmd;
short	DoAll;
short   SomeWork;
short	XSaveLockValid;
BPTR	XSaveLock;
char     version[] = VERSTAG "\0 Copyright 1994, O.I.C.\n";
char	*XFileName = "SDMakefile";
short	FileSpecified = 0;
short	ExitCode;
struct Library *UtilityBase = 0;

void xmyexit(void)
{
    if (XSaveLockValid) {
	CurrentDir(XSaveLock);
	XSaveLockValid = 0;
    }
}

void myexit(void)
{
    if (ExitCode < 20)
    {
	if (QuietOpt == 0)
	{
	    if (CheckTarget)
	    {
	        if (SomeWork)
		    printf("0\n");
	        else
		    printf("1\n");
	    }
	    else
	    {
	        if (SomeWork)
	            printf("SDMAKE Done.\n");
	        else
	            printf("All Targets up to date.\n");
	    }
	}
    }

    PDelete();
    if (UtilityBase)
	CloseLibrary(UtilityBase);
}


void wbmain(struct WBStartup *wbs)
{
    struct DiskObject *dob;
    short i;
    short j;
    short abortIt = 0;

    /*
     *	Search for options, set current directory to last valid
     *	disk object
     */

    InitStuff();

    for (i = 0; i < wbs->sm_NumArgs; ++i) {
	BPTR saveLock = CurrentDir((BPTR)wbs->sm_ArgList[i].wa_Lock);

	if (i == wbs->sm_NumArgs - 1 && FileSpecified == 0)
	{
	    if (!(XFileName = PAlloc(strlen(wbs->sm_ArgList[i].wa_Name) + 1)))
		MemErr();
	    strcpy(XFileName, wbs->sm_ArgList[i].wa_Name);
	}

	if (dob = GetDiskObject(wbs->sm_ArgList[i].wa_Name)) {
	    if (dob->do_ToolTypes)
	    {
	        for (j = 0; dob->do_ToolTypes[j]; ++j) {
		    char *ptr = dob->do_ToolTypes[j];

		    if (strnicmp(ptr, "FILE=", 5) == 0) {
		        const char *xptr = SkipAss(ptr);
		        if (!(XFileName = PAlloc(strlen(xptr) + 1)))
			    MemErr();
		        strcpy(XFileName, xptr);
		        FileSpecified = 1;
		    } else if (strnicmp(ptr, "DRYRUN=", 7) == 0) {
		        NoRunOpt = strtol(SkipAss(ptr), NULL, 0);
		    } else if (strnicmp(ptr, "TARGET=", 7) == 0) {
		        CreateDepRef(&DoList, SkipAss(ptr));
		    } else if (strnicmp(ptr, "QUIET=", 6) == 0) {
		        QuietOpt = strtol(SkipAss(ptr), NULL, 0);
		    } else if (strnicmp(ptr, "DEBUG=", 6) == 0) {
		        DDebug = strtol(SkipAss(ptr), NULL, 0);
		    } else if (strnicmp(ptr, "CONSOLE=", 8) == 0) {
		        OpenConsole(SkipAss(ptr)); /*  lib/misc.h  */
		    } else {
		        char buf[64];

		        sprintf(buf, "Bad ToolType: %s", ptr);
		        switch(AutoRequest(NULL, ITextOf(ptr), ITextOf("Ignore"), ITextOf("Abort"), 0, 0, 300, 40)) {
		        case 1:
			    break;
		        case 0:
			    abortIt = 1;
			    break;
		        }
		    }
		    if (abortIt)
		        break;
	        }
	    }
	    FreeDiskObject(dob);
	}
	CurrentDir(saveLock);
	if (abortIt)
	    break;
    }

    XSaveLock = CurrentDir((BPTR)wbs->sm_ArgList[wbs->sm_NumArgs-1].wa_Lock);
    XSaveLockValid = 1;
    atexit(xmyexit);

    if (abortIt == 0)
	realmain(1, NULL);
}


int realmain(int ac, char **av)
{
    short i;
    int r = 0;
    List tmpList;
    Var *var;
    NewList(&tmpList);

    InitStuff();

    /*printf("ARGS= %d\n", ac);*/
    for (i = 1; i < ac; ++i) {
	char *ptr = av[i];
	char *p2;

	/*printf("ARG[%d]= %d:%s\n", i, strlen(av[i]), av[i]);*/

	if (*ptr != '-') {
	    CreateDepRef(&DoList, ptr);
	    continue;
	}
	ptr += 2;
	switch(ptr[-1]) {
	case 'F':   /*  fast opt    */
	    break;
	case 'f':
	    XFileName = (*ptr) ? ptr : av[++i];
	    break;
	case 'Q':
	    QuietOpt = 1;
	case 'q':
	    CheckTarget	= 1;
	    QuietCmd = 1;
	case 'n':
	    NoRunOpt = 1;
	    break;

	case 'D':
	    ptr = (*ptr) ? ptr : av[++i];
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
	case 's':
	    QuietCmd = 1;
	case 'a':
	    DoAll = 1;
	case 'S':
	    QuietOpt = 1;
	    break;
	case 'h':
	default:
	    QuietOpt = 1;
	    help(1);
	}
    }
    if (QuietOpt == 0)
	puts(VERS " (" DATE ")");

    if (i > ac)
	error(FATAL, "Expected argument to command line option");

    /*
     *	resolve dependancies requested by the user.  If none requested
     *	the resolve the first one
     */

    (void)MakeVariable("TOPDIR", '$');

    {
	Var *var;
	TEXT osver[5];

	sprintf(osver, "%d", SysBase->LibNode.lib_Version);

	var = MakeVariable("HOST_OSVER", '$');
	AppendVariable(var, osver, strlen(osver));
    }

#if OSVERMIN < 36 && OSVERMAX >= 36
    if (DOSBase->dl_lib.lib_Version >= 36)
    {
#endif
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
	    Var *var;

	    var = MakeVariable("BUILDDATE", '$');
	    AppendVariable(var, date, strlen(date));
	    var = MakeVariable("BUILDTIME", '$');
	    AppendVariable(var, time, strlen(time));
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
	    Var *var;

	    var = MakeVariable("BUILDDATE", '$');
	    AppendVariable(var, datestr, strlen(datestr));
	    var = MakeVariable("BUILDTIME", '$');
	    AppendVariable(var, timestr, strlen(timestr));
	}
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
    }
#endif

    {
	DepRef *node;

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

	    if ((r = ExecuteDependency(NULL, node)) < 0)
	    {
		if (OnError)
		{
		    node = CreateDepRef(NULL, OnError);
		    ExecuteDependency(NULL, node);
		}
		break;
	    }
	}
    }
    if (r < 0 && ExitCode < 20)
	ExitCode = 20;
    if (CheckTarget && !SomeWork)
	ExitCode = 5;
    return(ExitCode);
}

int main(ULONG argc, char *argv[])
{
    if (argc == 0)
    {
	wbmain((struct WBStartup *)argv);
    }
    else
    {
	realmain(argc, argv);
    }

    return 0;
}

void MemErr(void)
{
    printf("Fatal error: memory allocation failed");
    exit(20);
}

static void InitStuff()
{
    static int Initialized;

    if (Initialized == 0) {
	Initialized = 1;
	if (!(MemPool = PCreate(8192, 384)))
	    MemErr();
	atexit(myexit);
	UtilityBase = OpenLibrary("utility.library", 37);
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
    puts("Distributed with WITHOUT ANY WARRANTY, under terms of GNU General Public License version 2");
    puts("SDMmake [-f file] [-n] [-Dvariable] [-d] [-a] [-q] [-h]");
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

