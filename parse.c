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
 *  PARSE.C
 *
 *  Parse the next token or tokens
 */

#include "defs.h"
#include <stdarg.h>

Prototype void InitParser(void);
Prototype void ParseFile(STRPTR);
Prototype token_t ParseAssignment(STRPTR varName, token_t t, int cond, char type);
Prototype token_t ParseDependency(STRPTR firstSym, token_t t, UWORD lefttype);
Prototype token_t GetElement(int ifTrue, int *expansion);
Prototype token_t XGetElement(void);
Prototype void	  ParseVariable(List *, short);
Prototype void ParseVariableList(List *srcList, List *dstList, short c0);
Prototype STRPTR ParseVariableBuf(List *, STRPTR, short);
Prototype void ExpandVariableList(List *srclist, List *list);
Prototype STRPTR ExpandVariable(STRPTR, List *);
Prototype token_t GetToken(void);
Prototype void expect(token_t, token_t);
Prototype void error(short type, CONST_STRPTR ctl, ...);


Prototype char SymBuf[256];
Prototype long LineNo;

extern short	QuietOpt;
extern STRPTR	OnError;

char SpecialChar[256];
char SChars[] = { ":=()\n\\\" \t\r\014" };
char SymBuf[256];
char AltBuf[256];
char AltBuf2[256];
long LineNo = 1;
char *FileName = "";
FILE *Fi;

void InitParser()
{
    short i;
    for (i = 0; SChars[i]; ++i)
	SpecialChar[(ubyte)SChars[i]] = 1;
}

/*
 *  Parse lines as follows:
 *
 *  symbol = ...
 *  symbol ... : symbol ...
 *	(commands, begin with tab or space)
 *	(blank line)
 *
 *
 */

