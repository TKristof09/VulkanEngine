#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0, set = 0) uniform ViewProjMatrix {
    mat4 viewProj;
} vp;
layout(binding = 0, set = 2) uniform  ModelMatrix {
    mat4 model;
} model;

void main() {
    gl_Position = vp.viewProj * model.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
	fragColor = (vec4(inPosition + 0.1f, 1.0f)).rgb;
}
