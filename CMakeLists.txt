
cmake_minimum_required(VERSION 3.0)

project (qemacs VERSION 5.2)

option (BUILD_ALL "Build all the targets" OFF)

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

# End commands

set (SOURCES_COMMON   # objs
  qe.c util.c cutils.c charset.c buffer.c search.c
  input.c display.c hex.c list.c)

set (SOURCES_EXTRA
  qescript.c extras.c variables.c fractal.c charsetjis.c charsetmore.c)

add_definitions (-DHAVE_QE_CONFIG_H)
include_directories (${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR} libqhtml)
add_subdirectory (libqhtml)

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


if (CMAKE_SYSTEM_PROCESSOR)
  set (WORDS_BIGENDIAN TRUE)
endif()

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

# Parse the modules.
function (make_modules output)
  if (NOT EXISTS ${output})
    set (GENERATE true)
  else ()
    foreach (f ${ARGN})
      if (${f} IS_NEWER_THAN ${output})
	set (GENERATE true)
	break ()
      endif ()
    endforeach ()
  endif ()

  if (GENERATE)
    message ("-- Generating: ${output}")
    file (WRITE ${output} "/* This file was generated automatically */\n")
    foreach (f ${ARGN})
      file (STRINGS ${f} VTMP REGEX "^qe_module_init")
      if (VTMP)
	string(REPLACE qe_module_init qe_module_declare VTMP ${VTMP})
	file (APPEND ${output} "${VTMP}\n")
      endif ()
    endforeach ()
  endif ()
endfunction ()

set (MODULES "${CMAKE_CURRENT_BINARY_DIR}/modules.txt")
set (SRC ${SOURCES_COMMON} ${SOURCES_EXTRA} ${QEND})
make_modules (${MODULES} ${SRC})
add_executable (qe "${SRC}")
target_link_libraries (qe "${TLIBS}" "${LIBS}")
target_compile_definitions (qe PRIVATE MODULES="${MODULES}")

if (NOT ENABLE_TINY)
  set (TMODULE "${CMAKE_CURRENT_BINARY_DIR}/tmodules.txt")
  set (TSRC ${SOURCES_COMMON} parser.c ${QEND})
  make_modules (${TMODULE} ${TSRC})
  add_executable (tqe ${TSRC})
  target_compile_definitions (tqe PRIVATE CONFIG_TINY)
  target_compile_definitions (tqe PRIVATE MODULES="${TMODULE}")
endif ()

if (QE_X11)
  set (XMODULES "${CMAKE_CURRENT_BINARY_DIR}/xmodules.txt")
  set (XSRC ${SOURCES_COMMON} ${SOURCES_EXTRA} x11.c ${QEND})
  make_modules (${XMODULES} ${XSRC})
  add_executable (xqe ${XSRC})
  target_compile_definitions (xqe PRIVATE CONFIG_X11)
  target_link_libraries (xqe ${TLIBS} ${LIBS} ${QE_X11})
  target_compile_definitions (xqe PRIVATE MODULES="${XMODULE}")

endif ()

# Extra files

configure_file (config.h.in config.h)

add_executable (ligtoqe ligtoqe.c cutils.c)
add_custom_target(ligatures
  COMMAND ligtoqe unifont.lig ligatures
  DEPENDS ligtoqe unifont.lig
  COMMENT "Generate ligatures"
  SOURCES unifont.lig)

add_executable (kmaptoqe kmaptoqe.c cutils.c)
file (GLOB kmaps_in kmap kmap/*.kmap)
add_custom_target(kmaps
  COMMAND kmaptoqe kmaps ${kmaps_in}
  DEPENDS kmaptoqe ${kmaps_in}
  COMMENT "Generate kmaps"
  SOURCES ${kmaps_in})

add_executable (cptoqe cptoqe.c cutils.c)
add_executable (jistoqe jistoqe.c cutils.c)

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
    charset.c charsetmore.c charsetjis.c
    libfbf.c fbfrender.c cfb.c fbffonts.c)

  add_executable (html2png ${OBJS1})
  target_link_libraries (html2png qhtml ${PNG_LIBRARIES})
endif ()

if (BUILD_ALL)
  set (CP 8859-2.TXT   8859-3.TXT   8859-4.TXT   8859-5.TXT   8859-6.TXT
    8859-7.TXT   8859-8.TXT   8859-9.TXT   8859-10.TXT  8859-11.TXT
    8859-13.TXT  8859-14.TXT  8859-15.TXT  8859-16.TXT
    CP437.TXT    CP737.TXT    CP850.TXT    CP852.TXT    CP866.TXT
    CP1125.TXT   CP1250.TXT   CP1251.TXT   CP1252.TXT   CP1256.TXT
    CP1257.TXT   MAC-LATIN2.TXT MAC-ROMAN.TXT
    kamen.cp     KOI8-R.TXT   koi8_u.cp    TCVN.TXT     VISCII.TXT
    CP037.TXT    CP424.TXT    CP500.TXT    CP875.TXT    CP1026.TXT
    ATARIST.TXT)

endif ()

find_program (MAKEINFO makeinfo)
if (TEXI2HTML)
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

