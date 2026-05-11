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

#define VAR_NUM 1
#define VAR_VAR -1

Prototype WORD ParseRevInclude(STRPTR includefile);

static void GetVerRev(STRPTR define);

WORD ParseRevInclude(STRPTR includefile)
{
	struct AsyncFile *revfile;

	if (revfile = OpenAsyncR(includefile))
	{
	    TEXT line[64];

	    while (ReadLineAsync(revfile, line, 64) > 0)
	    {
		if (!(strncmp(line, "#define", 7 )))
		    GetVerRev(line + 8);
	    }


	    CloseAsyncR(revfile);
	}
	else
	    error(FATAL, "Unable to open %s", revfile);

    return 1;
}

static void GetVerRev(STRPTR define)
{
    STRPTR varname;
    WORD verch, revch;

    verch = revch = 0;
    
    while (*define && (*define == ' ' || *define == '\t'))
	define++;

    if (!define[0] || *define == '\n' ||
	!((*define >= 'A' && *define <= 'Z') || (*define >= 'a' && *define <= 'z')))
	return;

    varname = define;

    while (*define && *define != ' ' && *define != '\n' && *define != '\t')
    {
	if (verch < 3 && revch < 3)
	{
	    if (*define == 'R')
	    {
		if (verch == 2)
		    verch++;
		else
		if (!revch)
		    revch++;
		else
		    verch = revch = 0;
	    }
	    else
	    if (*define == 'E')
	    {
		if (verch == 1)
		    verch++;
		else
		if (revch == 1)
		    revch++;
		else
		    verch = revch = 0;
	    }
	    else
	    if (*define == 'V')
	    {
		if (!verch)
		    verch++;
		else
		if (revch == 2)
		    revch++;
		else
		    verch = revch = 0;
	    }
	}

	define++;
    }

    if (!define[0] || *define == '\n' || (!verch && !revch))
	return;

    *define = 0;

    while (*++define && (*define == ' ' || *define == '\t'));

    if (!define[0] || *define == '\n')
	return;

    {
	LONG len = strlen(define);
	Var *var;
	WORD type = 0;

	PrintF("Define before: \"%s\"\n", define);
	
	if (*define >= '0' && *define <= '9')
	    type = VAR_NUM;
	else
	if ((*define >= 'A' && *define <= 'Z') || (*define >= 'a' && *define <= 'z'))
	    type = VAR_VAR;
	else
	    return;

	while (define[len - 1] == '\n' || define[len - 1] == ' ' || define[len - 1] == '\t')
	    define[--len] = 0;
	PrintF("Define After: \"%s\"\n", define);
	
	if (type == VAR_NUM)
	{
	    PrintF("Making var \"%s\" value \"%s\"\n", varname, define);
	    var = MakeVariable(varname, '$');
	    AppendVariable(var, define, len);
	}
	else
	{
	    Var *defvar;
	    List tmplist;
	    TEXT tmpbuf[32];

	    NewList(&tmplist);

	    PrintF("Finding var \"%s\"\n", define);
	    if (defvar = FindVariable(define, '$'))
	    {
		PrintF("Found var \"%s\", Making var: \"%s\"\n", define, varname);
		var = MakeVariable(varname, '$');
		if (CmdListSize(&defvar->var_CmdList) < sizeof(tmpbuf))
	        {
		    CopyCmdList(&defvar->var_CmdList, &tmplist);
		    CopyCmdListBuf(&tmplist, tmpbuf);
		    AppendVariable(var, tmpbuf, strlen(tmpbuf));
	        }
	        else
	        {
		    error(FATAL, ".revheader: Overflow, define \"%s\" too long!", define);
	        }
	    }
	}
    }
}
