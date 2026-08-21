/*
 * asyncr_sdmake.h - AsyncR wrapper for SDMake public interface.
 *
 * This file is Public Domain.
 *
 * This code comes with absolutely no warranty.
 * If it breaks, you get to keep the pieces.
 *
 */

#ifndef __ASYNCR_TRACKED_H
#define __ASYNCR_TRACKED_H

#include <asyncr.h>

struct FileList {
    AsyncRFile *File;
    struct FileList *Next;
};

extern struct FileList *OpenFiles;

AsyncRFile *OpenAsyncR_tracked(const STRPTR fileName);
void CloseAsyncR_tracked(struct AsyncRFile *file);

#endif /* __ASYNCR_TRACKED_H */
