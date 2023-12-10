#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindings.glsl"

// full screen quad
const vec2 verts[6] =
{
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0),
};


layout(location = 0) out vec3 eyeDirection;

layout(push_constant) uniform PushConstants {
   mat4 viewProj;
   int envMapSlot;
};


void main() {
    mat4 inverseVP = inverse(viewProj);
    eyeDirection = (inverseVP * vec4(verts[gl_VertexIndex], 0.0, 1.0)).xyz;

    gl_Position = vec4(verts[gl_VertexIndex], 0.0, 1.0);
}
