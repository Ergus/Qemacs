

# Parse the modules.
function (generate_modules output)
  string(REPLACE " " ";" INPUT_LIST ${ARGN})
  message ("-- Generating: ${output}")
  message ("-- For: ${INPUT_LIST}")
  file (WRITE ${output} "/* This file was generated automatically */\n")
  foreach (f ${INPUT_LIST})
    file (STRINGS ${f} VTMP REGEX "^qe_module_init")
    if (VTMP)
      string(REPLACE qe_module_init qe_module_declare VTMP ${VTMP})
      file (APPEND ${output} "${VTMP}\n")
    endif ()
  endforeach ()
endfunction ()

generate_modules (${OUT} "${IN}")
