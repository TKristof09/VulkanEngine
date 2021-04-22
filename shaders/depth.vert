#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;


layout(binding = 0, set = 0) uniform ViewProjMatrix {
    mat4 viewProj;
} vp;
layout(binding = 0, set = 1) uniform Filler {
    vec4 filler;
} filler;
layout(binding = 0, set = 2) uniform  ModelMatrix {
    mat4 model;
} model;

void main() {
    gl_Position = vp.viewProj * model.model * vec4(inPosition, 1.0);
}
