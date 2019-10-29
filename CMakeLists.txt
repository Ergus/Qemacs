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


cmake_minimum_required(VERSION 3.0)

project (qemacs VERSION 5.2)

option (ENABLE_TINY "Build a very small version" OFF)
option (ENABLE_GPROF "Add gprof build flags" OFF)
option (ENABLE-ATARI "enable atari cross compile" OFF)
option (ENABLE-WIN32 "enable win32 cross compile" OFF)
option (ENABLE-CYGWIN "compile for cygwin systems" OFF)

include (CMakeDependentOption)
include (GNUInstallDirs)

cmake_dependent_option(ENABLE_X11
  "Enable Xwindow support" ON "NOT ENABLE_TINY" OFF)
cmake_dependent_option(ENABLE_DOC
  "Build documentation" ON "NOT ENABLE_TINY" OFF)
cmake_dependent_option (ENABLE_XV
  "Enable Xvideo extension support" ON "ENABLE_X11" OFF)
cmake_dependent_option (ENABLE_XSHM
  "Enable XShm extension support" ON "ENABLE_X11" OFF)
cmake_dependent_option (ENABLE_XFT
  "Enable Xft extension support" ON "ENABLE_X11" OFF)

cmake_dependent_option (ENABLE_XRENDER
  "Enable Xrender extension support" ON "ENABLE_X11" OFF)

cmake_dependent_option (ENABLE_HTML
  "Disable graphical html support" ON "NOT ENABLE_TINY;NOT WIN32" OFF)

cmake_dependent_option (ENABLE_PNG
  "Enable png support" ON "NOT ENABLE_TINY" OFF)

cmake_dependent_option (ENABLE_PLUGINS
  "Enable plugins support" ON "NOT ENABLE_TINY;NOT WIN32" OFF)

cmake_dependent_option (ENABLE_FFMPEG
  "Enable ffmpeg support" ON "NOT ENABLE_TINY" OFF)

# End command lines

set (CHARSETMORE "${PROJECT_BINARY_DIR}/charsetmore.c")
set (CHARSETJIS "${PROJECT_BINARY_DIR}/charsetjis.def")
set_source_files_properties (${CHARSETMORE} ${CHARSETJIS}
  PROPERTIES GENERATED true)

set (CONFIG_HAS_TYPEOF true)
set (CONFIG_PTSNAME true)

