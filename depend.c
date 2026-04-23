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
 *  DEPEND.C
 */

#include "defs.h"

Prototype void InitDep(void);
Prototype DepRef  *CreateDepRef(List *, CONST_STRPTR);
Prototype DepCmdList *AllocDepCmdList(void);
Prototype DepRef  *DupDepRef(DepRef *);
Prototype void	  IncorporateDependency(DepRef *, DepRef *, List *);
Prototype int	  ExecuteDependency(DepNode *parent, DepRef *lhs);

Prototype List DepList;

List DepList;

void InitDep(void)
{
    NewList(&DepList);	    /*	master list */
}

DepRef *CreateDepRef(List *list, CONST_STRPTR name)
{
    DepRef *ref;
    DepNode *dep;

    for (dep = (DepNode *)GetTail(&DepList); dep; dep = (DepNode *)GetPred(&dep->dn_Node)) {
	if (strcmp(name, dep->dn_Node.ln_Name) == 0)
	    break;
    }
    if (dep == NULL) {
	dep = malloc(sizeof(DepNode) + strlen(name) + 1);
	clrmem(dep, sizeof(DepNode));
	dep->dn_Node.ln_Name = (char *)(dep + 1);
	NewList(&dep->dn_DepCmdList);
	strcpy(dep->dn_Node.ln_Name, name);
	AddTail(&DepList, &dep->dn_Node);
    }

    ref = malloc(sizeof(DepRef));
    clrmem(ref, sizeof(DepRef));

    ref->rn_Node.ln_Name = dep->dn_Node.ln_Name;
    ref->rn_Dep = dep;
    AddTail(list, &ref->rn_Node);
    return(ref);
}


DepRef *DupDepRef(DepRef *ref0)
{
    DepRef *ref = malloc(sizeof(DepRef));

    clrmem(ref, sizeof(DepRef));
    ref->rn_Node.ln_Name = ref0->rn_Node.ln_Name;
    ref->rn_Dep = ref0->rn_Dep;
    return(ref);
}

void IncorporateDependency(DepRef *lhs, DepRef *rhs, List *cmdList)
{
    DepNode *dep = lhs->rn_Dep;     /*	left hand side master */
    DepCmdList *depCmdList = (DepCmdList *)GetHead(&dep->dn_DepCmdList);

    /* 
     * Attempt to match against existing lhs:rhs command lists
     */

    while (depCmdList) {
	if (depCmdList->dc_CmdList == cmdList)
	    break;
	depCmdList = (DepCmdList *)GetSucc(&depCmdList->dc_Node);
    }
    if (depCmdList == NULL) {
	depCmdList = malloc(sizeof(DepCmdList));
	clrmem(depCmdList, sizeof(DepCmdList));
	NewList(&depCmdList->dc_RhsList);
	depCmdList->dc_CmdList = cmdList;
	AddTail(&dep->dn_DepCmdList, &depCmdList->dc_Node);
    }

    if (rhs)
	AddTail(&depCmdList->dc_RhsList, &rhs->rn_Node);

    db3printf(("Incorporate: %s -> %s\n", dep->dn_Node.ln_Name, (rhs) ? rhs->rn_Node.ln_Name : ""));

}

/*
 * ExecuteDependancy()
 *
 *  Execute a dependency.  Return the appropriate DN_ code.  We are passed
 *  (parent : lhs)  (lhs is one of possibly several right hand sides to
 *  parent).  We must resolve 'lhs' by running through its own right hand
 *  sides and then aggregating the result into parent.
 *
 *  The dependancy 'parent : lhs' is executed.  We take lhs in the context
 *  of its own dependancies (which is why we call it lhs).
 */

