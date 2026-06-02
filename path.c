
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
 */

#include "defs.h"

#define PATH_ABSOLUTE 3
#define PATH_ROOT     2
#define PATH_RELATIVE 1
#define PATH_REGULAR  0
#define PATH_ERROR   -1

Prototype BPTR LoadSegPath(char *cmd);
Prototype BPTR stealpath(struct Process *sproc);
Prototype void freepath(BPTR list);

static BOOL IsExecutableFile(BPTR lock, STRPTR namebuf);
static WORD PathType(STRPTR cmd);
static BPTR SearchMulti(STRPTR device, CONST_STRPTR path, BPTR pathlock);


typedef struct Process Process;

struct PathComponent {
    BPTR pc_Next;
    BPTR pc_Lock;
};

extern struct ExecBase *SysBase;

static BOOL IsExecutableFile(BPTR lock, STRPTR namebuf)
{
    struct FileInfoBlock *fib;
    BOOL isexe = FALSE;
    LONG ioerr = 0;

    if (fib = AllocDosObject(DOS_FIB, 0))
    {
	if (Examine(lock, fib))
	{
	    if (fib->fib_EntryType < 0 && !(fib->fib_Protection&(FIBF_EXECUTE|FIBF_SCRIPT)))
	    {
		isexe = TRUE;
		if (namebuf)
		    stccpy(namebuf, fib->fib_FileName, 108);
	    }
	}
	else
	{
	    ioerr = IoErr();
	    isexe = -1;
	}
	FreeDosObject(DOS_FIB, fib);
    }
    else
	return -1;

    SetIoErr(ioerr);
    return isexe;
}

static WORD PathType(STRPTR cmd)
{
    STRPTR ptr;
    WORD pathtype = PATH_REGULAR;

    if (cmd && *cmd)
    {
	for (ptr = cmd; *ptr; ptr++)
	{
	    if (*ptr == ':')
	    {
		if (ptr == cmd)
		    pathtype = PATH_ROOT;
		else
		    pathtype = PATH_ABSOLUTE;
		break;
	    }
	    if (*ptr == '/')
		pathtype = PATH_RELATIVE;
	}
    }
    else
    {
	SetIoErr(ERROR_INVALID_COMPONENT_NAME);
	pathtype = PATH_ERROR;
    }

    return pathtype;
}

static BPTR SearchMulti(STRPTR device, CONST_STRPTR path, BPTR pathlock)
{
    BPTR seglist = 0;
    struct DevProc *dp[16] = {0};
    STRPTR dev[15] = {0};
    BPTR lock;
    char *buf[16];
    char *linkbuf[15];
    LONG ioerr = 0;
    struct MsgPort *pathport;
    WORD linknum = 0;

    if (!path)
	return 0;

    {
	LONG len = strlen(path);

	if (len > 255)
	{
	    SetIoErr(ERROR_INVALID_COMPONENT_NAME);
	    return 0;
	}

	if (!(buf[0] = PAllocVec(257)))
	{
	    return 0;
	}
	*buf[0] = len;
	stccpy(buf[0] + 1, path, len + 1);
    }

    if (!device)
    {
	struct FileLock *fl;

	if (!pathlock)
	    pathlock = WorkProc->pr_CurrentDir;

	fl = BADDR(pathlock);
	pathport = fl->fl_Task;
	goto try_lock;
    }
    else
	dev[0] = device;


    while (TRUE)
    {
	if ((dp[linknum] = GetDeviceProc(dev[linknum], dp[linknum])))
	{
	    pathlock = dp[linknum]->dvp_Lock;

	    pathport = dp[linknum]->dvp_Port;
try_lock:
	    {
		WORD cont = 0;

		while (!(lock = DoPkt3(pathport,
				    ACTION_LOCATE_OBJECT,
				    pathlock,
				    MKBADDR(buf[linknum]),
				    SHARED_LOCK)) &&
		    (ioerr = IoErr()) == ERROR_IS_SOFT_LINK)
		{
		    WORD pathtype;

		    if (linknum >= 15)
		    {
			ioerr = ERROR_TOO_MANY_LEVELS;
			break;
		    }

		    if (!(buf[++linknum] = PAllocVec(258 + 288 + 32)))
		    {
			ioerr = IoErr();
			cont = -1;
			break;
		    }

		    linkbuf[linknum - 1] = buf[linknum] + 258;
		    dev[linknum] = linkbuf[linknum - 1] + 288;

		    if ((ReadLink(pathport, pathlock,
		    buf[linknum - 1] + 1, linkbuf[linknum - 1], 256)) > 0)
		    {
			STRPTR src, dst;

			src = linkbuf[linknum - 1];

			if ((pathtype = PathType(src)) == PATH_ABSOLUTE)
			{
			    WORD i = -1;
			    dst = dev[linknum];

			    do
			    {
				i++;
				if (i > 31)
				{
				    ioerr = ERROR_INVALID_COMPONENT_NAME;
				    cont = -1;
				    break;
				}
				dst[i] = *src;
				src++;
			    } while (dst[i] != ':');

			    if (cont < 0)
				break;

			    dst[i + 1] = 0;

			    cont = 1;
			}

			dst = buf[linknum] + 1;

			*buf[linknum] = stccpy(dst, src, 256) - 1;

			if (cont > 0)
			    break;
		    }
		    else
		    {
			ioerr = IoErr();
			if (ioerr != ERROR_OBJECT_WRONG_TYPE &&
			    ioerr != ERROR_OBJECT_NOT_FOUND)
			{
			    cont = -1;
			    break;
			}
		    }

		}

		if (cont > 0)
		{
		    continue;
		}
		else
		if (cont < 0)
		{
		    break;
		}
	    }

	    if (lock)
	    {
		TEXT filename[108];
		BOOL isexe;

		if ((isexe = IsExecutableFile(lock, filename)) == -1)
		{
		    ioerr = IoErr();
		    break;
		}

		if (isexe)
		{
		    BPTR parent;

		    if (parent = ParentDir(lock))
		    {
			BPTR orgdir;

			orgdir = CurrentDir(parent);
			seglist = LoadSeg(filename);
			UnLock(CurrentDir(orgdir));
		    }

		    UnLock(lock);

		    if (seglist)
			break;
		}
		else
		    UnLock(lock);
	    }


	}
	else
	{
	    ioerr = IoErr();
	    if (!linknum)
		break;
	}

	while (linknum && (!dp[linknum] || dp[linknum] && !(dp[linknum]->dvp_Flags&DVPF_ASSIGN)))
	{
	    if (dp[linknum])
	    {
		FreeDeviceProc(dp[linknum]);
		dp[linknum] = 0;
	    }

	    if (buf[linknum])
	    {
		PFreeVec(buf[linknum]);
		buf[linknum] = 0;
	    }

	    linknum--;
	}

	if (!(dp[linknum] && (dp[linknum]->dvp_Flags & DVPF_ASSIGN)))
	    break;
    }

    while (linknum >= 0)
    {
	if (dp[linknum])
	{
	    FreeDeviceProc(dp[linknum]);
	    dp[linknum] = 0;
	}

	if (buf[linknum])
	{
	    PFreeVec(buf[linknum]);
	    buf[linknum] = 0;
	}

	linknum--;
    }

    if (seglist)
	ioerr = 0;

    SetIoErr(ioerr);

    return seglist;
}


