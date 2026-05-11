
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
 *  PATH.C
 *
 *  Search the path for a command name
 */

#include <dos/dos.h>
#include <exec/types.h>
#include <exec/execbase.h>
#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <proto/dos.h>
#include <stdio.h>
#include "defs.h"

Prototype long _SearchPath(char *cmd);
Prototype BPTR stealpath(struct Process *sproc);
Prototype void freepath(BPTR list);

typedef struct Process Process;

typedef struct LockList {
    BPTR    NextPath;
    BPTR    PathLock;
} LockList;

extern struct ExecBase *SysBase;

long _SearchPath(char *cmd)
{
    CLI *cli;
    LockList *ll;

    if (((struct Task *)WorkProc)->tc_Node.ln_Type != NT_PROCESS)
	return(0);
    if ((cli = (CLI *)BADDR(WorkProc->pr_CLI)) == NULL)
	return(0);

    ll = (LockList *)BADDR(cli->cli_CommandDir);

    while (ll) {
	if (ll->PathLock) {
	    long oldLock = CurrentDir(ll->PathLock);
	    long lock;

	    if (lock = Lock(cmd, SHARED_LOCK)) {
		CurrentDir(oldLock);
		return(lock);
	    }
	    CurrentDir(oldLock);
	}
	ll = (LockList *)BADDR(ll->NextPath);
    }
    return(0);
}

#if OSVERMAX >= 36
BPTR stealpath(struct Process *sproc)
{
    struct CommandLineInterface	*scli;
    LockList *lls, *newpath = 0;
    BPTR path = 0;

    if (sproc && sproc->pr_Task.tc_Node.ln_Type == NT_PROCESS)
    {
	if (scli = BADDR(sproc->pr_CLI))
	{
	    for (lls = BADDR(scli->cli_CommandDir); lls; lls = BADDR(lls->NextPath))
	    {
		BPTR lock;
		LockList *ll;

		if (lock = DupLock(lls->PathLock))
		{
		    if (ll = (LockList *)AllocVec(sizeof(LockList), MEMF_PUBLIC|MEMF_CLEAR))
		    {
			ll->PathLock = lock;
			if (!path)
			    path = MKBADDR(ll);
			else
			    newpath->NextPath = MKBADDR(ll);
			newpath = ll;
		    }
		    else
		    {
			UnLock(lock);
			if (path)
			    freepath(path);
			return 0;
		    }
		}
	    }
	}
    }
    return path;
}

void freepath(BPTR list)
{
    LockList *path;

    if (!(path=(LockList *)BADDR(list)))
	return;

    while (path)
    {
	LockList *next=(LockList *)BADDR(path->NextPath);

	UnLock(path->PathLock);
	FreeVec(path);

	path=next;
    }
}
#endif
