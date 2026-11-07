# Naming regression coverage; this test does not change any simulation policy.
foreach(kind IN ITEMS suite mixed)
  if(kind STREQUAL "suite")
    set(binary "${MUNKBENCH}")
    set(selector --contact-solver)
    set(args -b FrictionalPyramid --profile smoke --summary-json)
  else()
    set(binary "${MIXEDBENCH}")
    set(selector --solver)
    set(args --scene light --count 8 --steps 8)
  endif()

  execute_process(COMMAND ${EMULATOR} "${binary}" ${args} ${selector} baseline
    RESULT_VARIABLE status OUTPUT_VARIABLE baseline ERROR_VARIABLE error)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "${kind}: baseline failed (${status}): ${error}")
  endif()
  execute_process(COMMAND ${EMULATOR} "${binary}" ${args}
    RESULT_VARIABLE status OUTPUT_VARIABLE default_output ERROR_VARIABLE error)
  # MixedBench includes elapsed time; compare only its deterministic output.
  string(REGEX REPLACE "\"(initSeconds|runSeconds)\":[0-9.eE+-]+" "" baseline "${baseline}")
  string(REGEX REPLACE "\"(initSeconds|runSeconds)\":[0-9.eE+-]+" "" default_output "${default_output}")
  if(NOT status STREQUAL "0" OR NOT baseline STREQUAL default_output)
    message(FATAL_ERROR "${kind}: baseline must match the unchanged default: ${error}")
  endif()
  execute_process(COMMAND ${EMULATOR} "${binary}" ${args} ${selector} original
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT status STREQUAL "2" OR NOT error MATCHES "baseline")
    message(FATAL_ERROR "${kind}: removed selector must fail with baseline guidance (${status}): ${error}")
  endif()
endforeach()
