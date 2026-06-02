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
 */

/*
 *  RUN.C
 */

#include "defs.h"
#include <math.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <dos/dostags.h>

struct bufdata {
    struct FileHandle *fh;
    BPTR oldBuf;
    LONG oldPos;
    LONG oldEnd;
};

typedef struct Process		    Process;

Prototype long Execute_Command(char **cmdptr, WORD *cmdflags, IfNode **cmdIfBase, LONG *cmdIfTrue, LONG *lastret, LONG cmdsize);
Prototype void InitCommand(void);
Prototype void SetReturnVar(LONG rc, LONG return2);

#if OSVERMAX >= 36
#if OSVERMIN < 37
static void RunCleanup(char *cmdArgs, struct bufdata *fhb);
#else
static void RunCleanup(char *cmdArgs);
#endif
static void SetLocalVar(CONST_STRPTR var, LONG value);
#endif

BPTR SaveLock;
#if OSVERMIN < 36
char RootPath[256];
#endif

void ICExit(void)
{
    if (SaveLock) {
	UnLock(CurrentDir(SaveLock));
	SaveLock = NULL;
    }
}

void InitCommand()
{
    BPTR path;

#if OSVERMIN < 36
    if (DOSBase->dl_lib.lib_Version < 36)
    {
	char *ok;

	strcpy(RootPath, "cd ");

	ok = getcwd(RootPath + 3, sizeof(RootPath) - 3);

	if (!ok)
	{
	    PrintF("CurrentDir error, path too long\n");
	    exit(20);
	}
    }
#endif

    path = DupLock(StartProc->pr_CurrentDir);
    SaveLock = CurrentDir(path);

    atexit(ICExit);
}

/*
 *  cmd[-1] is valid space and, in fact, must be long word aligned!
 */

