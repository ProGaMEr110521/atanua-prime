GLee 5.21 (vendored)
====================

This directory contains a vendored copy of Ben Woodhouse's GL Easy Extension
Library (GLee) version 5.21. GLee loads OpenGL extension function pointers.

It is compiled only by the legacy `makefile` at the repository root
(`glee/GLee.c`). Atanua Prime's recommended CMake build links OpenGL directly
and does not use GLee.

Upstream: http://elf-stone.com (see `extensionList.txt` for supported extensions)

Files here are unchanged third-party sources kept for historical compatibility
with the original Atanua build. Do not edit `GLee.c` or `GLee.h` unless you
are intentionally updating the vendored library.

License (summary): redistribution permitted with copyright notice retained;
see the LICENSE section in the original upstream readme for full terms.
