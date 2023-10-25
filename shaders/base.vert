#version 460
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0, set = 0) uniform ViewProjMatrix {
    mat4 viewProj;
};
layout(binding = 0, set = 2) uniform  ModelMatrix {
    mat4 model;
};

layout(binding = 0, set = 1) uniform Material {
    vec4 color;
    vec4 textureIndex;
};

void main() {

    vec4 position = viewProj * model * vec4(inPosition, 1.0);
    gl_Position = position;
    fragTexCoord = inTexCoord;
	fragColor = position.xyz;
}