long Execute_Command(char **cmdptr, WORD *cmdflags, IfNode **cmdIfBase, LONG *cmdIfTrue, LONG *lastret, long cmdsize)
{
    register char *ptr;
    WORD flags = *cmdflags;
    char *cmd = *cmdptr;

    for (ptr = cmd; *ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n'; ++ptr)
	;


    /*
     *	Internal MakeDir because AmigaDOS 2.04's MakeDir is unreliable
     *	with RunCommand() (it can crash)
     *  Internal makedir disabled by default for SDMake, can not reproduce
     *  crash.
     *
     *	Internal CD, IF, ELSE and ENDIF because we special case it
     */

    {
	LONG cmdlen = ptr - cmd;

	if (cmdlen == 5 && strnicmp(cmd, "endif", 5) == 0) {
	    LONG isif = *cmdIfTrue;

	    *cmdIfTrue = popIf(cmdIfBase);
	    if (!(flags&CMDF_QUIET) && !isif)
	    {
		if (!(*cmdIfTrue))
		    PrintF(" (Skipped by if-condition)\n");
		else
		    PrintF("\n");
	    }

	    return CMD_OK;
	} else
	if (cmdlen == 4 && strnicmp(cmd, "else", 4) == 0) {
	    LONG isif = *cmdIfTrue;

	    *cmdIfTrue = elseIf(cmdIfBase);

	    if (!(flags&CMDF_QUIET) && !isif)
	    {
		if (!(*cmdIfTrue))
		    PrintF(" (Skipped by if-condition)\n");
		else
		    PrintF("\n");
	    }

	    return CMD_OK;
	} else
	if (cmdlen == 2)
	{
	    if (strnicmp(cmd, "if", 2) == 0)
	    {
		char *t;
		short err = CMD_OK;
		WORD notfound = 1;

		while (*ptr == ' ' || *ptr == '\t')
		    ++ptr;
		if (*ptr)
		{
		    LONG arglen;

		    for (t = ptr; *t && *t != ' ' && *t != '\t'; t++);

		    arglen = t - ptr;

		    if (arglen == 2)
		    {
			WORD in, gt = 0;

			if ((in = strnicmp(ptr, "eq", 2)) == 0 ||
			    (gt = strnicmp(ptr, "in", 2)) == 0 ||
			    strnicmp(ptr, "gt", 2) == 0)
			{
			    if (*cmdIfTrue)
			    {
				ptr = t;

				while (*ptr == ' ' || *ptr == '\t')
				    ++ptr;

				if (*ptr)
				{
				    for (t = ptr; *t && *t != ' ' && *t != '\t'; t++);
				    arglen = t - ptr;

				    if (*t)
				    {
					*t = 0;

					do ++t; while (*t == ' ' || *t == '\t');

					if (*t)
					{
					    WORD result = 0;

					    notfound = 0;

					    if (in)
					    {
						if (gt)
						    result = (atol(ptr) > atol(t));
						else
						    result = StriInStr(ptr, t);
					    }
					    else
					    {
						if (strlen(t) == arglen)
						{
						    if (!(strnicmp(ptr, t, arglen)))
							result = 1;
						}
					    }

					    if (result) {
						*cmdIfTrue = pushIf(cmdIfBase, 1);
					    } else {
						*cmdIfTrue = pushIf(cmdIfBase, 0);
					    }
					}
				    }
				}
			    }
			    else
			    {
				*cmdIfTrue = pushIf(cmdIfBase, 0);
				if (!(flags&CMDF_QUIET))
				    PrintF(" (Skipped by if-condition)\n");
			    }
			}
		    }
		    else
		    if (arglen == 4 || arglen == 5)
		    {
			LONG val;

			if (((strnicmp(ptr, "WARN", 4) == 0) && (val = RETURN_WARN)) ||
			    ((strnicmp(ptr, "ERROR", 5) == 0) && (val = RETURN_ERROR)) ||
			    ((strnicmp(ptr, "FAIL", 2) == 0) && (val = RETURN_FAIL)))
			{
			    if (*cmdIfTrue)
			    {
				notfound = 0;

				if (*lastret >= val)
				{
					*cmdIfTrue = pushIf(cmdIfBase, 1);
				    }
				    else
				    {
					*cmdIfTrue = pushIf(cmdIfBase, 0);
				}
			    }
			    else
			    {
				*cmdIfTrue = pushIf(cmdIfBase, 0);
				if (!(flags&CMDF_QUIET))
				    PrintF(" (Skipped by if-condition)\n");
			    }
			}
		    }
		    else
		    if (arglen == 6)
		    {
			if (strnicmp(ptr, "exists", 6) == 0)
			{
			    if (*cmdIfTrue)
			    {
				ptr = t;


				while (*ptr == ' ' || *ptr == '\t')
				    ++ptr;

				if (*ptr)
				{
				    BPTR lock;

				    notfound = 0;

				    if (lock = Lock(ptr, ACCESS_READ))
				    {
					UnLock(lock);
					*cmdIfTrue = pushIf(cmdIfBase, 1);
				    }
				    else
				    {
					*cmdIfTrue = pushIf(cmdIfBase, 0);
				    }
				}
			    }
			    else
			    {
				*cmdIfTrue = pushIf(cmdIfBase, 0);
				if (!(flags&CMDF_QUIET))
				    PrintF(" (Skipped by if-condition)\n");
			    }
			}
		    }
		    else
		    if (arglen == 7)
		    {
			if (strnicmp(ptr, "defined", 7) == 0)
			{
			    if (*cmdIfTrue)
			    {
				ptr = t;

				while (*ptr == ' ' || *ptr == '\t')
				    ++ptr;

				if (*ptr)
				{
				    notfound = 0;

				    if (FindVariable(ptr, '$')) {
					*cmdIfTrue = pushIf(cmdIfBase, 1);
				    } else {
					*cmdIfTrue = pushIf(cmdIfBase, 0);
				    }
				}
			    }
			    else
			    {
				*cmdIfTrue = pushIf(cmdIfBase, 0);
				if (!(flags&CMDF_QUIET))
				    PrintF(" (Skipped by if-condition)\n");
			    }
			}
		    }
		}

		if (*cmdIfTrue)
		{
		    if (notfound)
		    {
			PrintF("Internal command if: Missing or unknown argument\n");
			err = CMD_ERROR;
		    }
		}

		return(err);
	    } else
	    if (!(*cmdIfTrue)) {
		if (!(flags&CMDF_QUIET))
		    PrintF(" (Skipped by if-condition)\n");
		return CMD_OK;
	    } else
	    if (strnicmp(cmd, "cd", 2) == 0) {
		long lock;
		CLI *cli = (CLI *)BADDR(WorkProc->pr_CLI);
		short err = CMD_OK;

		while (*ptr == ' ' || *ptr == '\t')
		    ++ptr;
		{
		    short len = strlen(ptr);    /*  XXX HACK HACK */
		    if (len && ptr[len-1] == '\n')
			ptr[len-1] = 0;
		}

		if (*ptr == 0)
		    lock = DupLock(SaveLock);
		else
		    lock = Lock(ptr, SHARED_LOCK);

		if (lock)
		{
		    UnLock(CurrentDir(lock));
#if OSVERMIN < 36 && OSVERMAX >= 36
		    if (ShellRunning && DOSBase->dl_lib.lib_Version < 36)
		    {
#endif
#if OSVERMIN < 36
			if (*ptr == 0)
			{
			    err = system13(RootPath);
			}
			else
			{
			    err = system13(cmd);
			}
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
		    }
#endif

		    cli->cli_Result2 = 0;
		    *lastret = 0;
		}

		if (!lock)
		{
		    cli->cli_Result2 = IoErr();
		    *lastret = 20;
		    PrintF("Unable to cd to %s\n", ptr);
		    err = CMD_ERROR;
		}
#if OSVERMIN < 36
		else
		if (err)
		{
		    PrintF("SDMake: fatal error while changing CurrentDir to %s\n", (*ptr)?ptr:RootPath + 3);
		    return CMD_FAIL;
		}
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
		if (DOSBase->dl_lib.lib_Version >= 36)
#endif
#if OSVERMAX >= 36
		    SetReturnVar(*lastret, cli->cli_Result2);
#endif
		return((flags&CMDF_IGNORE) ?  CMD_IGNORED : err);
	    }
	} else
	if (!(*cmdIfTrue)) {
	    if (!(flags&CMDF_QUIET))
		PrintF(" (Skipped by if-condition)\n");
	    return CMD_OK;
	} else
#ifdef INTERNAL_MAKEDIR
	if (cmdlen == 7 && strnicmp(cmd, "makedir", 7) == 0) {
	    long lock;
	    short err = CMD_OK;

	    while (*ptr == ' ' || *ptr == '\t')
		++ptr;
	    if (lock = CreateDir(ptr))
		UnLock(lock);
	    else {
		PrintF("Unable to makedir %s\n", ptr);
		err = CMD_ERROR;
	    }
	    return((flags&CMDF_IGNORE) ?  CMD_IGNORED : err);
	} else
#endif
	if (cmdlen == 6) {
	    LONG append;

	    if ((append = strnicmp(cmd, "fwrite", 6)) == 0 ||
	    strnicmp(cmd, "fappnd", 6) == 0)
	    {
		char *t;
		LONG ioerr = 0;
		BPTR file = 0;
#if OSVERMIN <= 36 && OSVERMAX >= 36
		struct FileHandle *fh = 0;
#endif
		BYTE multiline = 0;
		short err = CMD_OK;

		while (*ptr == ' ' || *ptr == '\t')
		    ++ptr;
		for (t = ptr; *t && *t != ' ' && *t != '\t'; t++);
		if (*t) *t++ = '\0';

		if (append)
		{
#if OSVERMIN < 36 && OSVERMAX >= 36
		    if (DOSBase->dl_lib.lib_Version >= 36)
		    {
#endif
#if OSVERMAX >= 36
#if OSVERMIN >= 37
			BPTR lock;

			if (lock = Lock(ptr, ACCESS_WRITE))
			{
			    if (!(file = OpenFromLock(lock)))
			    {
				ioerr = IoErr();
				UnLock(lock);
			    }
			}
#else
			BPTR lock;
			struct FileLock *fl;

			if (lock = Lock(ptr, ACCESS_WRITE))
			{
			    fh = AllocDosObject(DOS_FILEHANDLE, 0);
			    file = MKBADDR(fh);

			    fl = BADDR(lock);
			    fh->fh_Type = fl->fl_Task;

			    if (!(DoPkt2(fl->fl_Task, ACTION_FH_FROM_LOCK, file, lock)))
			    {
				ioerr = IoErr();
				UnLock(lock);
				FreeDosObject(DOS_FILEHANDLE, fh);
				file = 0;
				fh = 0;
			    }
			}
#endif
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
		    }
		    else
		    {
#endif
#if OSVERMIN < 36
			file = Open(ptr, MODE_READWRITE);
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
		    }

		    if (DOSBase->dl_lib.lib_Version >= 36)
		    {
#endif
#if OSVERMAX >= 36
			if (ioerr)
			{
			    if (ioerr == ERROR_ACTION_NOT_KNOWN)
				file = Open(ptr, MODE_OLDFILE);
			    else
				err = CMD_ERROR;
			}
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
		    }
#endif

		    if (!err && !file)
		    {
			if ((ioerr = IoErr()) != ERROR_OBJECT_NOT_FOUND)
			    err = CMD_ERROR;
		    }

		    if (file)
		    {
			Seek(file, 0, OFFSET_END);
			if (ioerr = IoErr())
			    err = CMD_ERROR;
		    }
		}


		if (!err && !file)
		{
		    if (!(file = Open(ptr, MODE_NEWFILE)))
			err = CMD_ERROR;
		}

		if (!err && file)
		{
		    int len;

		    len = strlen(t);

		    if (strnicmp(t, "<<\n", 3) == 0)
		    {
			t += 3;
			len -= 3;
			multiline = 1;
		    }
		    else
		    {
			for(ptr = t; *ptr; ptr++) if (*ptr == ' ') *ptr = '\n';

			t[len] = '\n';
			len++;
		    }

		    Write(file, t, len);
		    if (ioerr = IoErr())
			err = CMD_ERROR;
		}

#if OSVERMIN <= 36 && OSVERMAX >= 36
		if (fh)
		{
		    DoPkt1(fh->fh_Type, ACTION_END, fh->fh_Arg1);
		    FreeDosObject(DOS_FILEHANDLE, fh);
		}
		else
#endif
		if (file)
		    Close(file);

		if (QuietCmd == 0 && multiline)
		    PrintF("<\n");

		{
		    CLI *cli = (CLI *)BADDR(WorkProc->pr_CLI);

		    if (err)
		    {
			PrintF("Unable to write %s\n", ptr);
			cli->cli_Result2 = ioerr;
			*lastret = 20;
		    }
		    else
		    {
			*lastret = 0;
			cli->cli_Result2 = 0;
		    }
#if OSVERMIN < 36 && OSVERMAX >= 36
		    if (DOSBase->dl_lib.lib_Version >= 36)
#endif
#if OSVERMAX >= 36
			SetReturnVar(*lastret, cli->cli_Result2);
#endif
		}
		return((flags&CMDF_IGNORE) ?  CMD_IGNORED : err);
	    } else
	    if (strnicmp(cmd, "failat", 6) == 0)
	    {
		LONG failat;
		short err = CMD_OK;

		while (*ptr == ' ' || *ptr == '\t')
		    ++ptr;

		if (stcd_l(ptr, &failat))
		    Failat = failat;
		else
		{
		    err = CMD_ERROR;
		    PrintF("Missing or invalid argument for failat\n", ptr);
		}

		return((flags&CMDF_IGNORE) ?  CMD_IGNORED : err);
	    }
	} else
	if (cmdlen == 4 && strnicmp(cmd, "quit", 4) == 0) {
	    short err;

	    while (*ptr == ' ' || *ptr == '\t')
		++ptr;
	    if (strnicmp(ptr, "OK", 2) == 0)
		err = CMD_QUIT;
	    else
	    if (strnicmp(ptr, "ERROR", 5) == 0)
		err = CMD_ERROR;
	    else {
		PrintF("Missing or invalid argument for quit\n");
		err = CMD_ERROR;
	    }
	    return((flags&CMDF_IGNORE) ?  CMD_IGNORED : err);
	}
	else
	if (flags&CMDF_MAKETEMP)
	{
	    BPTR fh;
	    STRPTR content = 0;
	    LONG contentsize, usable;
	    LONG keepsize, needsize;
	    const char filename[] = "temp_sdmake.tmp";

	    if (QuietCmd == 0)
		PrintF("<\n");

	    do
	    {
		if (ptr[0] == '<' && ptr[1] == '<' && ptr[2] == '\n')
		{
		    content = ptr + 3;
		    break;
		}

		ptr++;
	    } while (*ptr);

	    contentsize = cmdsize - (content - cmd);
	    usable = contentsize + 3;
	    keepsize = cmdsize - usable;
	    needsize = keepsize + sizeof(filename);

	    if (fh = Open(filename, MODE_NEWFILE)) {
		Write(fh, content, contentsize);
		Close(fh);

		*content = 0;
		strcpy(ptr, filename);
	    }
	    else
	    {
		PrintF("Unable to write %s\n", filename);
		return CMD_ERROR;
	    }

	    if ((usable < sizeof(filename) && flags&CMDF_ALLOCATED) ||
		(!(flags&CMDF_ALLOCATED) && needsize > sizeof(CmdTmp1)))
	    {
		STRPTR tmp;

		if (!(tmp =  PAllocVec(needsize)))
		{
		    PrintF("Memory allocation error\n");
		    return CMD_FAIL;
		}

		stccpy(tmp, cmd, keepsize + 1);

		if (flags&CMDF_ALLOCATED)
		    PFreeVec(*cmdptr);

		*cmdptr = tmp;
		cmd = tmp;
		*cmdflags |= CMDF_ALLOCATED;
	    }

	    strcpy(cmd + keepsize, filename);
	}
    }

    /*
     * run command cmd
     *
     */

    {
	LONG err;
#if OSVERMAX >= 36
	UWORD dosVer = DOSBase->dl_lib.lib_Version;
	char *cmdArgs = 0;
#if OSVERMIN < 37
	struct bufdata fhb = {0};
#endif
	short i = 0;
	short useSystem = 0;
	short c = 0;
	WORD quote = 0;

	if (dosVer >= 36)
	{
	    short ci;

	    {
		WORD quoted;

		if (cmd[0] == '\"')
		{
		    quoted = 1;
		    cmd++;
		}
		else
		    quoted = 0;

		for (i = 0; cmd[i] && cmd[i] != '\t' && cmd[i] != '\n'; ++i)
		{
		    if (!quoted)
		    {
			if (cmd[i] == ' ')
			    break;
		    }
		    else
		    {
			if (cmd[i] == '\"')
			{
			    if (cmd[i + 1] && cmd[i + 1] != ' ' && cmd[i] != '\t' && cmd[i] != '\n')
				break;
			    quote = i;
			    quoted = 0;
			}
		    }

		}

		if (quoted)
		{
		    PrintF("Fail: missing or misplaced '\"' in command list\n");
		    return CMD_FAIL;
		}
	    }

	    if (strpbrk(cmd + i, "<>|`&"))
		useSystem = 1;
	    else
		useSystem = 0;

	    if (quote)
		cmd[quote] = 0;

	    if (c = cmd[ci = i])
		++ci;
	    cmd[i] = 0;

	    {
		char dt[4];

		if (GetVar(cmd, dt, 2, LV_ALIAS | GVF_LOCAL_ONLY) >= 0)
		    useSystem = 1;
	    }
	    
	    fflush(stdout);
	    if (StdOut)
		Flush(StdOut);

	    if (!useSystem)
	    {
		ULONG arglen = strlen(cmd + ci);

#if OSVERMIN < 37 && OSVERMAX >= 37
		if (dosVer >= 37)
		{
#endif
#if OSVERMAX >= 37
		    if (!(cmdArgs = PAllocVec(arglen + 2)))
			MemErr();
#endif
#if OSVERMIN < 37 && OSVERMAX >= 37
		}
		else
		{
#endif
#if OSVERMIN < 37
		    if (!(cmdArgs = AllocVec(max(arglen + 2, 104), MEMF_PUBLIC)))
			MemErr();

		    fhb.fh = BADDR(Input());
		    fhb.oldBuf = fhb.fh->fh_Buf;
		    fhb.oldPos = fhb.fh->fh_Pos;
		    fhb.oldEnd = fhb.fh->fh_End;

		    fhb.fh->fh_Buf = MKBADDR(cmdArgs);
		    fhb.fh->fh_Pos = 0;
		    fhb.fh->fh_End = arglen + 1;
#endif
#if OSVERMIN < 37 && OSVERMAX >= 37
		}
#endif
		sprintf(cmdArgs, "%s\n", cmd + ci);
	    }
	}
#endif

	/*
	 *  NOTE: RunCommand() is unreliable if no pr_CLI exists,
	 *  MUST use system13() in that case.
	 */

#if OSVERMIN < 36 && OSVERMAX >= 36
	if (dosVer >= 36)
#endif
#if OSVERMAX >= 36
	{
	    struct Segment *seg = 0;
	    BPTR seglist;
	    long stack;
	    CLI *cli = (CLI *)BADDR(WorkProc->pr_CLI);
	    static char OldCmd[128];

	    GetProgramName(OldCmd, sizeof(OldCmd));
	    SetProgramName(cmd);
	    stack = cli->cli_DefaultStack * 4;

	    if (useSystem)
		goto dosys;
	    
	    if (!quote)
	    {
		Forbid();
		if (seg = FindSegment(cmd, 0L, DOSFALSE))
		{
		    seg->seg_UC++;
		}
		else if (seg = FindSegment(cmd, 0L, DOSTRUE))
		{
		    if (seg->seg_UC != CMD_INTERNAL)
			seg = 0;
		}
		Permit();
	    }

	    if (seg) {
		dbprintf(("A cmd = '%s' stack = %d\n", cmdArgs, stack));
		err = RunCommand(seg->seg_Seg, stack, cmdArgs, strlen(cmdArgs));
		cli->cli_Result2 = IoErr();
		if (seg->seg_UC > 0)
		    seg->seg_UC--;
	    } else if (seglist = LoadSegPath(cmd)) {
		dbprintf(("B cmd = '%s' stack = %d\n", cmdArgs, stack));
		err = RunCommand(seglist, stack, cmdArgs, strlen(cmdArgs));
		cli->cli_Result2 = IoErr();
		UnLoadSeg(seglist);
	    } else
	    if (IoErr() == ERROR_NO_FREE_STORE)
	    {
		ExitIoErr = ERROR_NO_FREE_STORE;
		SetProgramName(OldCmd);
#if OSVERMIN < 37
		RunCleanup(cmdArgs, &fhb);
#else
		RunCleanup(cmdArgs);
#endif
		return CMD_FAIL;
	    }
	    else
dosys:
	    {
		struct TagItem tags[] = {
		    NP_CopyVars, TRUE,
		    TAG_END, NULL};

		dbprintf(("D\n"));
		cmd[i] = c;
		if (quote)
		{
		    cmd[quote] = '\"';
		    cmd--;
		}
		/*err = system13(cmd);*/
		err = SystemTagList(cmd, tags);
		cli->cli_Result2 = IoErr();
	    }

	    SetProgramName(OldCmd);
	    SetReturnVar(err, cli->cli_Result2);

	    Flush(WorkProc->pr_COS);
	}
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
	else
	{
#endif
#if OSVERMIN < 36
	    dbprintf(("E\n"));
	    err = system13(cmd);
#endif
#if OSVERMIN < 36 && OSVERMAX >= 36
	}
#endif

#if OSVERMAX >= 36
#if OSVERMIN < 37
		RunCleanup(cmdArgs, &fhb);
#else
		RunCleanup(cmdArgs);
#endif
#endif

	{
	    LONG ret = CMD_OK;

	    if (err)
	    {
		if (!QuietCmd && (!QuietOpt || err >= Failat))
		    PrintF("%s: Exit code %ld %s\n", cmd, err, (flags&CMDF_IGNORE) ? "(Ignored)":"");

		if (err == -1)
		    ret = CMD_FAIL;
		else
		if (flags&CMDF_IGNORE)
		    ret = CMD_IGNORED;
		else
		if (err >= Failat)
		    ret = CMD_ERROR;
	    }

	    *lastret = err;

	    return(ret);
	}
    }
}

#if OSVERMAX >= 36
#if OSVERMIN < 37
static void RunCleanup(char *cmdArgs, struct bufdata *fhb)
#else
static void RunCleanup(char *cmdArgs)
#endif
{
#if OSVERMAX >= 36
    if (cmdArgs)
    {
#if OSVERMIN < 37 && OSVERMAX >= 37
	if (DOSBase->dl_lib.lib_Version >= 37)
#endif
#if OSVERMAX >= 37
	    PFreeVec(cmdArgs);
#endif
#if OSVERMIN < 37 && OSVERMAX >= 37
	else
	{
#endif
#if OSVERMIN < 37
	    if(fhb->fh->fh_Buf == MKBADDR(cmdArgs))
	    {
		fhb->fh->fh_Buf = fhb->oldBuf;
		fhb->fh->fh_Pos = fhb->oldPos;
		fhb->fh->fh_End = fhb->oldEnd;
	    }
	    FreeVec(cmdArgs);
#endif
#if OSVERMIN < 37 && OSVERMAX >= 37
	}
#endif
    }
#endif
}


void SetReturnVar(LONG rc, LONG return2)
{
    SetLocalVar("RC", rc);
    SetLocalVar("Result2", return2);
}

static void SetLocalVar(CONST_STRPTR var, LONG value)
{
    TEXT buf[13] = {0};

    stcl_d(buf, value);
    SetVar(var, buf, -1, LV_VAR|GVF_LOCAL_ONLY);
}
#endif
