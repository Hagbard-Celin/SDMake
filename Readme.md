


SDMake is a port of DMake and back-ports of Amiga relevant parts of DXMake to SAS/C.

It was made mainly because SMake has become to limited for my main project.  
I needed a make that:
- could include multiple files.
- had a query option that worked.
- could parse version and revision from rev.h files to build libraries with SLink.

I assume all this could be done with GNU Make, but I do not want to go down the
road of slowly turning my AmigOS development-environment into POSIX. And I most
certainly do not want to replace a 30K make tool with one that is 161K.

So the best alternative seemed to be porting DMake and back-porting the .include
directive from DXMake.


Most notable changes from DMake/DXMake so far:
- Removed all not Amiga related code.
- Replaced many ANSI/POSIX functions with dos.library calls.
- Added cache to dependency-scanning to avoid multiple redundant Examine().
- Added multiple internal commands.
- Added multiple directives.
- Converted to use memory pools.
- Fixed starting from Workbench.
- Converted to asyncio.library/compiled in AsyncIO.

For details, see SDMake.guide. Sorry about no detailed change-log yet except for
the messy commit history, I will try to make one before I make first official
release.


To build and install, execute bootstrap.
This uses the supplied SMakefile to build a generic version in "bin/", which is
then used to build and install the appropriate version for the currently running
AmigaOS version to "sc:c/". It also installs the .guide file to "HELP:english/".
The file SDMakefile.config can be edited to change these locations. It will
also install the file SDMake-SMake.def to "ENVARC:SDMake/" if it is not already
present. This file is used for limited compatibility with SMake default variables
when the selected makefile is named smakefile.

There is also a line in SDMakefile.config that can be un-commented to switch on
support for -d command line option and DEBUG tool type.

If asyncio.library v39.2 or greater is detected in Libs:, the bootstrap will
build a version using that. In the opposite case it will compile and link the
relevant code from "asyncio/src/".


## License

This project is a combined fork of upstream code originally derived from the
DICE Amiga compiler and the DragonFly BSD project.

* The overall project and all modifications made by the author are licensed
  under the **GNU General Public License v2.0 or later** (see below), with the
  explicit exception of the AsyncIO subsystem modifications detailed below.
* Upstream code remains governed by its original copyright holders. See
  `DICE-LICENSE.txt` for the DICE copyright terms, and individual file headers
  for the DragonFly BSD Project terms.

---

### GPL-2.0-or-later

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

### AsyncIO Subsystem Exception

As a special exception to the GNU General Public License, permission is granted
to link this program with the "AsyncIO" utility routines and distribute the
resulting executable.

The original "AsyncIO" source code files and their derivatives contained
within this repository remain separate from the GPL. This subsystem has the
following intellectual property and authorship profile:

* **Martin Taillefer (Core Author):** The core asynchronous I/O logic was
  authored independently by Martin prior to Commodore's 1994 bankruptcy.
  Martin has explicitly granted permission to treat his portion of the code
  as **Public Domain**.
* **Olaf Barthel (Contributor):** Extensive custom code changes integrated
  from his `AIFF_dtc.lha v1.11` release were explicitly designated by him
  as **Public Domain**.
* **Legacy Header Discrepancies:** Users reviewing old Amiga Developer CDs
  will find conflicting corporate copyright claims stamped on these files:
  - *Commodore 1993:* This legacy header is superseded by Martin's personal
    ownership and his explicit permission to treat his work as Public Domain.
  - *Amiga, Inc. 1999:* This header was applied via automated compilation
    scripts by a later entity. Because the logic was authored by Martin prior
    to Commodore's 1994 bankruptcy, and he never held employment or contracts
    with the 1999 Amiga, Inc. corporation, that boilerplate claim is void.
* **Pending Community Approvals:** Changes to this code present in the
  unmodified `AsyncIO/` subdirectory are held under a pending-clarification
  status. Formal licensing inquiries to clear these blocks under compatible
  open terms were sent in June 2026 to:
  - **Magnus Holmgren:** For shared library code, bug-fixes and general
    enhancements.
  - **Michael B. Smith:** For the `FGetsAsync` and `FGetsLenAsync` functions.

All modifications made to these AsyncIO files by the author of this project
are explicitly dedicated to the **Public Domain**.

You must obey the GNU General Public License in all respects for all of
the code used other than AsyncIO. If you modify the project files, you may
extend this exception to your version, but you are not obligated to do so.
