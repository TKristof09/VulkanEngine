cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
project(VulkanEngine VERSION 0.1.0)


set( CMAKE_EXPORT_COMPILE_COMMANDS ON)
set( CMAKE_CXX_STANDARD 20)
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}/bin)
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}/lib)
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}/lib)

set (CMAKE_VS_JUST_MY_CODE_DEBUGGING ON)



add_subdirectory(${CMAKE_SOURCE_DIR}/external/glm)
#add_subdirectory(${CMAKE_SOURCE_DIR}/external/assimp)
#SPIRV CROSS
add_subdirectory(${CMAKE_SOURCE_DIR}/external/Vulkan-Utility-Libraries)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/SPIRV-Cross)
set(SPIRV_CROSS_CLI CACHE OFF FORCE)
set(SPIRV_CROSS_ENABLE_TESTS CACHE OFF FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/spdlog)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/glfw)

set(ASSIMP_BUILD_TESTS CACHE OFF FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/assimp)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/yaml-cpp)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/flecs)


add_subdirectory(${CMAKE_SOURCE_DIR}/Engine)
add_subdirectory(${CMAKE_SOURCE_DIR}/shaders)
add_subdirectory(${CMAKE_SOURCE_DIR}/Editor)

#add_subdirectory(${CMAKE_SOURCE_DIR}/external/glfw)

set_property(TARGET Engine PROPERTY CXX_STANDARD 20)
set_property(TARGET Engine PROPERTY CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set_property(TARGET Engine PROPERTY LINK_FLAGS "/PROFILE")
    set_property(TARGET Editor PROPERTY LINK_FLAGS "/PROFILE")
endif()