int ExecuteDependency(DepNode *parent, DepRef *lhs)
{
    DepNode *lhsDep = lhs->rn_Dep;
    DepRef *rhsRef;
    DepCmdList *depCmdList;
    int index = 0;
    int parStRes;
    int lhsStRes;
    int runCmds = 0;
    //struct stat parSt;
    //struct stat lhsSt;
    FileInfo parent_info;
    FileInfo lefthand_info;
    int yr = DN_NOCHANGE;
    static int tab;

    /*
     * parStRes reflects the existance of the parent dependancy.
     * e.g. if lib depends on a.o, b.o, c.o, the parent is 'lib'
     * and we run through lhs->rn_Dep (a.o, b.o, c.o).
     */
    if (parent == NULL || parent->dn_Flags & DNF_LEFT_VIRTUAL)
    {
	parStRes = -1;
	clrmem(&parent_info, sizeof(parent_info));
    }
    else
    {
	BPTR tmplock;
	D_S(struct FileInfoBlock, fib);

	if (tmplock = Lock(parent->dn_Node.ln_Name, ACCESS_READ))
	{
	    if (Examine(tmplock, fib))
	    {
		parent_info.size = fib->fib_Size;
		parent_info.type = fib->fib_DirEntryType;
		parent_info.datestamp = fib->fib_Date;
	    }
	    parStRes = 0;
	    UnLock(tmplock);
	}
	else
	{
	    parStRes = -1;
	    clrmem(&parent_info, sizeof(parent_info));
	}
    }

    if (lhsDep->dn_Flags & DNF_LEFT_VIRTUAL)
	lhsStRes = -1;
    else
    {
	BPTR tmplock;
	D_S(struct FileInfoBlock, fib);

	if (tmplock = Lock(lhsDep->dn_Node.ln_Name, ACCESS_READ))
	{
	    if (Examine(tmplock, fib))
	    {
		lefthand_info.size = fib->fib_Size;
		lefthand_info.type = fib->fib_DirEntryType;
		lefthand_info.datestamp = fib->fib_Date;
	    }
	    lhsStRes = 0;
	    UnLock(tmplock);
	}
	else
	    lhsStRes = -1;
    }

    /*
     *  If this lhs has no dependancies, compare the parent file against
     *  the file represented by this lhs to calculate the return value.
     */
    if (GetHead(&lhsDep->dn_DepCmdList) == NULL) {
	if (DoAll)
	    return(DN_CHANGED);

	if (parent == NULL)	/* XXX */
	    return(DN_CHANGED);

	if (lhsStRes < 0) {
	    printf("The file %s could not be found\n", lhsDep->dn_Node.ln_Name);
	    return(DN_FAILED);
	}
	if (parStRes < 0)
	    return(DN_CHANGED);

	if (CompareDates(&parent_info.datestamp, &lefthand_info.datestamp) < 0)
	    return(DN_NOCHANGE);
	return(DN_CHANGED);
    }

    /*
     * Scan the right hand side's dependancies.  e.g. if
     * we are lib : a.o b.o c.o then this scan is testing
     * a.o : a.c,  b.o : b.c,  and c.o : c.c.
     */

    lhsDep->dn_Result = DN_NOCHANGE;
    lhsDep->dn_Node.ln_Type = NT_RESOLVED;
    lhsDep->dn_Flags |= DNF_VIRTUAL;

    for (depCmdList = (DepCmdList *)GetHead(&lhsDep->dn_DepCmdList);
	lhsDep->dn_Result > DN_FAILED && depCmdList;
	depCmdList = (DepCmdList *)GetSucc(&depCmdList->dc_Node)
    ) {
	int xr = DN_NOCHANGE;

	++index;

	if (GetHead(depCmdList->dc_CmdList) != NULL)
	    lhsDep->dn_Flags &= ~DNF_VIRTUAL;

	/*
	 * Scan and run the rhs that our lhs depends on to determine
	 * whether the command list for our lhs must be run.
	 *
	 * Handle the special case where there are no rhs dependancies.
	 * In this case we must return 0 to execute the command list.
	 *
	 * Handle the special case where the lhs does not exist.  In this
	 * case we must return 0 to execute the command list.
	 *
	 * Handle the special case where an rhs dependancy is the same as
	 * the lhs, causing us to test for file existance.
	 */
	if (GetHead(&depCmdList->dc_RhsList) == NULL)
	    xr = DN_CHANGED;

	for (rhsRef = (DepRef *)GetHead(&depCmdList->dc_RhsList);
	    xr > DN_FAILED && rhsRef;
	    rhsRef = (DepRef *)GetSucc(&rhsRef->rn_Node)
	) {
	    int r;

	    if (lhsDep == rhsRef->rn_Dep) {
		/*
		 * file:file or dir:dir
		 */
		BPTR tmplock;

		if (tmplock = Lock(lhsDep->dn_Node.ln_Name, ACCESS_READ))
		{
		    r = DN_NOCHANGE;
		    UnLock(tmplock);
		}
		else
		    r = DN_CHANGED;
	    } else {
		/*
		 * Run the dependancy
		 */
		tab += 4;
		r = ExecuteDependency(lhsDep, rhsRef);
		tab -= 4;
	    }

	    /*
	     * If we won't trivially fail due to the rhs/lhs combo having to
	     * have been run or the parent not existing anyway, and this
	     * baby is not a virtual node (i.e. one that has no command list
	     * and does not exist as a file), then we still have to see if
	     * we are out of date relative to the rhs.
	     *
	     * However, if the rhs is a directory we simply check for
	     * existance.
	     *
	     * for example, lib depends on .o depends on .c.  If the .o is
	     * resolved fine but the library is out of date, we have to
	     * return a failure to generate the lib even though all our
	     * .o->.c dependancies succeeded.  This is 'yr'.
	     *
	     * Additionally, if the lhs does not exist at all we need to
	     * return a failure to generate the result (the else clause).
	     * This can occur in the situation:
	     *
	     *		install: target1 target2
	     *
	     *		target1: objectX
	     *
	     *		target2: objectX
	     *
	     * If we do not handle the case where target2 does not exist we
	     * will end up not running target2's command list due to target1
	     * having caused objectX to resolve.
	     */

	    if (parStRes == 0 &&
		r >= DN_NOCHANGE_TOUCH &&
		(lhsDep->dn_Flags & DNF_VIRTUAL) == 0
	    ) {
		BPTR tmplock;
		D_S(struct FileInfoBlock, fib);

		if (tmplock = Lock(lhsDep->dn_Node.ln_Name, ACCESS_READ))
		{
		    if (Examine(tmplock, fib))
		    {
			if (CompareDates(&parent_info.datestamp, &fib->fib_Date) > 0)
			    if (fib->fib_DirEntryType < 0)
				yr = DN_CHANGED;

		    }
		    UnLock(tmplock);
		} else {
		    yr = DN_CHANGED;
		}
	    }

	    /*
	     * If the parent doesn't exist at all and it is not virtual, we
	     * will want to return that it has changed.
	     */
	    if (parStRes < 0 && r > DN_CHANGED &&
		(parent == NULL || (parent->dn_Flags & DNF_VIRTUAL) == 0)
	    ) {
		yr = DN_CHANGED;
	    }

	    if (xr > r)
		xr = r;

	    dbprintf((
		"%*.*sRUNDEPEND lhs=\"%s\" rhs=\"%s\"\n"
		"%*.*s--------- r=%d cumulative=%d\n",
		tab, tab, "",
		lhsDep->dn_Node.ln_Name,
		rhsRef->rn_Node.ln_Name,
		tab, tab, "",
		r,
		xr
	    ));
	}
	/*
	 *  The DoAll flag forces the command list to be run
	 */
	if (xr > DN_CHANGED && DoAll)
	    xr = DN_CHANGED;

	/*
	 *  If our result is 0 then something had to be run in the
	 *  subdependancies, so we have to run this dependency's
	 *  command list.
	 *
	 *  [re]create %(left) and %(right) variables
	 */
	if (xr == DN_CHANGED)
	    runCmds = 1;
	if (lhsDep->dn_Result > xr)
	    lhsDep->dn_Result = xr;
    }

    /*
     * If runCmds was set, do another run through and execute all the
     * related commands.
     */
    for (depCmdList = (DepCmdList *)GetHead(&lhsDep->dn_DepCmdList);
	runCmds && lhsDep->dn_Result > DN_FAILED && depCmdList;
	depCmdList = (DepCmdList *)GetSucc(&depCmdList->dc_Node)
    ) {
	DepRef  *rhsRef;
	int xr = DN_CHANGED;

	if (GetHead(depCmdList->dc_CmdList) != NULL) {
	    Var *var;

	    dbprintf(("%*.*sRUNCMDLIST \"%s\" index=%d\n",
		tab, tab, "",
		lhsDep->dn_Node.ln_Name, index));

	    if ((var = MakeVariable("left", '%')) != NULL) {
		PutCmdListSym(&var->var_CmdList, lhsDep->dn_Node.ln_Name, NULL);
	    }
	    if ((var = MakeVariable("right", '%')) != NULL) {
		short space = 0;

		for (
		    rhsRef = (DepRef *)GetHead(&depCmdList->dc_RhsList);
		    rhsRef;
		    rhsRef = (DepRef *)GetSucc(&rhsRef->rn_Node)
		) {
		    PutCmdListSym(&var->var_CmdList, rhsRef->rn_Node.ln_Name, &space);
		}
	    }
	    SomeWork = 1;
	    if (ExecuteCmdList(lhsDep, depCmdList->dc_CmdList) > EXIT_CONTINUE)
		xr = DN_FAILED;
	}
	if (lhsDep->dn_Result > xr)
	    lhsDep->dn_Result = xr;
    }

    /*
     * If we ran commands and the left hand side result is marked as having
     * changed, and the left hand side represents a pre-existing file,
     * check to see if the file has been updated.  Normally the file will
     * have been updated by the commands that were run but in certain
     * cases, such as when generating prototypes or dependancies, it is
     * quite possible that no changes were made and the commands explicitly
     * did not rewrite the left hand side file because of that.
     */
    if (runCmds && lhsStRes == 0 && lhsDep->dn_Result == DN_CHANGED) {
	BPTR tmplock;
	D_S(struct FileInfoBlock, fib);

	if (tmplock = Lock(lhsDep->dn_Node.ln_Name, ACCESS_READ))
	{
	    if ((Examine(tmplock, fib)) &&
		(CompareDates(&lefthand_info.datestamp, &fib->fib_Date) == 0) &&
		lefthand_info.size == fib->fib_Size)
	    {
		printf("Setting DN_NOCHANGE_TOUCH %s\n", lhsDep->dn_Node.ln_Name);
		lhsDep->dn_Result = DN_NOCHANGE_TOUCH;
	    }
	    UnLock(tmplock);
	}
    }

    /*
     * yr overrides the final result, indicating that our target is out of
     * date.
     */
    if (lhsDep->dn_Result > yr)
	lhsDep->dn_Result = yr;

    /*
     * If the result code is DN_NOCHANGE_TOUCH we have to touch the
     * pre-existing left hand side file so the next dmake run does not
     * go through this whole mess again.  This will cause DN_NOCHANGE_TOUCH
     * to propogate so, for example, if a source module is changed but the
     * prototypes generation dependancy does not change the prototype file,
     * the prototype file, object modules, and library files will be touched
     * so the next dmake run doesn't have to regenerate the prototypes
     * again.
     */
    if (lhsDep->dn_Result == DN_NOCHANGE_TOUCH) {
	struct DateStamp ds;
	
	printf("TOUCHFILE %s\n", lhsDep->dn_Node.ln_Name);
	DateStamp(&ds);
	SetFileDate(lhsDep->dn_Node.ln_Name , &ds);
    }

    /*
     * If the parent does not exist as a file and we are not a virtual
     * dependancy, mark the parent as having changed.
     */
    yr = lhsDep->dn_Result;
    if (parStRes < 0 &&
	lhsDep->dn_Result > DN_FAILED &&
	(lhsDep->dn_Flags & DNF_VIRTUAL) == 0
    ) {
	yr = DN_CHANGED;
    }

    /*
     * Debugging
     */
    {
	const char *name = parent ? parent->dn_Node.ln_Name : "?";
	dbprintf(("%*.*sFINAL lhs=%s parStRes=%d(%s) r=%d\n",
	    tab, tab, "", lhsDep->dn_Node.ln_Name, parStRes,
	    name, lhsDep->dn_Result));
    }
    return(yr);
}

