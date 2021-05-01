file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
add_executable(Editor ${CPP_FILES})

target_compile_definitions(Editor PUBLIC "$<$<CONFIG:DEBUG>:VDEBUG>")
target_include_directories(Editor PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src ${Vulkan_INCLUDE_DIR} ${GLFW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
target_link_libraries(Editor PUBLIC Engine)
