TinyXML 2.5.3 (vendored)
========================

This directory contains a vendored copy of Lee Thomason's TinyXML 2.5.3 XML
parser (zlib license). It is included for the legacy `makefile` build at the
repository root.

Atanua Prime's recommended CMake build does NOT compile these sources.
Application XML I/O uses the TinyXML2 library instead (`tinyxml2` in
`vcpkg.json` on Windows; `libtinyxml2-dev` on Linux). See `src/core/fileio.cpp`,
`src/core/AtanuaConfig.cpp`, and `src/core/pluginchipfactory.cpp`.

Upstream project: https://sourceforge.net/projects/tinyxml/

Files here are unchanged third-party sources kept for historical compatibility
with the original Atanua makefile. For parser API documentation, refer to the
upstream TinyXML distribution or the HTML docs in `docs/`.
