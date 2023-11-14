#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#include "bindings.glsl"

layout(location = 0) in vec3 pos;

layout(location = 0) out vec4 outColor;



const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
  vec2 uv = SampleSphericalMap(normalize(pos));
  uv.y = 1.0 - uv.y;
  outColor = vec4(texture(textures[debugMode], uv).rgb, 1.0);
}

