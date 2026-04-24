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
 *  CMDLIST.c
 *
 */

#include "defs.h"

Prototype void InitCmdList(void);
Prototype void PutCmdListChar(List *, char);
Prototype void PutCmdListSym(List *, char *, short *);
Prototype void PutCmdListLen(List *list, char *buf, LONG len);
Prototype void CopyCmdList(List *, List *);
Prototype void FreeCmdList(List *);
Prototype void AppendCmdList(List *, List *);
Prototype WORD PopCmdListSym(List *, char *, WORD);
Prototype int  PopCmdListChar(List *);
Prototype void CopyCmdListBuf(List *, char *);
Prototype void CopyCmdListNewLineBuf(List *, char *);
Prototype long CmdListSize(List *);
Prototype long CmdListSizeNewLine(List *);
Prototype long CmdListSizeCommand(List *list);
Prototype void CopyCmdListConvert(List *, List *, char *, char *);
Prototype long ExecuteCmdList(DepNode *, List *);

List CmdFreeList;
__aligned char CmdTmp1[256];
__aligned char CmdTmp2[256];

void InitCmdList()
{
    NewList(&CmdFreeList);
}

void PutCmdListChar(List *list, char c)
{
    CmdNode *node;

    if ((node = (CmdNode *)GetTail(list)) == NULL || (node->cn_Idx == node->cn_Max)) {
	if ((node = (CmdNode *)RemHead(&CmdFreeList)) == NULL) {
	    if (!(node = PAlloc(sizeof(CmdNode) + 64)))
		MemErr();
	    node->cn_Node.ln_Name = (char *)(node + 1);
	    node->cn_Max = 64;
	}
	node->cn_Node.ln_Type = 0;
	node->cn_Idx = 0;
	node->cn_RIndex = 0;
	AddTail(list, &node->cn_Node);
    }
    node->cn_Node.ln_Name[node->cn_Idx++] = c;
}

void PutCmdListLen(List *list, char *buf, LONG len)
{
    if (*buf && len) {
	while (len--) {
	    PutCmdListChar(list, *buf);
	    ++buf;
	}
    }
}
void PutCmdListSym(List *list, char *buf, short *pspace)
{
    if (*buf) {
	if (pspace) {
	    if (*pspace)
		PutCmdListChar(list, ' ');
	    *pspace = 1;
	}
	/*if ((node = GetTail(list)) && node->cn_Idx && node->cn_Node.ln_Name[node->cn_Idx-1] != ' ')*/
	while (*buf) {
	    PutCmdListChar(list, *buf);
	    ++buf;
	}
    }
}

void CopyCmdList(List *fromList, List *toList)
{
    CmdNode *from;
    long n;
    long i;

    for (from = (CmdNode *)GetHead(fromList); from; from = (CmdNode *)GetSucc(&from->cn_Node)) {
	CmdNode *copy = NULL;

	dbprintf(("COPYFROM %*.*s\n", from->cn_Idx, from->cn_Idx, from->cn_Node.ln_Name));

	for (n = 0; n < from->cn_Idx; ) {
	    if ((copy = (CmdNode *)RemHead(&CmdFreeList)) == NULL) {
		if (!(copy = PAlloc(sizeof(CmdNode) + 64)))
		    MemErr();
		copy->cn_Max = 64;
		copy->cn_Node.ln_Name = (char *)(copy + 1);
	    }
	    i = (copy->cn_Max < from->cn_Idx - n) ? copy->cn_Max : from->cn_Idx - n;
	    copy->cn_Node.ln_Type = 0;
	    copy->cn_Idx = i;
	    copy->cn_RIndex = 0;
	    movmem(from->cn_Node.ln_Name + n, copy->cn_Node.ln_Name, i);
	    AddTail(toList, &copy->cn_Node);
	    n += i;
	}
	if (copy)
	    copy->cn_Node.ln_Type = from->cn_Node.ln_Type;
    }
}

void FreeCmdList(List *list)
{
    CmdNode *node;

    while ((node = (CmdNode *)RemHead(list)) != NULL)
	AddTail(&CmdFreeList, &node->cn_Node);
}

void AppendCmdList(List *fromList, List *toList)
{
    CmdNode *from;

    while ((from = (CmdNode *)RemHead(fromList)) != NULL)
	AddTail(toList, &from->cn_Node);
}


/*
 *  pop a symbol (symbols are separated by white space)
 */

WORD PopCmdListSym(List *cmdList, char *buf, WORD max)
{
    short c;
    WORD i = 0;

    --max;

    while ((c = PopCmdListChar(cmdList)) == ' ' || c == '\t' || c == '\n')
	;
    while (c != EOF && c != ' ' && c != '\t' && c != '\n' && i < max) {
	buf[i++] = c;
	c = PopCmdListChar(cmdList);
    }
    buf[i] = 0;

    return(i);
}

