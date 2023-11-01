#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;


void main() {
    //gl_Position = lights[slot].lightSpaceMatrices[cascadeIndex] * model * vec4(inPosition, 1.0);
}