void ParseFile(STRPTR fileName)
{
    FILE *fi;
    token_t t;
    IfNode *ifBase = NULL;
    int ifTrue = 1;
    int expansion;
    List topdirList;
    UWORD lefttype = 0;

    NewList(&topdirList);

    /*
     * Open the file
     */
    {
	Var *var = FindVariable("TOPDIR", '$');
	char *tfileName;
	int len;
	List list;

	NewList(&list);
	CopyCmdList(&var->var_CmdList, &list);
	CopyCmdList(&var->var_CmdList, &topdirList);
	len = CmdListSize(&list);
	tfileName = malloc(len + strlen(fileName) + 1);
	CopyCmdListBuf(&list, tfileName);
	strcpy(tfileName + len, fileName);

	if ((fi = fopen(tfileName, "r")) == NULL)
	    error(FATAL, "Unable to open %s", tfileName);
	free(tfileName);
    }
    FileName = strdup(fileName);
    Fi = fi;
    LineNo = 1;

    /*
     * Adjust TOPDIR based on include path
     */
    {
	const char *p;

	if (QuietOpt == 0)
	    printf("fileName: %s\n", fileName);
	if ((p = strrchr(fileName, '/')) != NULL) {
		Var *var = FindVariable("TOPDIR", '$');
		AppendVariable(var, fileName, p - fileName + 1);
	}
    }

    /*
     * Parse the file
     */
    for (t = GetElement(ifTrue, &expansion); t; ) {
	switch(t) {
	case TokNewLine:
	    t = GetElement(ifTrue, &expansion);
	    break;
	case TokSym:
	    strcpy(AltBuf2, SymBuf);

	    if (expansion == 0 && SymBuf[0] == '.')
	    {
		if (ifTrue && strcmp(SymBuf, ".export") == 0) {
		    char *data;
		    Var *var;

		    while ((t = GetElement(ifTrue, &expansion)) != TokNewLine) {
			int maxl;

			if (t != TokSym)
			    error(FATAL, "Expected a symbol for .export!");

			if ((var = FindVariable(SymBuf, '$')) == NULL) {
			    error(
				FATAL,
				"export %s failed, variable not found",
				SymBuf
			    );
			}
			{
			    static List CmdList = {
				(Node *)&CmdList.lh_Tail,
				NULL,
				(Node *)&CmdList.lh_Head
			    };
			    CopyCmdList(&var->var_CmdList, &CmdList);
			    maxl = CmdListSize(&CmdList) + 1;
			    data = malloc(maxl);
			    CopyCmdListBuf(&CmdList, data);
			    if (Running2_04())
			    {
				SetVar(SymBuf, data, -1, GVF_GLOBAL_ONLY);
			    }
			    else
			    {
				BPTR dir, file, old;
				if (dir = Lock("ENV:", SHARED_LOCK))
				{
				    old = CurrentDir(dir);
				    if (file = Open(SymBuf, MODE_NEWFILE))
				    {
					Write(file, data, strlen(data) + 1);
					Close(file);
				    }
				    UnLock(CurrentDir(old));
				}
			    }
			    free(data);
			}
		    }
		} else if (ifTrue && strcmp(SymBuf, ".include") == 0) {
		    FILE *saveFi = Fi;
		    char *saveFileName = FileName;
		    int saveLine = LineNo;
		    char *path;

		    t = GetElement(ifTrue, &expansion);
		    if (t != TokSym)
			error(FATAL, "Expected a symbol for .include!");
		    path = strdup(SymBuf);
		    ParseFile(path);
		    free(path);

		    LineNo = saveLine;
		    Fi = saveFi;
		    FileName = saveFileName;

		    t = GetElement(ifTrue, &expansion);
		    if (t != TokNewLine)
			error(FATAL, "Expected newline after .include filename");
		} else if (ifTrue && strcmp(SymBuf, ".revheader") == 0) {
		    t = GetElement(ifTrue, &expansion);
		    if (t != TokSym)
			error(FATAL, "Expected a symbol for .revhheader!");
		    ParseRevInclude(SymBuf);
		    t = GetElement(ifTrue, &expansion);
		    if (t != TokNewLine)
			error(FATAL, "Expected newline after .revheader filename");
		} else if (strcmp(SymBuf, ".else") == 0) {
		    if (ifBase == NULL)
			error(FATAL, ".else without .if*");
		    ifTrue = elseIf(&ifBase);
		} else if (strcmp(SymBuf, ".ifeq") == 0) {
		    if (ifTrue) {
			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a symbol for .ifeq!");
			strcpy(AltBuf2, SymBuf);
			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a second symbol for .ifeq!");
			if (stricmp(SymBuf, AltBuf2) == 0)
			    ifTrue = pushIf(&ifBase, 1);
			else
			    ifTrue = pushIf(&ifBase, 0);
			t = GetElement(ifTrue, &expansion);
		        if (t != TokNewLine)
			    error(FATAL, "Expected newline after .ifeq second argument");
		    } else {
			ifTrue = pushIf(&ifBase, 0);
		    }
		} else if (strcmp(SymBuf, ".ifgt") == 0) {
		    if (ifTrue) {
			LONG firstvalue;

			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a symbol for .ifgt!");
			firstvalue = atol(SymBuf);
			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a second symbol for .ifgt!");
			if (firstvalue > atol(SymBuf))
			    ifTrue = pushIf(&ifBase, 1);
			else
			    ifTrue = pushIf(&ifBase, 0);
			t = GetElement(ifTrue, &expansion);
		        if (t != TokNewLine)
			    error(FATAL, "Expected newline after .ifgt second argument");
		    } else {
			ifTrue = pushIf(&ifBase, 0);
		    }
		} else if (strcmp(SymBuf, ".ifin") == 0) {
		    if (ifTrue) {
			STRPTR found = 0;
			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a symbol for .ifin!");
			strcpy(AltBuf2, SymBuf);
			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a second symbol for .ifin!");
			do
			{
			    if (!found)
				found = strstr(SymBuf, AltBuf2);
			} while ((t = GetElement(ifTrue, &expansion)) == TokSym);
			if (found)
			    ifTrue = pushIf(&ifBase, 1);
			else
			    ifTrue = pushIf(&ifBase, 0);
			if (t != TokNewLine)
			    error(FATAL, "Expected newline after .ifin last argument");
		    } else {
			ifTrue = pushIf(&ifBase, 0);
		    }
		} else if (strcmp(SymBuf, ".ifdef") == 0) {
		    if (ifTrue) {
			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a symbol for .ifdef!");
			if (FindVariable(SymBuf, '$')) {
			    ifTrue = pushIf(&ifBase, 1);
			} else {
			    ifTrue = pushIf(&ifBase, 0);
			}
			t = GetElement(ifTrue, &expansion);
		        if (t != TokNewLine)
			    error(FATAL, "Expected newline after .ifdef variable-name");
		    } else {
			ifTrue = pushIf(&ifBase, 0);
		    }
		} else if (strcmp(SymBuf, ".iffile") == 0) {
		    if (ifTrue) {
			BPTR tmplock;

			t = GetElement(ifTrue, &expansion);
			if (t != TokSym)
			    error(FATAL, "Expected a symbol for .iffile!");

			if (tmplock = Lock(SymBuf, ACCESS_READ))
			{
			    UnLock(tmplock);
			    ifTrue = pushIf(&ifBase, 1);
			} else {
			    ifTrue = pushIf(&ifBase, 0);
			}
			t = GetElement(ifTrue, &expansion);
		        if (t != TokNewLine)
			    error(FATAL, "Expected newline after .iffile filename");
		    } else {
			ifTrue = pushIf(&ifBase, 0);
		    }
		} else if (strcmp(SymBuf, ".endif") == 0) {
		    if (ifBase == NULL)
			error(FATAL, ".endif without .if");
		    ifTrue = popIf(&ifBase);
		} else if (ifTrue && strcmp(SymBuf, ".onerror") == 0) {
		    t = GetElement(ifTrue, &expansion);
		    if (t != TokSym)
			error(FATAL, "Expected a symbol for .onerror!");
		    if (OnError)
			free(OnError);
		    OnError = strdup(SymBuf);
		} else if (ifTrue && strcmp(SymBuf, ".leftislabel") == 0) {
		    printf("Setting left dummy\n");
		    lefttype &= ~LT_MASK;
		    lefttype |= LT_DUMMY;
		} else if (ifTrue && strcmp(SymBuf, ".leftisgroup") == 0) {
		    printf("Setting left group\n");
		    lefttype &= ~LT_MASK;
		    lefttype |= LT_GROUP;
		} else if (ifTrue && strcmp(SymBuf, ".leftisfile") == 0) {
		    printf("Setting left file\n");
		    lefttype &= ~LT_MASK;
		    lefttype |= LT_FILE;
		} else if (ifTrue) {
		    error(FATAL, "unknown '.' directive");
		}

		/*
		 * End of special directive handling
		 */
		while (t && t != TokNewLine)
		    t = GetElement(ifTrue, &expansion);
		continue;
	    }

	    /*
	     * Ignore .if'd out code
	     */
	    if (ifTrue == 0) {
		while (t && t != TokNewLine)
		    t = GetElement(ifTrue, &expansion);
		continue;
	    }

	    /*
	     *	check for '=' -- assignment
	     */
	    t = GetElement(ifTrue, &expansion);
	    if (t == TokQuestion) {
		t = GetElement(ifTrue, &expansion);
		if (t == TokEq)
		    t = ParseAssignment(AltBuf2, t, 1, '$');
		else
		    error(FATAL, "Expected '?=' got '?'");
	    } else if (t == TokEq) {
		t = ParseAssignment(AltBuf2, t, 0, '$');
	    } else {
		t = ParseDependency(AltBuf2, t, lefttype);
	    }
	    break;
	case TokColon:
	    /*
	     * Ignore .if'd out code
	     */
	    if (ifTrue == 0) {
		while (t && t != TokNewLine)
		    t = GetElement(ifTrue, &expansion);
		continue;
	    }
	    t = ParseDependency(NULL, t, lefttype);
	    break;
	default:
	    /*
	     * Ignore .if'd out code
	     */
	    if (ifTrue == 0) {
		while (t && t != TokNewLine)
		    t = GetElement(ifTrue, &expansion);
		continue;
	    }
	    error(FATAL, "Expected a symbol!");
	    break;
	}
    }
    if (ifBase != NULL)
	error(FATAL, "Dangling .if's at EOF");

    /*
     * Restore TOPDIR
     */
    {
	Var *var = FindVariable("TOPDIR", '$');
	FreeCmdList(&var->var_CmdList);
	AppendCmdList(&topdirList, &var->var_CmdList);
    }
}

