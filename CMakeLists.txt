
cmake_minimum_required(VERSION 3.0)

project (qemacs VERSION 5.2)

option (ENABLE_TINY "Build a very small version" OFF)
option (ENABLE_GPROF "Add gprof build flags" OFF)
option (ENABLE-ATARI "enable atari cross compile" OFF)
option (ENABLE-WIN32 "enable win32 cross compile" OFF)
option (ENABLE-CYGWIN "compile for cygwin systems" OFF)

include (CMakeDependentOption)

cmake_dependent_option(ENABLE_X11
  "Enable Xwindow support" ON "NOT ENABLE_TINY" OFF)
cmake_dependent_option (ENABLE_XV
  "Enable Xvideo extension support" ON "ENABLE_X11" OFF)
cmake_dependent_option (ENABLE_XSHM
  "Enable XShm extension support" ON "ENABLE_X11" OFF)
cmake_dependent_option (ENABLE_XRENDER
  "Enable Xrender extension support" ON "ENABLE_X11" OFF)

cmake_dependent_option (ENABLE_HTML
  "Disable graphical html support" ON "NOT ENABLE_TINY" OFF)

cmake_dependent_option (ENABLE_PNG
  "Enable png support" ON "NOT ENABLE_TINY" OFF)

cmake_dependent_option (ENABLE_PLUGINS
  "Enable plugins support" ON "NOT ENABLE_TINY" OFF)

cmake_dependent_option (ENABLE_FFMPEG
  "Enable ffmpeg support" ON "NOT ENABLE_TINY" OFF)

# End command lines

set (CHARSETMORE "${PROJECT_BINARY_DIR}/charsetmore.c")
set (CHARSETJIS "${PROJECT_BINARY_DIR}/charsetjis.def")
set_source_files_properties (${CHARSETMORE} ${CHARSETJIS}
  PROPERTIES GENERATED true)

include_directories (${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR} libqhtml)

add_subdirectory (libqhtml)
add_subdirectory (cp)

add_definitions (-DHAVE_QE_CONFIG_H)

# Sources

set (SOURCES_COMMON   # objs
  qe.c util.c cutils.c charset.c buffer.c search.c
  input.c display.c hex.c list.c)

