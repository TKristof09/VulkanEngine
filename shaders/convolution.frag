#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindings.glsl"
#include "common.glsl"


layout(location = 0) in vec3 pos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    uint envMapSlot;
};

void main()
{
    // the sample direction equals the hemisphere's orientation
    vec3 normal = normalize(pos);

    vec3 irradiance = vec3(0.0);


    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up         = cross(normal, right);

    float sampleDelta = 0.025;
    uint sampleCount = 0;
    for(float phi = 0.0; phi < TWO_PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < HALF_PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += texture(cubemapTextures[envMapSlot], sampleVec).rgb * cos(theta) * sin(theta);
            sampleCount++;
        }
    }
    outColor = vec4(PI * irradiance / float(sampleCount), 1.0);
}