/*
 *  Parse an asignment.  Parsed as-is (late eval)
 *
 *  t contains TokEq, ignore
 */

token_t ParseAssignment(STRPTR varName, token_t t, int cond, char type)
{
    Var *var;
    int newVar = 0;
    long len;
    short done;
    short eol = 1;
    List tmpList;

    if (cond == 0 || FindVariable(varName, type) == NULL) {
	newVar = 1;
    }

    NewList(&tmpList);

    while (fgets(AltBuf, sizeof(AltBuf), Fi)) {
	len = strlen(AltBuf);

	if (eol && AltBuf[0] == '#') {
	    ++LineNo;
	    continue;
	}
	if (len && AltBuf[len-1] == '\n') {
	    ++LineNo;
	    --len;
	    if (len && AltBuf[len-1] == '\\') {
		--len;
		done = 0;
	    } 
	    else {
		done = 1;
	    }
	    eol = 1;
	} 
	else {
	    done = 0;
	    eol = 0;
	}

	AltBuf[len] = 0;
	{
	    long i;

	    for (i = 0; i < len && (AltBuf[i] == ' ' || AltBuf[i] == '\t'); ++i)
		;
	    /*
	     * allow one whitespace in
	     */
	    if (i && ((AltBuf[i-1] == '\t') || (AltBuf[i] == ' ')))
		--i;
	    for (; i < len; ++i)
		PutCmdListChar(&tmpList, AltBuf[i]);

	}
	if (done > 0)
	    break;
    }

    /*
     *	Now, load temp list into buffer and expand into the variable
     */

    {
	char *buf = malloc(CmdListSize(&tmpList) + 1);
	CopyCmdListBuf(&tmpList, buf);
	if (newVar) {
	    ExpandVariable(buf, &tmpList);
	    var = MakeVariable(varName, type);
	    AppendCmdList(&tmpList, &var->var_CmdList);
	}
	free(buf);
    }
    return(GetElement(1, NULL));
}

