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
#include <dos/dosextens.h>
#include <dos/var.h>
#include <dos/dostags.h>

typedef struct CommandLineInterface CLI;
typedef struct Process		    Process;

Prototype long Execute_Command(char *cmd, short ignore, short quiet, IfNode **cmdIfBase, LONG *cmdIfTrue, LONG *lastret);
Prototype void InitCommand(void);

static BPTR LoadSegLock(BPTR lock, char *cmd);

BPTR SaveLock;
char RootPath[512];

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

    path = DupLock(((Process *)FindTask(NULL))->pr_CurrentDir);
    SaveLock = CurrentDir(path);
#if OSVERMIN >= 36
    NameFromLock(path, RootPath, sizeof(RootPath));
#else
    getcwd(RootPath, sizeof(RootPath));
#endif

    atexit(ICExit);
}

/*
 *  cmd[-1] is valid space and, in fact, must be long word aligned!
 */

long Execute_Command(char *cmd, short ignore, short quiet, IfNode **cmdIfBase, LONG *cmdIfTrue, LONG *lastret)
{
    register char *ptr;

    for (ptr = cmd; *ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n'; ++ptr)
	;


    /*
     *	Internal MakeDir because AmigaDOS 2.04's MakeDir is unreliable
     *	with RunCommand() (it can crash)
     *
     *	Internal CD, IF, ELSE and ENDIF because we special case it
     */

    {
	LONG cmdlen = ptr - cmd;

	if (cmdlen == 5 && strnicmp(cmd, "endif", 5) == 0) {
	    LONG isif = *cmdIfTrue;

	    *cmdIfTrue = popIf(cmdIfBase);
	    if (quiet == 0 && !isif)
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

	    if (quiet == 0 && !isif)
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
				if (quiet == 0)
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
				if (quiet == 0)
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
				if (quiet == 0)
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
				if (quiet == 0)
				    PrintF(" (Skipped by if-condition)\n");
			    }
			}
		    }
	        }

		if (*cmdIfTrue)
	        {
		    if (notfound)
		    {
			PrintF("Internal command if: Wrong number of arguments\n");
			err = CMD_ERROR;
		    }
	        }

	        return(err);
	    } else
	    if (!(*cmdIfTrue)) {
		if (quiet == 0)
		    PrintF(" (Skipped by if-condition)\n");
		return CMD_OK;
	    } else
	    if (strnicmp(cmd, "cd", 2) == 0) {
	        long lock;
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
	            UnLock(CurrentDir(lock));
	        else {
	            PrintF("Unable to cd %s\n", ptr);
	            err = CMD_ERROR;
	        }
	        return((ignore) ?  CMD_IGNORED : err);
	    }
	} else
	if (!(*cmdIfTrue)) {
	    if (quiet == 0)
		PrintF(" (Skipped by if-condition)\n");
	    return CMD_OK;
	} else
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
	    return((ignore) ?  CMD_IGNORED : err);
	} else
	if (cmdlen == 6) {
	    LONG mode;

	    if ((mode = strnicmp(cmd, "fwrite", 6)) == 0 ||
	    strnicmp(cmd, "fappnd", 6) == 0)
	    {
	        char *t;
	        BPTR fh;
	        short err;

		if (!mode)
		    mode = MODE_NEWFILE;
		else
		    mode = MODE_OLDFILE;

	        while (*ptr == ' ' || *ptr == '\t')
	            ++ptr;
	        for (t = ptr; *t && *t != ' ' && *t != '\t'; t++);
	        if (*t) *t++ = '\0';
		if (fh = Open(ptr, mode)) {
	            int len;

		    if (mode == MODE_OLDFILE)
			Seek(fh, 0, OFFSET_END);

	            len = strlen(t);

		    if (strnicmp(t, "<<\n", 3) == 0)
		    {
		        t += 3;
		        len -= 3;
		    }
		    else
		        for(ptr = t; *ptr; ptr++) if (*ptr == ' ') *ptr = '\n';

	            t[len] = '\n';
	            Write(fh, t, len+1);
	            t[len] = '\0';

	            Close(fh);
	            err = CMD_OK;
	        }
	        else
	        {
	            PrintF("Unable to write %s\n", ptr);
	            err = CMD_ERROR;
	        }
		return((ignore) ?  CMD_IGNORED : err);
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

		return((ignore) ?  CMD_IGNORED : err);
	    }
	}
    }

    /*
     * run command cmd
     *
     */

    {
	short i;
	short ci;
	short c;
	short useSystem;
	LONG err;
	char *cmdArgs;
	Process *proc = (Process *)FindTask(NULL);

	for (i = 0; cmd[i] && cmd[i] != ' ' && cmd[i] != 9 && cmd[i] != 10 && cmd[i] != 13; ++i)
	    ;
	if (strpbrk(cmd + i, "<>|`"))
	    useSystem = 1;
	else
	    useSystem = 0;

	if (c = cmd[ci = i])
	    ++ci;
	cmd[i] = 0;

	if (!(cmdArgs = PAllocVec(strlen(cmd + ci) + 3)))
	    MemErr();
	sprintf(cmdArgs, "%s\n", cmd + ci);
	fflush(stdout);

	/*
	 *  NOTE: RunCommand() is unreliable if no pr_CLI exists,
	 *  MUST use system13() in that case.
	 */

	if (SysBase->LibNode.lib_Version >= 36 && proc->pr_CLI) {
	    struct Segment *seg;
	    BPTR seglist, lock = 0;
	    long stack;
	    CLI *cli = (CLI *)BADDR(proc->pr_CLI);
	    static char OldCmd[128];
	    char dt[4];
	    struct TagItem tags[] = {
		NP_CopyVars, TRUE,
		TAG_END, NULL};

	    if (cli) {
		GetProgramName(OldCmd, sizeof(OldCmd));
		SetProgramName(cmd);
		stack = cli->cli_DefaultStack * 4;
	    } else {
		stack = 8192;
	    }

	    /*
	     *	note: Running2_04() means V37 or greater
	     */

	    if (useSystem || (Running2_04() && GetVar(cmd, dt, 2, LV_ALIAS | GVF_LOCAL_ONLY) >= 0))
		goto dosys;

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

	    if (seg) {
		dbprintf(("A cmd = '%s' stack = %d\n", cmdArgs, stack));
		err = RunCommand(seg->seg_Seg, stack, cmdArgs, strlen(cmdArgs));
		if (seg->seg_UC > 0)
		    seg->seg_UC--;
	    } else if ((lock = _SearchPath(cmd)) && (seglist = LoadSegLock(lock, ""))) {
		dbprintf(("B cmd = '%s' stack = %d\n", cmdArgs, stack));
		err = RunCommand(seglist, stack, cmdArgs, strlen(cmdArgs));
		UnLoadSeg(seglist);
	    } else {
dosys:
		dbprintf(("D\n"));
		cmd[i] = c;
		/*err = system13(cmd);*/
		err = SystemTagList(cmd, tags);
	    }

	    if (cli)
		SetProgramName(OldCmd);

	    if (lock)
		UnLock(lock);
	} else
	{
	    dbprintf(("E\n"));
	    cmd[i] = c;
	    err = system13(cmd);
	}

	PFreeVec(cmdArgs);

	{
	    LONG ret = CMD_OK;

	    if (err)
	    {
		if (!QuietCmd && (!QuietOpt || err >= Failat))
		    PrintF("%s: Exit code %ld %s\n", cmd, err, (ignore) ? "(Ignored)":"");

		if (err == -1)
		    ret = CMD_FAIL;
		else
	        if (ignore)
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


static BPTR LoadSegLock(BPTR lock, char *cmd)
{
    BPTR oldLock;
    BPTR seg;

    oldLock = CurrentDir(lock);
    seg = LoadSeg(cmd);
    CurrentDir(oldLock);
    return(seg);
}
