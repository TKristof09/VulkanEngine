file(GLOB_RECURSE ENGINE_CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

add_library(Engine ${ENGINE_CPP_FILES} ${IMGUI_CPP_FILES})


# You can link the imgui target to other targets as needed
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(glm_DIR C:/glm/cmake/glm/)
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

file(GLOB_RECURSE IMGUI_CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui/*.cpp)
add_library(imgui STATIC ${IMGUI_CPP_FILES})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)
target_compile_options(imgui PRIVATE /W0)  # Suppress warnings for ImGui files

if(MSVC)
    target_compile_options(Engine PRIVATE /W4 /WX /external:anglebrackets /external:I"${CMAKE_SOURCE_DIR}/external" /external:W0)
else()
  target_compile_options(Engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_compile_definitions(Engine PUBLIC "$<$<CONFIG:DEBUG>:VDEBUG>")
target_precompile_headers(Engine PUBLIC src/pch.h)

target_link_libraries(Engine PUBLIC imgui Vulkan::Vulkan Vulkan::shaderc_combined assimp glfw spdlog spirv-cross-core yaml-cpp flecs::flecs_static)
target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(Engine SYSTEM PUBLIC ${CMAKE_CURRENT_LIST_DIR}/external/include/imgui  ${CMAKE_CURRENT_LIST_DIR}/external/include  ${GLFW_INCLUDE_DIRS} glm flecs)
