#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;


layout(binding = 0, set = 0) uniform ViewProjMatrix {
    mat4 viewProj;
    vec3 cameraPosition;
};
layout(set = 0, binding = 1, std430) readonly buffer LightBuffer
{
	Light lights[];
};
layout(binding = 0, set = 1) uniform Filler {
    vec4 filler;
};
layout(binding = 0, set = 2) uniform  ModelMatrix {
    mat4 model;
};

layout( push_constant ) uniform PC
{
	uint slot;
};

void main() {
    gl_Position = lights[slot].lightSpaceMatrix * model * vec4(inPosition, 1.0);
}
