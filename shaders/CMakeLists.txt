set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")


file(GLOB_RECURSE GLSL_HEADER_FILES "*.glsl" )
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "*.frag"
    "*.vert"
    "*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${CMAKE_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL} ${GLSL_HEADER_FILES})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)
add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
    )

add_dependencies(Engine Shaders)

#add_custom_command(TARGET YourMainTarget POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:YourMainTarget>/shaders/"
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#        "${PROJECT_BINARY_DIR}/shaders"
#        "$<TARGET_FILE_DIR:YourMainTarget>/shaders"
#        )
