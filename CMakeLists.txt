cmake_minimum_required(VERSION 3.20)
project(atanua VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  add_compile_options(/W3 /MP)
endif()

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
  find_package(SDL2 REQUIRED)
endif()
find_package(OpenGL REQUIRED)
find_package(tinyxml2 CONFIG QUIET)
if(NOT tinyxml2_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(tinyxml2 REQUIRED IMPORTED_TARGET tinyxml2)
  add_library(tinyxml2::tinyxml2 ALIAS PkgConfig::tinyxml2)
endif()

set(ATANUA_SOURCES
  src/8051/core.c
  src/8051/disasm.c
  src/8051/opcodes.c
  src/basecode/angelcodefont.cpp
  src/basecode/mersennetwister.cpp
  src/basecode/toolkit.cpp
  src/chip/16segchip.cpp
  src/chip/7segchip.cpp
  src/chip/and3chip.cpp
  src/chip/and8chip.cpp
  src/chip/andchip.cpp
  src/chip/audiochip.cpp
  src/chip/box.cpp
  src/chip/buttonchip.cpp
  src/chip/chip2051.cpp
  src/chip/chip27xx.cpp
  src/chip/chip309.cpp
  src/chip/chip7400.cpp
  src/chip/chip7402.cpp
  src/chip/chip7404.cpp
  src/chip/chip7408.cpp
  src/chip/chip7410.cpp
  src/chip/chip74138.cpp
  src/chip/chip74139.cpp
  src/chip/chip74151.cpp
  src/chip/chip74154.cpp
  src/chip/chip74163.cpp
  src/chip/chip74164.cpp
  src/chip/chip74165.cpp
  src/chip/chip74181.cpp
  src/chip/chip74191.cpp
  src/chip/chip74192.cpp
  src/chip/chip74193.cpp
  src/chip/chip74195.cpp
  src/chip/chip7420.cpp
  src/chip/chip74240.cpp
  src/chip/chip74241.cpp
  src/chip/chip74244.cpp
  src/chip/chip74245.cpp
  src/chip/chip74283.cpp
  src/chip/chip7432.cpp
  src/chip/chip7447.cpp
  src/chip/chip74574.cpp
  src/chip/chip7473.cpp
  src/chip/chip7474.cpp
  src/chip/chip7485.cpp
  src/chip/chip7486.cpp
  src/chip/chip7489.cpp
  src/chip/chip7490.cpp
  src/chip/clockchip.cpp
  src/chip/dchip.cpp
  src/chip/dflipflop.cpp
  src/chip/dxchip.cpp
  src/chip/extpin.cpp
  src/chip/extrapin.cpp
  src/chip/jkchip.cpp
  src/chip/jkflipflop.cpp
  src/chip/label.cpp
  src/chip/ledchip.cpp
  src/chip/ledgrid.cpp
  src/chip/logicprobe.cpp
  src/chip/muxchip.cpp
  src/chip/nand3chip.cpp
  src/chip/nand8chip.cpp
  src/chip/nandchip.cpp
  src/chip/nor3chip.cpp
  src/chip/nor8chip.cpp
  src/chip/norchip.cpp
  src/chip/notchip.cpp
  src/chip/or3chip.cpp
  src/chip/or8chip.cpp
  src/chip/orchip.cpp
  src/chip/sedchip.cpp
  src/chip/serchip.cpp
  src/chip/srchip.cpp
  src/chip/srflipflop.cpp
  src/chip/srnegchip.cpp
  src/chip/staticlevelchip.cpp
  src/chip/stepper.cpp
  src/chip/switchchip.cpp
  src/chip/tchip.cpp
  src/chip/tflipflop.cpp
  src/chip/xorchip.cpp
  src/core/AtanuaConfig.cpp
  src/core/basechipfactory.cpp
  src/core/BoxStitchingInformation.cpp
  src/core/chip.cpp
  src/core/fileio.cpp
  src/core/fileutils.cpp
  src/core/main.cpp
  src/core/nativefunctions.cpp
  src/core/net.cpp
  src/core/pin.cpp
  src/core/pluginchip.cpp
  src/core/pluginchipfactory.cpp
  src/core/simutils.cpp
  src/core/slidingaverage.cpp
  src/core/wire.cpp
  src/stb/stb_image.c
  src/stb/stb_image_write.c
)

add_executable(atanua ${ATANUA_SOURCES})

if(MSVC)
  # Third-party stb sources trigger benign 4244/4267 narrowing warnings
  # under /W3; keep first-party code warning-clean without forking them.
  set_source_files_properties(
    src/stb/stb_image.c
    src/stb/stb_image_write.c
    PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4267"
  )
endif()

target_include_directories(atanua PRIVATE
  src/include
  src
)

target_link_libraries(atanua PRIVATE
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  OpenGL::GL
  tinyxml2::tinyxml2
)

if(WIN32)
  target_link_libraries(atanua PRIVATE OpenGL::GLU winmm)
  set_target_properties(atanua PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Linux file dialogs (nativefunctions.cpp) use GTK unconditionally.
if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0)
  target_link_libraries(atanua PRIVATE PkgConfig::GTK)
endif()

# Runtime assets (fonts, textures) live in data/ next to the exe.
# data/ is git-ignored; copy it there if present (e.g. from a release zip).
if(EXISTS "${CMAKE_SOURCE_DIR}/data")
  add_custom_command(TARGET atanua POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/data"
      "$<TARGET_FILE_DIR:atanua>/data"
  )
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT atanua)
