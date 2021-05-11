#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;


layout(binding = 0, set = 0) uniform ViewProjMatrix {
    mat4 viewProj;
    vec3 cameraPosition;
};
layout(binding = 0, set = 1) uniform Filler {
    vec4 filler;
};
layout(binding = 0, set = 2) uniform  ModelMatrix {
    mat4 model;
};

void main() {
    gl_Position = viewProj * model * vec4(inPosition, 1.0);
}
