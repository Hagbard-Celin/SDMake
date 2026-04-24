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
 *  VAR.C
 */

#include "defs.h"
#include <dos/dos.h>

Prototype void InitVariable(void);
Prototype Var *MakeVariable(char *, char);
Prototype Var *FindVariable(char *, char);
Prototype void AppendVariable(Var *, char *, long);

List VarList;

void InitVariable(void)
{
    NewList(&VarList);
}

/*
 *  create a variable, deleting any previous contents
 */

Var *MakeVariable(char *name, char type)
{
    Var *var;

    for (var = (Var *)GetHead(&VarList); var; var = (Var *)GetSucc(&var->var_Node)) {
	if ((char)var->var_Node.ln_Type == type && strcmp(var->var_Node.ln_Name, name) == 0) {
	    while (PopCmdListChar(&var->var_CmdList) != EOF)
		;
	    return(var);
	}
    }
    if (!(var = PAlloc(sizeof(Var) + strlen(name) + 1)))
	MemErr();
    clrmem(var, sizeof(Var));

    var->var_Node.ln_Name = (char *)(var + 1);
    var->var_Node.ln_Type = type;
    strcpy(var->var_Node.ln_Name, name);
    NewList(&var->var_CmdList);
    AddTail(&VarList, &var->var_Node);
    return(var);
}

Var *FindVariable(char *name, char type)
{
    Var *var;

    for (var = (Var *)GetHead(&VarList); var; var = (Var *)GetSucc(&var->var_Node)) {
	if ((char)var->var_Node.ln_Type == type && strcmp(var->var_Node.ln_Name, name) == 0)
	    break;
    }

    /*
     *	check for local & env variable(s).  local variables under 2.04
     *	or later only.
     */

    if (var == NULL || var->var_Node.ln_Type == '0')
    {
	if (Running2_04())
	{
	    char *ptr;
	    long len;

	    if (GetVar(name, (char *)&ptr, 2, 0) >= 0)
	    {
		len = IoErr();
		if (!(ptr = PAlloc(len + 1)))
		    MemErr();
		if (GetVar(name, ptr, len + 1, 0) >= 0)
		{
		    var = MakeVariable(name, '0');
		    AppendVariable(var, ptr, strlen(ptr));
		}
		PFree(ptr, len + 1);
	    }
	}
	else
	{
	    BPTR lock, old, fh;
	    long size;

	    if (lock = Lock("ENV:", SHARED_LOCK))
	    {
		old = CurrentDir(lock);

		if (fh = Open(name, MODE_OLDFILE))
		{
		    Seek(fh, 0L, 1);
		    if ((size = Seek(fh, 0L, -1)) >= 0)
		    {
			char *ptr;

			if (!(ptr = PAlloc(size + 1)))
			    MemErr();
			Read(fh, ptr, size);
			ptr[size] = 0;

			var = MakeVariable(name, '0');
			AppendVariable(var, ptr, strlen(ptr));
			PFree(ptr, size + 1);
		    }
		    Close(fh);
		}

		UnLock(CurrentDir(old));
	    }
	}
    }
    return(var);
}


void AppendVariable(Var *var, char *buf, long len)
{
    while (len--)
	PutCmdListChar(&var->var_CmdList, *buf++);
}

