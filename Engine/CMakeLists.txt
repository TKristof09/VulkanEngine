file(GLOB_RECURSE ENGINE_CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

file(GLOB_RECURSE IMGUI_CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui/*.cpp)
add_library(Engine ${ENGINE_CPP_FILES} ${IMGUI_CPP_FILES})


find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(Assimp REQUIRED)

target_compile_definitions(Engine PUBLIC "$<$<CONFIG:DEBUG>:VDEBUG>")
target_precompile_headers(Engine PUBLIC include/pch.hpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(Engine PUBLIC ${Vulkan_LIBRARY} ${CMAKE_CURRENT_LIST_DIR}/external/lib/assimpd.lib glfw)
	target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui  ${CMAKE_CURRENT_LIST_DIR}/external/include ${Vulkan_INCLUDE_DIR} ${GLM_INCLUDE_DIRS})

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(Engine PUBLIC ${Vulkan_LIBRARY} ${ASSIMP_LIBRARY} glfw spdlog)
	target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui  ${CMAKE_CURRENT_LIST_DIR}/external/include ${Vulkan_INCLUDE_DIR} ${GLFW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS} )
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
