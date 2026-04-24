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
#include <fctype.h>

Prototype BOOL StriInStr(CONST_STRPTR find, CONST_STRPTR string);
Prototype STRPTR StrToLower(CONST_STRPTR string, ULONG len);


BOOL StriInStr(CONST_STRPTR find, CONST_STRPTR string)
{
    STRPTR Find, String;
    ULONG findlen, stringlen;
    BOOL ret = FALSE;

    if (!find || !string ||
	!(findlen = strlen(find)) ||
	!(stringlen = strlen(string)))
    {
	return 0;
    }

    Find = StrToLower(find, findlen);
    String = StrToLower(string, stringlen);

    if (strstr(string, find))
	ret = TRUE;

    PFree(Find, findlen + 1);
    PFree(String, stringlen + 1);
    return ret;
}

STRPTR StrToLower(CONST_STRPTR string, ULONG len)
{
    STRPTR newstr, newptr;

    if (!string || !len)
	return 0;

    if (!(newstr = PAlloc(len + 1)))
	MemErr();

    newptr = newstr;
    if (Running2_04())
    {
	do
	{
	    *newptr = ToLower((ULONG)*string);
	    newptr++;
	} while (*++string);
    }
    else
    {
	do
	{
	    *newptr = tolower(*string);
	    newptr++;
	} while (*++string);
    }
    *newptr = 0;
    return newstr;
}