/*
 *  Parse a dependency
 */

token_t ParseDependency(STRPTR firstSym, token_t t, UWORD lefttype)
{
    DepRef  *lhs;
    DepRef  *rhs;
    List    lhsList;
    List    rhsList;
    List    *cmdList = malloc(sizeof(List));
    long    nlhs = 0;
    long    nrhs = 0;
    short   ncol = 0;

    NewList(cmdList);
    NewList(&lhsList);
    NewList(&rhsList);

    if (firstSym) {
	++nlhs;
	lhs = CreateDepRef(&lhsList, firstSym);
	if (lefttype == LT_DUMMY)
	    lhs->rn_Dep->dn_Flags |= DNF_LEFT_VIRTUAL;
	else if (lefttype == LT_GROUP)
	    lhs->rn_Dep->dn_Flags |= DNF_LEFT_GROUP;
    }

    while (t != TokColon) {
	expect(t, TokSym);
	lhs = CreateDepRef(&lhsList, SymBuf);
	if (lefttype == LT_DUMMY)
	    lhs->rn_Dep->dn_Flags |= DNF_LEFT_VIRTUAL;
	else if (lefttype == LT_GROUP)
	    lhs->rn_Dep->dn_Flags |= DNF_LEFT_GROUP;
	++nlhs;
	t = GetElement(1, NULL);
    }
    t = GetElement(1, NULL);
    if (t == TokColon) {
	++ncol;
	t = GetElement(1, NULL);
    }

    while (t != TokNewLine) {
	expect(t, TokSym);
	CreateDepRef(&rhsList, SymBuf);
	++nrhs;
	t = GetElement(1, NULL);
    }

    /*
     *	parse command list
     */

    {
	short c;
	short blankLine = 1;
	short ws = 0;		/*  white space skip	*/
	WORD twolt = 0;
	WORD threelt = 0;
	WORD newline = 0;

	while ((c = getc(Fi)) != EOF) {
	    if (twolt < 2)
	    {
		if (c == '<')
		{
		    twolt++;
		    PutCmdListChar(cmdList, c);
		}
		else
		{
		    twolt = 0;

		    if (c == '\n') {
			++LineNo;
			if (blankLine)
			    break;
			PutCmdListChar(cmdList, '\n');
			blankLine = 1;
			ws = 0;
			continue;
		    }
		    if (blankLine && ws == 0) {
			if (c != ' ' && c != '\t')
			{
			    ungetc(c, Fi);
			    break;
			}
		    }

		    switch(c) {
		    case ' ':
		    case '\t':
			if (blankLine) {    /*	remove all but one ws after nl */
			    ws = 1;
			    continue;
			}
			PutCmdListChar(cmdList, c);
			break;
		    case '\\':
			if (ws) {
			    PutCmdListChar(cmdList, ' ');
			    ws = 0;
			}
			c = getc(Fi);
			if (c == '\n') {
			    blankLine = 1;
			    ++LineNo;
			    continue;
			}
			PutCmdListChar(cmdList, '\\');
			PutCmdListChar(cmdList, c);
			break;
		    default:
			if (ws) {
			    PutCmdListChar(cmdList, ' ');
			    ws = 0;
			}
			PutCmdListChar(cmdList, c);
			break;
		    }
		}
	        blankLine = 0;
	    }
	    else
	    {
		if (newline)
		{
		    if (threelt < 3)
		    {
			if (c == '<')
			    ++threelt;
			else
			{
			    threelt = 0;
			    if (c == '#')
			    {
				while ((c = getc(Fi)) != EOF) {
				    if (c == '\n') {
					++LineNo;
					c = getc(Fi);
					if (c == '#')
					    continue;
					break;
				    }
				}

			    }
			    if (c != '\n')
				newline = 0;
			}
		    }
		    else
		    {
			if (c == '\n')
			{
			    twolt = 0;
			    ++LineNo;
			}
			newline = threelt = 0;
		    }
		}

		if (c == '\n')
		{
		    if (twolt)
			newline = 1;
		    else
		    {
			blankLine = 1;
			ws = 0;
		    }

		    ++LineNo;
		}
		PutCmdListChar(cmdList, c);
	    }
	}
    }
    dbprintf(("parse: %d : %d\n", nlhs, nrhs));

    /*
     *	formats allowed:
     *
     *	    X :: Y	each item depends on all items (X x Y dependancies)
     *	    1 : N	item depends on items
     *	    N : N	1:1 map item to item
     *	    N : 1	items depend on item
     */

    if (ncol == 1) {
	while ((lhs = (DepRef *)RemHead(&lhsList)) != NULL) {
	    if (GetHead(&lhsList)) {
		for (rhs = (DepRef *)GetHead(&rhsList); rhs; rhs = (DepRef *)GetSucc(&rhs->rn_Node))
		    IncorporateDependency(lhs, DupDepRef(rhs), cmdList);
	    } else {
		while ((rhs = (DepRef *)RemHead(&rhsList)) != NULL)
		    IncorporateDependency(lhs, rhs, cmdList);
	    }
	    IncorporateDependency(lhs, NULL, cmdList);
	    free(lhs);
	}
    } else if (nlhs == 1) {
	lhs = (DepRef *)RemHead(&lhsList);
	while ((rhs = (DepRef *)RemHead(&rhsList)) != NULL)
	    IncorporateDependency(lhs, rhs, cmdList);
	IncorporateDependency(lhs, NULL, cmdList);
	free(lhs);
    } else if (nrhs == 1) {
	rhs = (DepRef *)RemHead(&rhsList);
	while ((lhs = (DepRef *)RemHead(&lhsList)) != NULL) {
	    IncorporateDependency(lhs, rhs, cmdList);
	    free(lhs);
	}
    } else if (nlhs == nrhs) {
	while ((lhs = (DepRef *)RemHead(&lhsList)) && (rhs = (DepRef *)RemHead(&rhsList))) {
	    IncorporateDependency(lhs, rhs, cmdList);
	    free(lhs);
	}
    } else {
	error(FATAL, "%d items on the left, %d on the right of colon!", nlhs, nrhs);
    }
    return(t);
}

