find_package(Doxygen REQUIRED)

set(DOXYFILE ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
set(DOCS_HTML_DIR ${CMAKE_SOURCE_DIR}/build/html)

add_custom_target(docs
    ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOCS_HTML_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/branding ${DOCS_HTML_DIR}/assets/branding
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
