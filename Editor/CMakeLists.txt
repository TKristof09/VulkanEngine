file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
add_executable(Editor ${CPP_FILES})

if(MSVC)
    target_compile_options(Editor PRIVATE /W4 /WX /external:anglebrackets /external:I"${CMAKE_SOURCE_DIR}/external" /external:W0)
else()
  target_compile_options(Editor PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_compile_definitions(Editor PUBLIC "$<$<CONFIG:DEBUG>:VDEBUG>")
target_include_directories(Editor SYSTEM PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src Engine)
target_link_libraries(Editor PUBLIC Engine)