/*
 *  GetElement()    - return a token after variable/replace parsing
 */

token_t GetElement(int ifTrue, int *expansion)
{
    static List CmdList = { (Node *)&CmdList.lh_Tail, NULL, (Node *)&CmdList.lh_Head };
    token_t t;
    short c;

top:
    if (PopCmdListSym(&CmdList, SymBuf, sizeof(SymBuf)) == 0) {
	if (expansion)
	    *expansion = 1;
	return(TokSym);
    }
    if (expansion)
	*expansion = 0;

    t = GetToken();
swi:
    switch(t) {
    case TokSym:
	{
	    WORD pos = strlen(SymBuf) - 1;

	    if (SymBuf[pos] == '$')
	    {
	        LONG i;

	        SymBuf[pos] = 0;

	        for (i = 0; i < pos; i++)
		    PutCmdListChar(&CmdList, SymBuf[i]);

	        t = TokDollar;
	        goto swi;
	    }
	}
    case TokDollar:
    case TokPercent:
	c = fgetc(Fi);
	if (c == '(' && ifTrue) {
	    ParseVariable(&CmdList, (t == TokPercent) ? '%' : '$');

	    /*
	     *	XXX how to handle dependancies verses nominal string concat?
	     */

	    while ((c = fgetc(Fi)) != ' ' && c != '\t' && c != '\n' && c != ':') {
		if (c == EOF)
		    break;
		if (c == '$') {
		    t = TokDollar;
		    goto swi;
		}
		if (c == '%') {
		    t = TokPercent;
		    goto swi;
		}
		PutCmdListChar(&CmdList, c);
	    }
	    if (c != EOF)
		ungetc(c, Fi);
	    goto top;
	}
	ungetc(c, Fi);
	/* fall through */
    default:
	break;
    }
    return(t);
}

/*
 *  ParseVariable() - parse a variable reference, expanding it into a
 *  command list.  Fi begins at the first character in the variable name
 *
 *  $(NAME)
 *  $(NAME:"from":"to")
 *
 */