set (SOURCES_EXTRA
  qescript.c extras.c variables.c fractal.c charsetjis.c ${CHARSETMORE})


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  SET(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -Wall -g -O2 -funsigned-char -Wno-format-zero-length")
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g")
endif ()

if (ENABLE_GPROF)
  set (CONFIG_HAVE_GPROF TRUE)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")
endif ()

set (CONFIG_HAS_TYPEOF TRUE)
set (CONFIG_PTSNAME TRUE)

set (QE_VERSION ${CMAKE_PROJECT_VERSION})
set (CONFIG_QE_PREFIX ${CMAKE_INSTALL_PREFIX})
set (CONFIG_QE_DATADIR "${CONFIG_QE_DATADIR}/share")
set (CONFIG_QE_MANDIR "${CONFIG_QE_DATADIR}/man")

if (NOT ENABLE_TINY)
  set (CONFIG_ALL_KMAPS TRUE)
  set (SOURCES_EXTRA ${SOURCES_EXTRA} kmap.c)

  set (CONFIG_UNICODE_JOIN TRUE)
  set (SOURCES_EXTRA ${SOURCES_EXTRA}
    unicode_join.c arabic.c indic.c qfribidi.c)

  set (CONFIG_ALL_MODES TRUE)
  set (SOURCES_EXTRA ${SOURCES_EXTRA}
    unihex.c bufed.c clang.c xml.c
    htmlsrc.c forth.c arm.c lisp.c makemode.c
    markdown.c orgmode.c perl.c script.c
    ebnf.c cobol.c rlang.c
    txl.c nim.c rebol.c elm.c jai.c ats.c ${EXTRA_MODES} extra-modes.c)
endif ()

if (WIN32)
  set (CONFIG_WIN32 TRUE)
  set (CONFIG_INIT_CALLS false)
  set (CONFIG_HTML false)
  set (CONFIG_DLL false)
  set (CONFIG_MMAP false)
  set (CONFIG_UNLOCKIO false)

  if (${CMAKE_SYSTEM_NAME} STREQUAL "CYGWIN")
    set (CONFIG_CYGWIN TRUE)
  else ()
    set (TLIBS ${TLIBS} ws2_32)
  endif ()

  set (SOURCES_COMMON ${SOURCES_COMMON} unix.c win32.c)
  set (TLIBS ${TLIBS} msvcrt gdi32 wsock32)

else ()
  set (CONFIG_INIT_CALLS TRUE)
  set (CONFIG_NETWORK TRUE)
  set (CONFIG_DLL TRUE)
  set (CONFIG_MMAP TRUE)
  set (CONFIG_UNLOCKIO TRUE)

  set (SOURCES_COMMON ${SOURCES_COMMON} unix.c tty.c)
  set (TLIBS ${TLIBS} m)

  if (CONFIG_ALL_MODES)
    set (SOURCES_EXTRA ${SOURCES_EXTRA} shell.c dired.c latex-mode.c archive.c)
  endif ()

  if (APPLE)
    set (CONFIG_DARWIN TRUE)
    set (CONFIG_INIT_CALLS false)
    set (CONFIG_DLL false)
    set (CONFIG_UNLOCKIO false)
  elseif (CMAKE_SYSTEM_NAME EQUAL Haiku)
    set (CONFIG_HAIKU TRUE)
    set (CONFIG_HTML TRUE)
    set (SOURCES_EXTRA haiku.cpp)
    set (LIBS ${LIBS} network be stdc++)
  elseif (CMAKE_SYSTEM_NAME MATCHES BSD)
    set (CONFIG_DLL false)
    set (CONFIG_PTSNAME false)
  endif ()

  find_package (X11)
  if (X11_FOUND)
    set (QE_X11 ${X11_LIBRARIES})

    if (X11_Xv_FOUND)
      set (CONFIG_XV ${X11_Xv_FOUND})
      set (QE_X11 ${QE_X11} ${X11_Xv_LIB})
    endif ()

    if (X11_XShm_FOUND)
      set (CONFIG_XSHM ${X11_XShm_FOUND})
      set (QE_X11 ${QE_X11} ${X11_Xext_LIB})
    endif ()

    if (X11_Xrender_FOUND)
      set (CONFIG_XRENDER ${X11_Xrender_FOUND})
      set (QE_X11 ${QE_X11} ${X11_Xrender_LIB})
    endif ()

    set (CONFIG_HTML TRUE)
  endif ()

  if (CONFIG_INIT_CALLS)
    set (QEND qeend.c)
  endif ()

  if (CONFIG_DLL)
    set (LIBS ${LIBS} dl)
  endif ()
endif ()

function (make_modules output)
  set (targetname ${output})
  set (filename "${${output}}")
  add_custom_command (
    OUTPUT "${filename}"
    COMMAND cmake -DOUT=${filename} -DIN=${ARGN} -P cmake-modules.cmake
    DEPENDS ${ARGN}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generate ${filename}"
    )
  add_custom_target (${targetname} DEPENDS ${filename})
endfunction ()

set (MODULES "${CMAKE_CURRENT_BINARY_DIR}/modules.txt")
set (SRC ${SOURCES_COMMON} ${SOURCES_EXTRA} ${QEND})
make_modules (${MODULES} ${SRC})
add_executable (qe "${SRC}")
add_dependencies(qe CHARSET MODULES)
target_link_libraries (qe "${TLIBS}" "${LIBS}")
target_compile_definitions (qe PRIVATE MODULES="${MODULES}")

if (NOT ENABLE_TINY)
  set (TMODULE "${CMAKE_CURRENT_BINARY_DIR}/tmodules.txt")
  set (TSRC ${SOURCES_COMMON} parser.c ${QEND})
  make_modules (${TMODULE} ${TSRC})
  add_executable (tqe ${TSRC})
  add_dependencies(qe TMODULE)
  target_compile_definitions (tqe PRIVATE CONFIG_TINY MODULES="${TMODULE}")
endif ()

if (QE_X11)
  set (XMODULES "${CMAKE_CURRENT_BINARY_DIR}/xmodules.txt")
  set (XSRC ${SOURCES_COMMON} ${SOURCES_EXTRA} x11.c ${QEND})
  make_modules (${XMODULES} ${XSRC})
  add_executable (xqe ${XSRC})
  add_dependencies(xqe CHARSET XMODULES)
  target_compile_definitions (xqe PRIVATE CONFIG_X11 MODULES="${XMODULE}")
  target_link_libraries (xqe ${TLIBS} ${LIBS} ${QE_X11})
endif ()

# Extra files
configure_file (config.h.in config.h)

add_executable (ligtoqe ligtoqe.c cutils.c)
add_custom_command (
  OUTPUT "${PROJECT_BINARY_DIR}/ligatures"
  COMMAND ligtoqe unifont.lig "${PROJECT_BINARY_DIR}/ligatures"
  DEPENDS ligtoqe unifont.lig
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(lig ALL DEPENDS "${PROJECT_BINARY_DIR}/ligatures")

add_executable (kmaptoqe kmaptoqe.c cutils.c)
file (GLOB kmaps_in kmap kmap/*.kmap)
add_custom_target(kmaps
  COMMAND kmaptoqe kmaps ${kmaps_in}
  DEPENDS kmaptoqe ${kmaps_in}
  COMMENT "Generate kmaps"
  SOURCES ${kmaps_in})

find_package (PNG)
if (PNG_FOUND)
  add_executable (fbftoqe fbftoqe.c cutils.c)
  file (GLOB fonts_in fonts fonts/*.fbf)
  add_custom_command(
    OUTPUT fbffonts.c
    COMMAND fbftoqe ${fonts_in} > fbffonts.c
    DEPENDS fbftoqe ${fonts_in}
    COMMENT "Generate fbffonts.c"
    SOURCES ${fonts_in})

  find_package (PNG)

  set (CONFIG_PNG_OUTPUT TRUE)

  set (OBJS1 html2png.c util.c cutils.c
    arabic.c indic.c qfribidi.c display.c unicode_join.c
    charset.c ${CHARSETMORE} charsetjis.c
    libfbf.c fbfrender.c cfb.c fbffonts.c)

  add_executable (html2png ${OBJS1})
  add_dependencies (html2png CHARSET)
  target_link_libraries (html2png qhtml ${PNG_LIBRARIES})
endif ()


find_program (MAKEINFO makeinfo)
if (MAKEINFO)
  function (buildtexi input output)
    set(SOURCE_DOC "${PROJECT_SOURCE_DIR}/${input}")

    add_custom_command(OUTPUT ${output}
      COMMAND ${MAKEINFO} --html --no-split ${SOURCE_DOC}
      MAIN_DEPENDENCY ${SOURCE_DOC})

    add_custom_target("${output}_doc" ALL DEPENDS ${output})
  endfunction ()

  buildtexi (qe-doc.texi qe-doc.html)
else ()
  message ("makeinfo program not available")
endif ()

