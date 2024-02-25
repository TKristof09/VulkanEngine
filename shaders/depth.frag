#version 460

layout(location = 0) in vec3 normal;

layout(location = 0) out vec4 outColor;
void main()
{
    outColor = vec4(0.5 * normalize(normal) + 0.5, 1.0);
}