int PopCmdListChar(List *cmdList)
{
    CmdNode *node;
    short c = EOF;

    while ((node = (CmdNode *)GetHead(cmdList)) != NULL) {
	if (node->cn_RIndex != node->cn_Idx)
	    return((ubyte)node->cn_Node.ln_Name[node->cn_RIndex++]);
	Remove((struct Node *)node);
	AddTail(&CmdFreeList, &node->cn_Node);
    }
    return(c);
}

void CopyCmdListBufLen(List *list, char *buf, LONG len)
{
    CmdNode *node;

    while ((node = (CmdNode *)RemHead(list)) != NULL) {
	LONG nodelen = node->cn_Idx - node->cn_RIndex;
	LONG copylen;

	if (nodelen < len)
	    copylen = nodelen;
	else
	    copylen = len;

	movmem(node->cn_Node.ln_Name + node->cn_RIndex, buf, copylen);
	buf += copylen;
	len -= copylen;
	if (copylen < nodelen) {
	    node->cn_RIndex += copylen;
	    AddHead(list, &node->cn_Node);
	    break;
	}
	AddTail(&CmdFreeList, &node->cn_Node);
    }
    buf[0] = 0;
}

void CopyCmdListBuf(List *list, char *buf)
{
    CmdNode *node;

    while ((node = (CmdNode *)RemHead(list)) != NULL) {
	LONG nodelen = node->cn_Idx - node->cn_RIndex;
	movmem(node->cn_Node.ln_Name + node->cn_RIndex, buf, nodelen);
	buf += nodelen;
	AddTail(&CmdFreeList, &node->cn_Node);
    }
    buf[0] = 0;
}

void CopyCmdListNewLineBuf(List *list, char *buf)
{
    CmdNode *node;
    long i;

    while ((node = (CmdNode *)RemHead(list)) != NULL) {
	for (i = node->cn_RIndex; i < node->cn_Idx && node->cn_Node.ln_Name[i] != '\n'; ++i)
	    *buf++ = node->cn_Node.ln_Name[i];
	if (i != node->cn_Idx) {
	    node->cn_RIndex = i + 1;
	    AddHead(list, &node->cn_Node);
	    break;
	}
	AddTail(&CmdFreeList, &node->cn_Node);
    }
    *buf = 0;
}

long CmdListSize(List *list)
{
    CmdNode *node;
    long n = 0;

    for (node = (CmdNode *)GetHead(list); node; node = (CmdNode *)GetSucc(&node->cn_Node))
	n += node->cn_Idx - node->cn_RIndex;
    return(n);
}

long CmdListSizeCommand(List *list)
{
    CmdNode *node;
    long n = 0;
    long i;
    WORD twolt = 0;
    WORD threelt = 0;
    WORD newline = 0;

    for (node = (CmdNode *)GetHead(list); node; node = (CmdNode *)GetSucc(&node->cn_Node)) {

	for (i = node->cn_RIndex; i < node->cn_Idx; ++i)
	{
	    if (twolt < 2)
	    {
		if (node->cn_Node.ln_Name[i] == '<')
		    twolt++;
		else
		{
		    twolt = 0;

		    if (node->cn_Node.ln_Name[i] == '\n')
		        break;
		}
	    }
	    else
	    {
		if (newline)
		{
		    if (threelt < 3)
		    {
			if (node->cn_Node.ln_Name[i] == '<')
			    ++threelt;
			else
			{
			    threelt = 0;
			    if (node->cn_Node.ln_Name[i] != '\n')
				newline = 0;
			}
		    }
		    else
		    {
			if (node->cn_Node.ln_Name[i] == '\n')
			{
			    break;
			}
			newline = threelt = 0;
		    }
		}
		if (node->cn_Node.ln_Name[i] == '\n')
		    newline = 1;
	    }
	}

	n += i - node->cn_RIndex;
	if (i != node->cn_Idx)
	    break;
    }
    return(n);
}

long CmdListSizeNewLine(List *list)
{
    CmdNode *node;
    long n = 0;
    long i;

    for (node = (CmdNode *)GetHead(list); node; node = (CmdNode *)GetSucc(&node->cn_Node)) {
	for (i = node->cn_RIndex; i < node->cn_Idx && node->cn_Node.ln_Name[i] != '\n'; ++i)
	    ;
	n += i - node->cn_RIndex;
	if (i != node->cn_Idx)
	    break;
    }
    return(n);
}

