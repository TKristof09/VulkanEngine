file(GLOB_RECURSE ENGINE_CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

add_library(Engine ${ENGINE_CPP_FILES})


find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)

target_compile_definitions(Engine PUBLIC "$<$<CONFIG:DEBUG>:VDEBUG>")

target_precompile_headers(Engine PUBLIC include/pch.hpp)

target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/external/include ${Vulkan_INCLUDE_DIR} ${GLFW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
target_link_libraries(Engine ${Vulkan_LIBRARY} ${CMAKE_CURRENT_LIST_DIR}/external/lib/glfw3.lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
find_package(glfw3 REQUIRED)
target_link_libraries(Engine ${Vulkan_LIBRARY} glfw)
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")