#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_multiview : enable

#include "common.glsl"
#include "bindings.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(buffer_reference, std430, buffer_reference_align=4) readonly buffer ShadowMatricesBuffer {
    ShadowMatrices data[];
};

layout(push_constant) uniform PushConstants {
    int lightIndex;
    ShadowMatricesBuffer shadowMatricesBuffer;
    Transforms transformsPtr;
};
void main() {
    gl_Position = shadowMatricesBuffer.data[lightIndex].lightSpaceMatrices[gl_ViewIndex] * transformsPtr.m[gl_DrawID] * vec4(inPosition, 1.0);}
