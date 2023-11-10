file(GLOB_RECURSE ENGINE_CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

file(GLOB_RECURSE IMGUI_CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui/*.cpp)
add_library(Engine ${ENGINE_CPP_FILES} ${IMGUI_CPP_FILES})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(glm_DIR C:/glm/cmake/glm/)
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

find_package(Vulkan REQUIRED)

target_compile_definitions(Engine PUBLIC "$<$<CONFIG:DEBUG>:VDEBUG>")
target_precompile_headers(Engine PUBLIC src/pch.h)

target_link_libraries(Engine PUBLIC ${Vulkan_LIBRARY} assimp glfw spdlog spirv-cross-core yaml-cpp)
target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui  ${CMAKE_CURRENT_LIST_DIR}/external/include ${Vulkan_INCLUDE_DIR} ${GLFW_INCLUDE_DIRS} glm)