void ParseVariable(List *cmdList, short c0)
{
    short c;
    short i = 0;
    Var *var;

    /*
     *	variable name
     */

    while ((c = getc(Fi)) != EOF && !SpecialChar[c])
	AltBuf[i++] = c;
    AltBuf[i] = 0;

    var = FindVariable(AltBuf, c0);
    if (var == NULL)
	error(FATAL, "Variable %s does not exist", AltBuf);

    dbprintf(("ParseVariable: (%c:%c) %s\n", c0, c, AltBuf));

    /*
     *	now, handle modifiers
     */

    if (c == ')') {
	CopyCmdList(&var->var_CmdList, cmdList);
	return;
    }
    if (c != ':')
	error(FATAL, "Bad variable specification after name");

    /*
     *	source operation
     */

    c = fgetc(Fi);
    if (c == '\"') {
	ungetc(c, Fi);
	expect(GetToken(), TokStr);
	c = fgetc(Fi);
    } else {
	i = 0;
	while (c != ')' && c != ':' && c != EOF) {
	    SymBuf[i++] = c;
	    c = fgetc(Fi);
	}
	SymBuf[i] = 0;
    }

    strcpy(AltBuf, SymBuf);

    /*
     *	destination operation
     */

    if (c == ')') {
	CopyCmdListConvert(&var->var_CmdList, cmdList, AltBuf, AltBuf);
	return;
    }

    if (c != ':')
	error(FATAL, "Bad variable replacement spec: %c", c);

    c = fgetc(Fi);
    if (c == '\"') {
	ungetc(c, Fi);
	expect(GetToken(), TokStr);
	c = fgetc(Fi);
    } else {
	i = 0;
	while (c != ')' && c != ':' && c != EOF) {
	    SymBuf[i++] = c;
	    c = fgetc(Fi);
	}
	SymBuf[i] = 0;
    }

    if (c != ')')
	error(FATAL, "Bad variable replacement spec: %c", c);

    CopyCmdListConvert(&var->var_CmdList, cmdList, AltBuf, SymBuf);
}

/*
 *  Since this is recursively called we have to save/restore our temporary
 *  bufferse (SymBuf & AltBuf).  the buf pointer may itself be pointing
 *  into these but we are ok since it is guarenteed >= our copy destination
 *  as we index through it.
 */


