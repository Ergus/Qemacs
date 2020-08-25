# Copyright (C) 2019  Jimmy Aguilar Mena

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 3 of the
# License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

include_directories ("${PROJECT_BINARY_DIR}"
  "${CMAKE_BINARY_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src"
  )

set (SOURCES "")

foreach(file ${TINY_SOURCES})
  get_filename_component(fpath ${file} DIRECTORY)
  if ("${fpath}" STREQUAL "")
    list(APPEND SOURCES "${PROJECT_SOURCE_DIR}/src/${file}")
  else()
    list(APPEND SOURCES "${file}")
  endif()
endforeach()

add_executable (test_container test_container.c )
target_link_libraries(test_container lqemacs)

add_test(NAME Test_Container COMMAND test_container)