BPTR LoadSegPath(char *cmd)
{
    BPTR seglist;
    struct PathComponent *path;
    char devname[32];
    WORD pathtype;

    if ((pathtype = PathType(cmd)) == PATH_ERROR)
	return 0;

    if (pathtype == PATH_ABSOLUTE)
    {
	char *src = cmd;
	char *dst = devname;

	do
	{
	    *dst = *src;
	    src++;

	} while (*dst++ != ':');

	*dst = 0;
	seglist = SearchMulti(devname, src, NULL);
    }
    else
	seglist = SearchMulti(NULL, cmd, NULL);

    if (IoErr() == ERROR_NO_FREE_STORE)
	return 0;

    if (!seglist && pathtype == PATH_REGULAR)
    {
	path = (struct PathComponent *)BADDR(((CLI *)BADDR(WorkProc->pr_CLI))->cli_CommandDir);

	while (path)
	{
	    if (path->pc_Lock)
	    {
		if (seglist = SearchMulti(NULL, cmd, path->pc_Lock))
		    break;
		else
		if (IoErr() == ERROR_NO_FREE_STORE)
		    return 0;
	    }
	    path = (struct PathComponent *)BADDR(path->pc_Next);
	}

	if (!seglist)
	    seglist = SearchMulti("C:", cmd, NULL);
    }


    return seglist;
}

BPTR stealpath(struct Process *sproc)
{
    struct CommandLineInterface	*scli;
    struct PathComponent *paths, *newpath = 0;
    BPTR path = 0;

    if (sproc && sproc->pr_Task.tc_Node.ln_Type == NT_PROCESS)
    {
	if (scli = BADDR(sproc->pr_CLI))
	{
	    for (paths = BADDR(scli->cli_CommandDir); paths; paths = BADDR(paths->pc_Next))
	    {
		BPTR lock;
		struct PathComponent *pathd;

		if (lock = DupLock(paths->pc_Lock))
		{
		    if (pathd = (struct PathComponent *)AllocVec(sizeof(struct PathComponent), MEMF_PUBLIC|MEMF_CLEAR))
		    {
			pathd->pc_Lock = lock;
			if (!path)
			    path = MKBADDR(pathd);
			else
			    newpath->pc_Next = MKBADDR(pathd);
			newpath = pathd;
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
    struct PathComponent *path;

    if (!(path=(struct PathComponent *)BADDR(list)))
	return;

    while (path)
    {
	struct PathComponent *next=(struct PathComponent *)BADDR(path->pc_Next);

	UnLock(path->pc_Lock);
	FreeVec(path);

	path=next;
    }
}