void ParseVariableList(List *srcList, List *dstList, short c0)
{
    short c;
    short i = 0;
    Var *var;
    char *symBuf = AllocPathBuffer();
    char *altBuf = AllocPathBuffer();

    /*
     *	variable name
     */

    while ((c = PopCmdListChar(srcList)) != EOF && !SpecialChar[c])
    {
	altBuf[i++] = c;
	if (i >= PBUFSIZE)
	    error(FATAL, "Symbol overflow: %s", altBuf);
    }
    altBuf[i] = 0;
    dbprintf(("%s finding var altBuf: %s\n", __FUNC__, altBuf));

    var = FindVariable(altBuf, c0);
    if (var == NULL)
	error(FATAL, "Variable %s does not exist", altBuf);

    /*
     *	now, handle modifiers
     */

    if (c == ')') {
	CopyCmdList(&var->var_CmdList, dstList);
	FreePathBuffer(symBuf);
	FreePathBuffer(altBuf);
	return;
    }
    if (c != ':')
	error(FATAL, "Bad variable specification after name %x", c);

    /*
     *	source operation
     */

    c = PopCmdListChar(srcList);

    if (c == '\"') {
	i = 0;
	while ((c = PopCmdListChar(srcList)) && c != '\"' && c != EOF)
	{
	    symBuf[i++] = c;
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
	if (c == '\"')
	    c = PopCmdListChar(srcList);
    } else {
	i = 0;
	while (c && c != ')' && c != ':' && c != EOF) {
	    symBuf[i++] = c;
	    c = PopCmdListChar(srcList);
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
    }

    symBuf[i] = 0;
    strcpy(altBuf, symBuf);

    /*
     *	destination operation
     */

    if (c == ')') {
	dbprintf(("File: %s Line: %ld Func: %s CopyConvert to %s %s (%s) %08lx\n", __FILE__, __LINE__, __FUNC__, altBuf, symBuf, var->var_Node.ln_Name, GetHead(&var->var_CmdList)));
	CopyCmdListConvert(&var->var_CmdList, dstList, altBuf, symBuf);
	FreePathBuffer(symBuf);
	FreePathBuffer(altBuf);
	return;
    }

    if (c != ':')
	error(FATAL, "Bad variable replacement spec: %c", c);

    c = PopCmdListChar(srcList);

    if (c == '\"') {
	i = 0;
	while ((c = PopCmdListChar(srcList)) != EOF && c != '\"')
	{
	    symBuf[i++] = c;
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
	if (c == '\"')
	    c = PopCmdListChar(srcList);
    } else {
	i = 0;
	while (c && c != ')' && c != ':' && c != EOF) {
	    symBuf[i++] = c;
	    c = PopCmdListChar(srcList);
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
    }
    symBuf[i] = 0;

    if (c != ')')
	error(FATAL, "Bad variable replacement spec: %c", c);

    dbprintf(("File: %s Line: %ld Func: %s CopyConvert to %s %s (%s) %08lx\n", __FILE__, __LINE__, __FUNC__, altBuf, symBuf, var->var_Node.ln_Name, GetHead(&var->var_CmdList)));

    CopyCmdListConvert(&var->var_CmdList, dstList, altBuf, symBuf);
    FreePathBuffer(symBuf);
    FreePathBuffer(altBuf);
    return;
}

/*
 *  Since this is recursively called we have to save/restore our temporary
 *  bufferse (SymBuf & AltBuf).  the buf pointer may itself be pointing
 *  into these but we are ok since it is guarenteed >= our copy destination
 *  as we index through it.
 */


STRPTR ParseVariableBuf(List *cmdList, ubyte *buf, short c0)
{
    short c;
    short i = 0;
    Var *var;
    char *symBuf = AllocPathBuffer();
    char *altBuf = AllocPathBuffer();

    dbprintf(("ParseVariableBuf: (%c) %s\n", c0, buf));
    /*
     *	variable name
     */

    while ((c = *buf++) && !SpecialChar[c])
    {
	altBuf[i++] = c;
	if (i >= PBUFSIZE)
	    error(FATAL, "Symbol overflow: %s", altBuf);
    }
    altBuf[i] = 0;

    var = FindVariable(altBuf, c0);
    if (var == NULL)
	error(FATAL, "Variable %s does not exist", altBuf);

    /*
     *	now, handle modifiers
     */

    if (c == ')') {
	CopyCmdList(&var->var_CmdList, cmdList);
	FreePathBuffer(symBuf);
	FreePathBuffer(altBuf);
	return(buf);
    }
    if (c != ':')
	error(FATAL, "Bad variable specification after name %x", c);

    /*
     *	source operation
     */

    c = *buf++;

    if (c == '\"') {
	i = 0;
	while ((c = *buf++) && c != '\"')
	{
	    symBuf[i++] = c;
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
	if (c == '\"')
	    c = *buf++;
    } else {
	i = 0;
	while (c && c != ')' && c != ':') {
	    symBuf[i++] = c;
	    c = *buf++;
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
    }

    symBuf[i] = 0;
    strcpy(altBuf, symBuf);

    /*
     *	destination operation
     */

    if (c == ')') {
	CopyCmdListConvert(&var->var_CmdList, cmdList, altBuf, symBuf);
	FreePathBuffer(symBuf);
	FreePathBuffer(altBuf);
	return(buf);
    }

    if (c != ':')
	error(FATAL, "Bad variable replacement spec: %c", c);

    c = *buf++;

    if (c == '\"') {
	i = 0;
	while ((c = *buf++) && c != '\"')
	{
	    symBuf[i++] = c;
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
	if (c == '\"')
	    c = *buf++;
    } else {
	i = 0;
	while (c && c != ')' && c != ':') {
	    symBuf[i++] = c;
	    c = *buf++;
	    if (i >= PBUFSIZE)
		error(FATAL, "Symbol overflow: %s", symBuf);
	}
    }
    symBuf[i] = 0;

    if (c != ')')
	error(FATAL, "Bad variable replacement spec: %c", c);

    dbprintf(("CopyConvert to %s %s (%s) %08lx\n", altBuf, symBuf, var->var_Node.ln_Name, GetHead(&var->var_CmdList)));

    CopyCmdListConvert(&var->var_CmdList, cmdList, altBuf, symBuf);
    FreePathBuffer(symBuf);
    FreePathBuffer(altBuf);
    return(buf);
}

void ExpandVariableList(List *srclist, List *list)
{
    short c, c1;
    static int Levels;

    if (++Levels == 20)
	error(FATAL, "Too many levels of variable recursion");

    c = PopCmdListChar(srclist);
    while (c != EOF) {
	if (c == '$' || c == '%') {
	    if ((c1 = PopCmdListChar(srclist)) == '(') {
		ParseVariableList(srclist, list,  c);
	    } else if (c1 == c) {
		PutCmdListChar(list, c);
	    } else {
		PutCmdListChar(list, c);
		c = c1;
		continue;
	    }
	} else if (c == '\'') {
	    PutCmdListChar(list, c);
	    if (c = PopCmdListChar(srclist))
	    {
	        do
	        {
		    PutCmdListChar(list, c);
	        }
		while (c != '\'' && (c = PopCmdListChar(srclist)) != EOF);
		if (c == EOF)
		    break;
	    }
	} else {
	    PutCmdListChar(list, c);
	}
	c = PopCmdListChar(srclist);
    }
    --Levels;
    return;
}

STRPTR ExpandVariable(ubyte *buf, List *list)
{
    short c;
    short n = 0;
    short tmpListValid;
    short keepInList;
    List tmpList;
    static int Levels;

    if (++Levels == 20)
	error(FATAL, "Too many levels of variable recursion");

    if (list) {
	keepInList = 1;
	tmpListValid = 1;
    } else {
	keepInList = 0;
	tmpListValid = 0;
	list = &tmpList;
	NewList(list);
    }

    while ((c = buf[n]) != 0) {
	if (c == '$' || c == '%') {
	    if (buf[n+1] == '(') {
		if (tmpListValid == 0) {
		    int i;

		    for (i = 0; i < n; ++i)
			PutCmdListChar(list, buf[i]);
		    tmpListValid = 1;
		}
		n = (ubyte *)ParseVariableBuf(list, buf + n + 2, c) - buf;
	    } else if (buf[n+1] == c) {
		if (tmpListValid)
		    PutCmdListChar(list, c);
		n += 2;
	    } else {
		if (tmpListValid)
		    PutCmdListChar(list, c);
		++n;
	    }
	} else if (c == '\'') {
	    if (tmpListValid == 0) {
	        int i;

	        for (i = 0; i < n; ++i)
		    PutCmdListChar(list, buf[i]);
	        tmpListValid = 1;
	    }
	    PutCmdListChar(list, c);
	    if (c = buf[++n])
	    {
	        do
	        {
		    PutCmdListChar(list, c);
		    n++;
	        }
		while (c != '\'' && (c = buf[n]));
	    }
	} else {
	    if (tmpListValid)
		PutCmdListChar(list, c);
	    ++n;
	}
    }
    if (keepInList == 0) {
	if (tmpListValid) {
	    buf = malloc(CmdListSize(list) + 1);
	    CopyCmdListBuf(list, buf);
	}
    }
    --Levels;
    return(buf);
}


/*
 *  GetToken()	- return a single token
 */


token_t GetToken()
{
    short c;
    short i;

    for (;;) {
	switch(c = getc(Fi)) {
	case EOF:
	    return(0);
	case ':':
	    return(TokColon);
	case '=':
	    return(TokEq);
	case '?':
	    return(TokQuestion);
	case '\n':
	    ++LineNo;
	    return(TokNewLine);
	case '(':
	    return(TokOpenParen);
	case ')':
	    return(TokCloseParen);
	case '$':
	    return(TokDollar);
	case '%':
	    return(TokPercent);
	case ' ':
	case '\t':
	case '\014':
	case '\r':
	    break;
	case '#':
	    while ((c = getc(Fi)) != EOF) {
		if (c == '\n') {
		    ++LineNo;
		    break;
		}
	    }
	    break;
	case '\"':
	    for (i = 0; i < sizeof(SymBuf) - 1 && (c = fgetc(Fi)) != EOF; ++i) {
		if (c == '\n')
		    error(FATAL, "newline in control string");
		if (c == '\"')
		    break;
		if (c == '\\')
		    c = fgetc(Fi);
		SymBuf[i] = c;
	    }
	    SymBuf[i] = 0;
	    if (i == sizeof(SymBuf) - 1)
		error(FATAL, "Symbol overflow: %s", SymBuf);
	    if (c != '\"')
		error(FATAL, "Expected closing quote");
	    return(TokStr);
	case '\\':
	    c = fgetc(Fi);
	    if (c == '\n') {
		++LineNo;
		break;
	    }
	    /* fall through */
	default:
	    SymBuf[0] = c;
	    {
		WORD gotcol = 0;

		for (i = 1; i < sizeof(SymBuf) - 1 && (c = getc(Fi)) != EOF; ++i) {
		    if (gotcol)
		    {
			if (SpecialChar[c])
			{
			    fseek(Fi, -2, SEEK_CUR);
			    i--;
			    break;
			}
			else
			    gotcol = 0;
		    }
		    else if (SpecialChar[c]) {
			if (c == ':')
			    gotcol = 1;
			else
			{
			    ungetc(c, Fi);
			    break;
			}
		    }
		    SymBuf[i] = c;
		}
	    }
	    SymBuf[i] = 0;
	    if (i == sizeof(SymBuf) - 1)
		error(FATAL, "Symbol overflow: %s", SymBuf);
	    return(TokSym);
	}
    }
}

void expect(token_t tgot, token_t twant)
{
    if (tgot != twant)
	error(FATAL, "Unexpected token");
}

void error(short type, CONST_STRPTR ctl, ...)
{
    static char *TypeString[] = { "Fatal", "Warning", "Debug" };
    static char ExitAry[] = { 1, 0, 0 };
    va_list va;

    printf("%s: %s Line %ld: ", FileName, TypeString[type], LineNo);
    va_start(va, ctl);
    vprintf(ctl, va);
    va_end(va);
    puts("");
    if (ExitAry[type])
    {
	exit(20);
    }
}

