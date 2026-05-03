


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
- Replaced many POSIX functions with dos.library calls.
- Added cache to dependency-scanning to avoid multiple redundant Examine().
- Added multiple internal commands.
- Added multiple directives.
- Converted to use memory pools.
- Fixed starting from Workbench.

For details, see SDMake.guide. Sorry about no detailed change-log yet except for
the messy commit history, I will try to make one before I make first official
release.


To build and install, execute bootstrap.
This uses the supplied SMakefile to build a generic version in "bin/", which is
then used to build and install the appropriate version for the currently running
AmigaOS version to "sc:c/". It also installs the .guide file to "HELP:english/".
The file SDMakefile.config can be edited to change these locations.


This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