set (QE_VERSION ${PROJECT_VERSION})
set (CONFIG_QE_PREFIX "${CMAKE_INSTALL_PREFIX}")
set (CONFIG_QE_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}")
set (CONFIG_QE_MANDIR "${CMAKE_INSTALL_FULL_MANDIR}")
set (CONFIG_QE_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")

if (CMAKE_BUILD_TYPE STREQUAL Debug)
  set (CONFIG_QE_PLUGINS_DIR "${PROJECT_BINARY_DIR}/plugins")
endif ()

add_definitions (-DHAVE_QE_CONFIG_H)

# Sources

if (ENABLE_TINY)
  set (QE_DEFINITION "CONFIG_TINY")
  set (SOURCES ${SOURCES} parser.c)
else ()
  set (CONFIG_ALL_KMAPS true)
  set (CONFIG_UNICODE_JOIN true)
endif ()

set (CMAKE_C_FLAGS_RELEASE
  "${CMAKE_C_FLAGS_RELEASE} -Wall -O2 -funsigned-char -Wno-format-zero-length")
set (CMAKE_C_FLAGS_DEBUG
  "${CMAKE_C_FLAGS_DEBUG} -Wall -g -O0 -funsigned-char -Wno-format-zero-length")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g")

if (ENABLE_GPROF)
  set (CONFIG_HAVE_GPROF true)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")
endif ()

if (ENABLE_PNG)
  find_package (PNG)
  if (PNG_FOUND)
    set (CONFIG_PNG_OUTPUT true)
  endif ()
endif ()

if (WIN32)
  set (CONFIG_WIN32 true)
  set (CONFIG_INIT_CALLS false)
  set (CONFIG_MMAP false)
  set (CONFIG_UNLOCKIO false)

  if (${CMAKE_SYSTEM_NAME} STREQUAL "CYGWIN")
    set (CONFIG_CYGWIN true)
  endif ()

else ()
  set (CONFIG_INIT_CALLS true)
  set (CONFIG_NETWORK true)
  set (CONFIG_MMAP true)
  set (CONFIG_UNLOCKIO true)
  set (CONFIG_PLUGINS ${ENABLE_PLUGINS})

  set (SOURCES ${SOURCES} unix.c tty.c)
  set (LIBS ${LIBS} m)

  if (NOT ENABLE_TINY)
    set (SOURCES ${SOURCES} shell.c dired.c latex-mode.c archive.c)
  endif ()

  if (APPLE)
    set (CONFIG_DARWIN true)
    set (CONFIG_INIT_CALLS false)
    set (CONFIG_UNLOCKIO true)
    set (CONFIG_PLUGINS false)
  elseif (CMAKE_SYSTEM_NAME STREQUAL Haiku)
    set (CONFIG_HAIKU true)
    set (CONFIG_UNLOCKIO false)
    set (SOURCES ${SOURCES} haiku.cpp)
    set (CONFIG_HTML ${ENABLE_HTML})
    set (ENABLE_X11 false)
    set (LIBS ${LIBS} network be stdc++)
  elseif (CMAKE_SYSTEM_NAME MATCHES "BSD")
    set (CONFIG_BSD true)
    set (CONFIG_UNLOCKIO false)
    set (CONFIG_PTSNAME false)
    set (CONFIG_PLUGINS false)
  endif ()

  if (ENABLE_X11)
    find_package (X11)
    if (X11_FOUND)
      include_directories ("${X11_INCLUDE_DIR}")
      set (CONFIG_HTML ${ENABLE_HTML})
      set (QE_X11 ${X11_LIBRARIES})
      set (QE_DEFINITION "CONFIG_X11")

      if (ENABLE_XV AND  X11_Xv_FOUND)
	set (CONFIG_XV ${X11_Xv_FOUND})
	set (QE_X11 ${QE_X11} ${X11_Xv_LIB})
      endif ()

      if (ENABLE_XSHM AND X11_XShm_FOUND)
	set (CONFIG_XSHM ${X11_XShm_FOUND})
	set (QE_X11 ${QE_X11} ${X11_Xext_LIB})
      endif ()

      if (ENABLE_XRENDER AND X11_Xrender_FOUND)
	set (CONFIG_XRENDER ${X11_Xrender_FOUND})
	set (QE_X11 ${QE_X11} ${X11_Xrender_LIB})
      endif ()

      if (ENABLE_XFT AND X11_Xft_FOUND)
	find_package(Freetype QUIET)
	if (FREETYPE_FOUND)
	  set(CONFIG_XFT TRUE)
	  include_directories("${FREETYPE_INCLUDE_DIRS}")
	  set (QE_X11 ${QE_X11} ${X11_Xft_LIB} ${FREETYPE_LIBRARIES})
	endif ()
      endif()

      if (CONFIG_XFT)
	find_package (PkgConfig REQUIRED)
	pkg_check_modules (GLIB2 glib-2.0 gio-2.0)
	if (GLIB2_FOUND)
	  set(CONFIG_GCONFIG true)
	  include_directories("${GLIB2_INCLUDE_DIRS}")
	  link_directories("${GLIB2_LIBRARY_DIRS}")
	  set (QE_X11 ${QE_X11} ${GLIB2_LIBRARIES})
	endif ()
      endif ()
    endif ()
  endif ()

  if (CONFIG_PLUGINS)
    set (CONFIG_DLL true)
    set (LIBS ${LIBS} ${CMAKE_DL_LIBS})
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")
  endif ()

endif ()

add_subdirectory (src)

# Doc

if (ENABLE_DOC)
  add_subdirectory (doc)
endif ()

install (FILES config.eg DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

# CPack

set (CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "qemacs (qe) - a small editor with some special features")

set (CPACK_PACKAGE_DESCRIPTION
  "QEmacs is an terminal-based text editor (with Emacs look&feel) with
a small size and some unique features:

* Full screen editor with an Emacs compatible key subset (including undo and
  incremental search) and emacs look and feel.
* Can edit huge files (e.g. 100MBytes) without being slow by using a highly
  optimized internal representation and by mmaping the file.
* Full UTF8 support, including double width chars such as ideograms, provided
  you have an UTF8 VT100 emulator such as a recent xterm.
* Bidirectional editing conforming to the Unicode Bidir algorithm (for Hebrew or
  Arabic).
* Can optionally contain input methods from the Yudit editor for most
  languages, including Chinese CJ, Hebrew and Arabic.
* Hexadecimal editing mode with insertion and block commands. Can edit binary
  files as well as text files."
)

set (CPACK_PACKAGE_HOMEPAGE_URL "https://bellard.org/qemacs/")

# RPM

set (CPACK_RPM_PACKAGE_GROUP "Editors")

include(CPack)
