#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : require
#include "bindings.glsl"

layout(location = 0) in vec3 eyeDirection;

layout(location = 0) out vec4 outColor;
//
// tonemap from https://64.github.io/tonemapping/
vec3 uncharted2_tonemap_partial(vec3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 uncharted2_filmic(vec3 v)
{
    float exposure_bias = 2.0f;

    vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(vec3(11.2f));
    return uncharted2_tonemap_partial(v * exposure_bias) * white_scale;
}
void main() {
    vec3 color = texture(cubemapTextures[int(data[0])], eyeDirection).rgb;
    outColor = vec4(uncharted2_filmic(color), 1.0);
}
