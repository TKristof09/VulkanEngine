file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
add_executable(Application ${CPP_FILES})


target_include_directories(Application PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include ${Vulkan_INCLUDE_DIR} ${GLFW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
target_link_libraries(Application Engine)
