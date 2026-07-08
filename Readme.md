


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


The build is currently broken, this is beeing worked on.
Updated build/install instruction will be posted when it is again possible to
compile and link the project.


## License

This project is a combined work derived from multiple upstream sources,
integrated with original modifications by the author.

### Overall Combined Work & Modifications
* **The Combined Work:** When used, distributed, or compiled as a whole, this
  project is licensed under the **GNU General Public License v2.0 or later**
  (see below).
* **Author Modifications:** All modifications and original code added by the
  author (outside of the `AsyncIO/` subsystem) are licensed under the
  **GNU General Public License v2.0 or later**.

### Upstream Components & Subsystems
* **DICE Amiga Compiler Code:** Upstream code remains governed by its original
  copyright holders under the **BSD 3-Clause License**. See `DICE-LICENSE.txt`
  for the full terms.
* **DragonFly BSD Project Code:** Upstream code remains governed by its original
  copyright holders under the **BSD 3-Clause License**. See individual file
  headers for specific terms.
* **AsyncIO Subsystem:** The `AsyncIO/` directory is entirely **Public Domain**
  and is completely exempt from the GPL. This subsystem is based on code by
  Martin Taillefer (permission archived in `LICENSE.AsyncIO`). All modifications
  made to this subsystem by the author are also explicitly dedicated to the
  Public Domain.
* **AsyncIO Subsystem:** The `AsyncIO/` directory is entirely **Public Domain**
  and is completely exempt from the GPL. This subsystem is based on code by
  Martin Taillefer (permission archived in `LICENSE.AsyncIO`). All modifications
  made to this subsystem by the author are also explicitly dedicated to the
  Public Domain.

---

### GNU General Public License v2.0 or later

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

You must obey the GNU General Public License in all respects for the combined
project, all author modifications (outside `AsyncIO/`), and any works derived
from them.

*Note: Unmodified upstream files from the DICE and DragonFly BSD projects remain
available under their original, permissive BSD terms if extracted and used
independently from this combined work.*

### AsyncIO Subsystem (Public Domain)

The `AsyncIO/` subsystem included with this repository is entirely dedicated
to the **Public Domain**.

As a result, all source files contained within the `AsyncIO/` subsystem may
be used, modified, copied, redistributed, or incorporated into other works
without any copyleft restrictions or obligations under the GPL.

