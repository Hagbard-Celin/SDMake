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

Prototype AsyncFile *OpenAsyncR(const STRPTR fileName);
Prototype void CloseAsyncR(struct AsyncFile *file);

Prototype struct FileList *OpenFiles;

struct FileList *OpenFiles;


AsyncFile *OpenAsyncR(const STRPTR fileName)
{
    BPTR lock;
    UWORD buffsize = 8192;
    AsyncFile *file;

    if (lock = Lock(fileName, ACCESS_READ))
    {
	D_S(struct FileInfoBlock, fib);
	UWORD half = 4096;

	Examine(lock, fib);

	while (fib->fib_Size < half)
	{
	    buffsize = half;
	    if (buffsize == 256)
		break;
	    half >>= 1;
	}

	UnLock(lock);
    }

    if (file = OpenAsync(fileName, MODE_READ, buffsize))
    {
	struct FileList *thisfile;

	if (!(thisfile = PAlloc(sizeof(struct FileList))))
	    error(FATAL, "memory allocation failed");

	thisfile->File = file;

	if (!OpenFiles)
	    thisfile->Next = 0;
	else
	    thisfile->Next = OpenFiles;
	
	OpenFiles = thisfile;
    }

    return file;
}

void CloseAsyncR(struct AsyncFile *file)
{
    if (OpenFiles)
    {
	struct FileList *thisfile;

	thisfile = OpenFiles;

	if (thisfile->File == file)
	{
	    OpenFiles = thisfile->Next;
	}
	else
	{
	    struct FileList *prevfile;

	    do
	    {
		prevfile = thisfile;
		thisfile = prevfile->Next;
	    } while (thisfile && thisfile->File != file);

	    if (thisfile)
		prevfile->Next = thisfile->Next;
	}

	if (thisfile)
	    PFree(thisfile, sizeof(struct FileList));
    }

    CloseAsync(file);
}

