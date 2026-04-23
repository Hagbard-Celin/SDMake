
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

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include "lists.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#define Running2_04() (SysBase->LibNode.lib_Version >= 37)

typedef struct Node Node;
typedef struct List List;

typedef unsigned char ubyte;
typedef unsigned short uword;

#define EXIT_CONTINUE	5

#define Prototype extern

#define FATAL	0
#define WARN	1
#define DEBUG	2

#define PBUFSIZE 256

#define NT_RESOLVED	0x01

#define D_S(type,name) char a_##name[sizeof(type)+3]; \
		       type *name = (type *)((ULONG)(a_##name+3) & ~3UL)


#define clrmem(d,n)	  memset(d,0,n)
#define BTOC(bptr, type)  ((type *)((long)(bptr) << 2))
#define CTOB(cptr)  ((BPTR)((unsigned long)(cptr) >> 2))

#if USE_DEBUG
#define dbprintf(x)  { if (DDebug) printf x;}
#define db3printf(x) { if (DDebug >= 3) printf x;}
#define db4printf(x) { if (DDebug >= 4) printf x;}
#else
#define dbprintf(x)
#define db3printf(x)
#define db4printf(x)
#endif
/*
 *  A DepNode collects an entire left hand side symbol
 *  A DepCmdList collects one of possibly several groups for a DepNode
 *  A DepRef specifies a single dependency within a group
 *
 */

typedef struct DepNode {
    Node    dn_Node;
    List    dn_DepCmdList;	/*  list of lists   */
/*    time_t  dn_Time;*/
    short   dn_Symbolic;
    short   dn_Flags;
    int	    dn_Result;
} DepNode;

#define DNF_VIRTUAL	0x0001	/* virtual lhs - has no command list */
#define DNF_LEFT_VIRTUAL (1<<1)

#define DN_FAILED		-1
#define DN_CHANGED 		0
#define DN_NOCHANGE_TOUCH	1
#define DN_NOCHANGE		2


typedef struct DepRef  {
    Node    rn_Node;
    DepNode *rn_Dep;
} DepRef;

typedef struct DepCmdList {
    Node    dc_Node;		/*  greater link node	*/
    List    dc_RhsList; 	/*  right hand side(s)	*/
    List    *dc_CmdList;	 /*  command buf list	 */
} DepCmdList;

#define NT_CMDEOL   0x01

typedef struct CmdNode {
    Node    cn_Node;
    long    cn_Idx;
    long    cn_Max;
    long    cn_RIndex;
} CmdNode;

typedef struct Var {
    Node    var_Node;
    List    var_CmdList;
} Var;

typedef struct IfNode {
    struct IfNode *if_Next;
    int		if_Value;
} IfNode;

typedef struct FileInfo
{
    ULONG size;
    LONG type;
    struct DateStamp datestamp;
} FileInfo;

#include "tokens.h"
#include "sdmake-protos.h"

