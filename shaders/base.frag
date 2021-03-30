#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 1) uniform Material {
    vec4 color;
    vec4 textureIndex;
} material;

layout(binding = 1, set = 1) uniform sampler2D albedo[32];
void main() {
    //outColor = material.color;
    outColor = texture(albedo[nonuniformEXT(uint(material.textureIndex.x))], fragTexCoord);
}
