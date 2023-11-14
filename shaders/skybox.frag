#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : require
#include "bindings.glsl"

layout(location = 0) in vec3 eyeDirection;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(texture(cubemapTextures[debugMode], eyeDirection).rgb, 1.0);
}
