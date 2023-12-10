#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#include "bindings.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    Transforms transformsPtr;
};

void main() {
    mat4 model = transformsPtr.m[gl_DrawID];
    gl_Position = viewProj * model * vec4(inPosition, 1.0);
}

