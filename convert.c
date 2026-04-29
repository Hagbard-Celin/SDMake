/*
 *    (c)Copyright 1992-1997 Obvious Implementations Corp.  Redistribution and
 *    use is allowed under the terms of the DICE-LICENSE FILE,
 *    DICE-LICENSE.TXT.
 */

/*
 *  CONVERT.C
 */

#include "defs.h"

#define MAXLEVELS   10

Prototype int WildConvert(char *srcBuf, List *dstList, char *srcMat, char *dstMat);

static void ExpandVariableList(List *srclist, List *list);
static void ParseVariableList(List *srcList, List *dstList, short c0);

/*
 *  Run srcBuf through the srcMat pattern matcher and if it matches
 *  convert it via dstMat into dstBuf, else dstBuf \0 len.
 */


int WildConvert(char *srcBuf, List *dstList, char *srcMat, char *dstMat)
{
    long i;
    short r = 0;
    static short Index;
    static short SubLen[MAXLEVELS];
    static char *SubStr[MAXLEVELS];

    if (Index == MAXLEVELS)
	error(FATAL, "maximum recursion reached in WildConvert");

    db4printf(("WildConvert %-15s (%s -> %s)\n", srcBuf, srcMat, (dstMat) ? dstMat : "..."));

    /*
     *	skip non-wildcards, srcBuf must match srcMat
     */

    while (*srcMat && *srcMat != '*' && *srcMat != '?') {
	if (*srcBuf != *srcMat)
	    return(-1);
	++srcBuf;
	++srcMat;
    }

    switch(*srcMat) {
    case '\0':                      /*  match end, terminating case */
	if (*srcBuf)                /*  buf srcBuf not exhausted!   */
	    r = -1;
	break;
    case '?':                       /*  match 1 */
	if (*srcBuf == 0)           /*  match failed against srcbuf  */
	    return(-1);
	SubStr[Index] = srcBuf;
	SubLen[Index] = 1;
	++Index;
	r = WildConvert(srcBuf + 1, NULL, srcMat + 1, NULL);
	--Index;
	break;
    case '*':                       /*  match any   */
	/*
	 *  strangeness in loop is so \0 (nil string) is checked for,
	 *  it is perfectly valid for the remainder to be nil.
	 *
	 *  note: bug in NeXT's GCC -O/-O2, had to reorder r == -1 to
	 *  the right side of the &&
	 */

	r = -1;
	for (i = 0; (i == 0 || srcBuf[i-1]) && r == -1; ++i) {
	    SubStr[Index] = srcBuf;
	    SubLen[Index] = i;
	    ++Index;
	    r = WildConvert(srcBuf + i, NULL, srcMat + 1, NULL);
	    --Index;
	}
	break;
    }
    if (r == 0 && dstMat) {
	List tmplist;
	List *targetlist = dstList;
	short k = 0;
	short n = -1;

	NewList(&tmplist);

	while (*dstMat) {
	    switch(*dstMat) {
	    case '%':
		n = (dstMat[1] - '1');
	    case '*':
	    case '?':
		if (*dstMat == '%')
		    ++dstMat;
		else
		    n = k++;

		if (n >= 0 && n < MAXLEVELS) {
		    PutCmdListLen(targetlist, SubStr[n], SubLen[n]);
		}
		break;
	    case '\'':
		if (targetlist == dstList)
		{
		    targetlist = &tmplist;
		    break;
		}
		else
		{
		    ExpandVariableList(&tmplist, dstList);
		    targetlist = dstList;
		    break;
		}
	    default:
		PutCmdListChar(targetlist, *dstMat);
		break;
	    }
	    ++dstMat;
	}
    }
    db4printf((" r = %ld\n", r));
    return(r);
}

static void ExpandVariableList(List *srclist, List *list)
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

/*
 *  Since this is recursively called we have to save/restore our temporary
 *  bufferse (SymBuf & AltBuf).  the buf pointer may itself be pointing
 *  into these but we are ok since it is guarenteed >= our copy destination
 *  as we index through it.
 */


static void ParseVariableList(List *srcList, List *dstList, short c0)
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


