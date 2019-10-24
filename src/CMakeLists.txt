# Copyright (C) 2019  Jimmy Aguilar Mena

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

include_directories (${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_BINARY_DIR})

set (CHARSETMORE "${PROJECT_BINARY_DIR}/charsetmore.c")
set (CHARSETJIS "${PROJECT_BINARY_DIR}/charsetjis.def")
set_source_files_properties (${CHARSETMORE} ${CHARSETJIS}
  PROPERTIES GENERATED true)

add_subdirectory (cp)

if (CONFIG_HTML)
  include_directories (libqhtml)
  add_subdirectory (libqhtml)
endif ()

# Sources

set (SOURCES
  qe.c util.c cutils.c charset.c buffer.c search.c
  input.c display.c hex.c list.c)

if (ENABLE_TINY)
  set (QE_DEFINITION "CONFIG_TINY")
  set (SOURCES ${SOURCES} parser.c)
else ()
  set (SOURCES ${SOURCES}
    qescript.c extras.c variables.c fractal.c charsetjis.c ${CHARSETMORE})

  if (CONFIG_ALL_KMAPS)
    set (SOURCES ${SOURCES} kmap.c)
  endif ()

  if (CONFIG_UNICODE_JOIN)
    set (SOURCES ${SOURCES} unicode_join.c arabic.c indic.c qfribidi.c)
  endif ()

  set (SOURCES ${SOURCES}
    unihex.c bufed.c clang.c xml.c
    htmlsrc.c forth.c arm.c lisp.c makemode.c
    markdown.c orgmode.c perl.c script.c
    ebnf.c cobol.c rlang.c txl.c nim.c rebol.c elm.c jai.c ats.c
    ${EXTRA_MODES} extra-modes.c)
endif ()

if (WIN32)
  if (NOT CONFIG_CYGWIN)
    set (LIBS ${LIBS} ws2_32)
  endif ()

  set (SOURCES ${SOURCES} unix.c win32.c)
  set (LIBS ${LIBS} msvcrt gdi32 wsock32)
else ()
  set (SOURCES ${SOURCES} unix.c tty.c)
  set (LIBS ${LIBS} m)

  if (NOT ENABLE_TINY)
    set (SOURCES ${SOURCES} shell.c dired.c latex-mode.c archive.c)
  endif ()

  if (CMAKE_SYSTEM_NAME STREQUAL Haiku)
    set (SOURCES ${SOURCES} haiku.cpp)
    set (LIBS ${LIBS} network be stdc++)
  endif ()

  if (ENABLE_X11 AND X11_FOUND)
      set (SOURCES ${SOURCES} x11.c)
      set (LIBS ${LIBS} ${QE_X11})
  endif ()

  if (CONFIG_INIT_CALLS)
    set (QEND qeend.c)
  endif ()

  if (CONFIG_PLUGINS)
    add_subdirectory (plugins)
  endif ()

endif ()

function (make_modules output)
  set (targetname "${output}")
  set (filename "${${output}}")
  add_custom_command (
    OUTPUT "${filename}"
    COMMAND cmake -DOUT="${filename}" -DIN="${ARGN}" -P "${PROJECT_SOURCE_DIR}/cmake-modules.cmake"
    DEPENDS ${ARGN}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generate ${filename}"
    )
  add_custom_target (${targetname} DEPENDS ${filename})
  add_dependencies(${targetname} CHARSET)
endfunction ()

set (MODULES "${CMAKE_CURRENT_BINARY_DIR}/modules.txt")
make_modules (MODULES "${SOURCES}")
add_executable (qemacs "${SOURCES}")
add_dependencies (qemacs CHARSET MODULES)
target_link_libraries (qemacs ${LIBS})
target_compile_definitions (qemacs PRIVATE ${QE_DEFINITION})
install(TARGETS qemacs DESTINATION ${CMAKE_INSTALL_BINDIR})

# Extra files

# Generate config.h
configure_file (config.h.in config.h)

if (CONFIG_HTML)
  set (SOURCES ${SOURCES} html.c docbook.c)
  set (LIBS ${LIBS} qhtml)

  add_executable (fbftoqe fbftoqe.c cutils.c)
  file (GLOB fonts_in fonts/*.fbf)
  add_custom_command(
    OUTPUT fbffonts.c
    COMMAND fbftoqe ${fonts_in} > fbffonts.c
    DEPENDS fbftoqe ${fonts_in})

  set (OBJS1 html2png.c util.c cutils.c
    arabic.c indic.c qfribidi.c display.c unicode_join.c
    charset.c ${CHARSETMORE} charsetjis.c
    libfbf.c fbfrender.c cfb.c fbffonts.c)

  add_executable (html2png ${OBJS1})
  add_dependencies (html2png CHARSET)
  target_link_libraries (html2png qhtml)
  install(TARGETS html2png DESTINATION ${CMAKE_INSTALL_BINDIR})

  if (CONFIG_PNG_OUTPUT)
    target_compile_definitions (html2png PRIVATE CONFIG_PNG_OUTPUT=1)
    target_link_libraries (html2png ${PNG_LIBRARIES})
  endif ()
endif ()

add_executable (ligtoqe ligtoqe.c cutils.c)
add_custom_command (
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ligatures"
  COMMAND ligtoqe unifont.lig "${CMAKE_CURRENT_BINARY_DIR}/ligatures"
  DEPENDS ligtoqe unifont.lig
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_target(makelig ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/ligatures")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/ligatures" DESTINATION "${CMAKE_INSTALL_DATADIR}/qe")

add_executable (kmaptoqe kmaptoqe.c cutils.c)
file (GLOB kmaps_in kmap/*.kmap)
add_custom_command(
  OUTPUT "${PROJECT_BINARY_DIR}/kmaps"
  COMMAND kmaptoqe "${PROJECT_BINARY_DIR}/kmaps" ${kmaps_in}
  DEPENDS kmaptoqe ${kmaps_in})
add_custom_target(makekmaps ALL DEPENDS "${PROJECT_BINARY_DIR}/kmaps")
install (FILES "${PROJECT_BINARY_DIR}/kmaps" DESTINATION "${CMAKE_INSTALL_DATADIR}/qe")

