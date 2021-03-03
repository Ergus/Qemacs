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

# Documentation

install (FILES qe.1 DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

find_program (MAKEINFO makeinfo)
if (MAKEINFO)
  function (buildtexi input output)
    set(SOURCE_DOC "${CMAKE_CURRENT_SOURCE_DIR}/${input}")

    add_custom_command(
      OUTPUT ${output}
      COMMAND ${MAKEINFO} --html --no-split ${SOURCE_DOC}
      MAIN_DEPENDENCY ${SOURCE_DOC})

    add_custom_target("${output}_doc" ALL DEPENDS ${output})
  endfunction ()

  buildtexi (qe-doc.texi qe-doc.html)
else ()
  message ("makeinfo program not available")
endif ()
