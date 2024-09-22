#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#include "bindings.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    Transforms transformsPtr;
    ShaderData shaderDataPtr;
    ObjectIDMap objectIDMap;
};

layout(location = 0) out vec3 outNormal;

SHADER_DATA
{
    mat4 viewProj;
    mat4 view;
};

void main() {
    uint objectID = objectIDMap.data[gl_DrawID + 1];
    mat4 model = transformsPtr.m[objectID];
    outNormal = (shaderDataPtr.view * model * vec4(inNormal, 0.0)).xyz;
    gl_Position = shaderDataPtr.viewProj * model * vec4(inPosition, 1.0);
}

