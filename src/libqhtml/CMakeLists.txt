

add_executable (csstoqe csstoqe.c)

add_custom_command(
  OUTPUT html_style.c
  COMMAND csstoqe html_style < "${CMAKE_CURRENT_SOURCE_DIR}/html.css" > "${CMAKE_CURRENT_BINARY_DIR}/html_style.c"
  DEPENDS csstoqe
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generate html_style.c"
  SOURCES html.css
)

add_custom_command(
  OUTPUT docbook_style.c
  COMMAND csstoqe docbook_style < "${CMAKE_CURRENT_SOURCE_DIR}/docbook.css" > "${CMAKE_CURRENT_BINARY_DIR}/docbook_style.c"
  DEPENDS csstoqe
  COMMENT "Generate html_style.c"
  SOURCES docbook.css
)

add_library (qhtml css.c xmlparse.c cssparse.c html_style.c docbook_style.c)
target_include_directories (qhtml PRIVATE ${PROJECT_SOURCE_DIR})
