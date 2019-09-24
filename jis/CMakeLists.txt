set (JIS JIS0208.TXT JIS0212.TXT)

add_executable (jistoqe jistoqe.c ${PROJECT_SOURCE_DIR}/cutils.c)

add_custom_command(
  OUTPUT charsetjis.def
  COMMAND ./jistoqe ${JIS} > "${CMAKE_CURRENT_BINARY_DIR}/charsetjis.def"
  DEPENDS jistoqe ${JIS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generate charsetjis.def"
  SOURCES $(JIS)
  )
