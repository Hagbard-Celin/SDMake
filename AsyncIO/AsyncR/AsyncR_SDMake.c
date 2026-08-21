/*
 * AsyncR_SDMake.c - a simple AsyncR wrapper for SDMake.
 *
 * This file is Public Domain.
 *
 * This code comes with absolutely no warranty.
 * If it breaks, you get to keep the pieces.
 *
 */

#include "defs.h"

#include <asyncr_sdmake.h>

struct FileList *OpenFiles;


AsyncRFile *OpenAsyncR_tracked(const STRPTR fileName)
{
    AsyncRFile *file;

    file = OpenAsyncR(fileName, 8192);

    if (file)
    {
	struct FileList *thisfile;

	if (!(thisfile = PAlloc(sizeof(struct FileList))))
	{
	    CloseAsyncR(file);
	    error(FATAL, IoErr(), "memory allocation failed");
	}

	thisfile->File = file;

	if (!OpenFiles)
	    thisfile->Next = 0;
	else
	    thisfile->Next = OpenFiles;

	OpenFiles = thisfile;
    }

    return file;
}

void CloseAsyncR_tracked(struct AsyncRFile *file)
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
	{
	    PFree(thisfile, sizeof(struct FileList));
	    CloseAsyncR(file);
	}
    }
}
