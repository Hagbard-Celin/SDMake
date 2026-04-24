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
Prototype void StrToLower(STRPTR string);


BOOL StriInStr(CONST_STRPTR find, CONST_STRPTR string)
{
    STRPTR Find, String;
    BOOL ret = FALSE;

    if (!find || !string)
	return 0;

    Find = strdup(find);
    String = strdup(string);

    if (strstr(string, find))
	ret = TRUE;

    free(Find);
    free(String);
    return ret;
}

void StrToLower(STRPTR string)
{
    if (!string || !string[0])
	return;

    if (Running2_04())
    {
	do
	{
	    *string = ToLower((ULONG)*string);
	} while (*++string);
    }
    else
    {
	do
	{
	    *string = tolower(*string);
	} while (*++string);
    }
}