void CopyCmdListConvert(List *fromList, List *toList, char *srcMat, char *dstMat)
{
    List tmpList;
    short space = 0;
    char *orgsrc = srcMat;
    char *orgdst = dstMat;

    dbprintf(("fromlist %08lx copyconvert '%s' -> '%s'\n", GetHead(fromList), srcMat, dstMat));
    srcMat = ExpandVariable(srcMat, NULL);
    dstMat = ExpandVariable(dstMat, NULL);

    NewList(&tmpList);
    CopyCmdList(fromList, &tmpList);

    if (!*srcMat)
    {
       if (!GetHead(&tmpList))
       {
	  PutCmdListSym(toList, dstMat, &space);
	  return;
       }
       /* We need to replace the src and destination with meaningful wildcards */
       srcMat = dstMat = "*";
    }

    /*
     *	run each symbol through the conversion
     */

    while (PopCmdListSym(&tmpList, CmdTmp1, sizeof(CmdTmp1)) == 0)
    {
	if (space)
	    PutCmdListChar(toList, ' ');
	space = 1;
	WildConvert(CmdTmp1, toList, srcMat, dstMat);
    }
    if (srcMat != orgsrc)
	PFreeVec(srcMat);
    if (dstMat != orgdst)
	PFreeVec(dstMat);
}

/*
 *  The command list is executed by making a duplicate of it then reparsing
 *  it resolving variable references
 *
 */

long ExecuteCmdList(DepNode *dep, List *list)
{
    List tmpSrc;
    List tmpDst;
    short c;
    short withfail = 0;
    long r = 0;
    IfNode *cmdIfBase = NULL;
    LONG cmdIfTrue = 1;

    NewList(&tmpSrc);
    NewList(&tmpDst);
    CopyCmdList(list, &tmpSrc);

    while ((c = PopCmdListChar(&tmpSrc)) != EOF) {
	if (c == '$' || c == '%') {
	    short c0 = c;

	    c = PopCmdListChar(&tmpSrc);
	    if (c == '(') {     /*  Variable Ref */
		char *spec = AllocPathBuffer();

		/*
		 *  copy variable specification into a buffer then resolve
		 *  it to tmpDst
		 */
		spec[0] = c0;
		spec[1] = c;
		for (c0 = 2; (c = PopCmdListChar(&tmpSrc)) != EOF && c != ')' && c0 < PBUFSIZE - 3; ++c0) {
		    if (c == '\"') {
			spec[c0++] = c;
			while ((c = PopCmdListChar(&tmpSrc)) != EOF && c != '\"' && c0 < PBUFSIZE - 3)
			    spec[c0++] = c;
		    }
		    spec[c0] = c;
		}
		if (c != ')')
		    error(FATAL, "bad variable spec in command list for %s", dep->dn_Node.ln_Name);
		spec[c0++] = c;
		spec[c0] = 0;
		ExpandVariable(spec, &tmpDst);
		FreePathBuffer(spec);
		continue;
	    }
	    if (c != c0)		/*  $$, %% escape   */
		PutCmdListChar(&tmpDst, c0);
	}
	PutCmdListChar(&tmpDst, c);
    }

    /*
     *	pop into a command buffer for execution
     */

    {
	long n;

	while (r <= EXIT_CONTINUE && (n = CmdListSizeCommand(&tmpDst))) {
	    short allocated;
	    short quiet = 0;
	    short ignore= 0;
	    char *cmd;

	    if (QuietCmd)
		quiet = 1;

	    if (n >= sizeof(CmdTmp1) - 2) {	/*  avoid malloc    */
		allocated = 1;
		if (!(cmd = (char *)PAllocVec(n + 2)))
		    error(FATAL, "memory allocation failed");
	    } else {
		allocated = 0;
		cmd = CmdTmp1;
	    }
	    while ((c = PopCmdListChar(&tmpDst)) != EOF && (c == ' ' || c == '\t'))
		--n;
	    if (c == '@') {
		quiet = 1;
		c = PopCmdListChar(&tmpDst);
		--n;
	    }
	    if (c == '-') {
		ignore = 1;
		c = PopCmdListChar(&tmpDst);
		--n;
	    }
	    cmd[0] = c;
	    CopyCmdListBufLen(&tmpDst, cmd + 1, n);

	    if (c) {
		cmd[n] = 0;

		if (cmd[--n] == '<' && cmd[--n] == '<' && cmd[--n] == '<' && cmd[--n] == '\n')
		    cmd[n] = 0;

		if (quiet == 0)
		{
		    printf("    %s", cmd);
		    if (cmdIfTrue)
			printf("\n");
		}

		if (NoRunOpt == 0 && cmd[0] != '#') {
		    r = Execute_Command(cmd, ignore, quiet, &cmdIfBase, &cmdIfTrue);
		    SomeWork = 1;
		    if (r == -42)
		    {
			withfail = 1;
			r = 0;
		    }
		    if (r < 0)
			r = 20;
		    if (ExitCode < r)
			ExitCode = r;
		}
		else
		{
		    if (NoRunOpt && cmd[0] != '#')
			SomeWork = 1;
		    if (quiet == 0 && !cmdIfTrue)
			printf("\n");
		}
	    }
	    if (allocated)
		PFreeVec(cmd);
	}

	if (cmdIfBase != NULL)
	    error(FATAL, "missing endif(s) in command list for %s", dep->dn_Node.ln_Name);

    }
    if (withfail)
	return (-1);
    return(r);
}
