#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindings.glsl"
#include "common.glsl"

layout(location = 0) in vec3 pos;

layout(location = 0) out vec4 outColor;


// Based omn http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float Random(vec2 co)
{
    float a = 12.9898;
    float b = 78.233;
    float c = 43758.5453;
    float dt= dot(co.xy ,vec2(a,b));
    float sn= mod(dt,3.14);
    return fract(sin(sn) * c);
}

vec2 Hammersley2D(uint i, uint N)
{
    // Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return vec2(float(i) /float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
vec3 ImportanceSample_GGX(vec2 Xi, float roughness, vec3 normal)
{
    // Maps a 2D point to a hemisphere with spread based on roughness
    float alpha = roughness * roughness;
    float phi = 2.0 * PI * Xi.x + Random(normal.xz) * 0.1;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Tangent space
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangentX = normalize(cross(up, normal));
    vec3 tangentY = normalize(cross(normal, tangentX));

    // Convert to world Space
    return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

// Normal Distribution function (Distribution of microfacets that face the correct normal)
float NDF_GGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return (alpha2)/(denom*denom)*INVPI;
}
void main()
{
    // Filtering based on https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
    vec3 normal = normalize(pos);
    vec3 viewDir = normal;
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    float envMapDim = float(textureSize(cubemapTextures[int(data[0])], 0).s);

    float roughness = data[1];

    const uint numSamples = 128; // article says this can be as low as 32
    for(uint i = 0u; i < numSamples; i++) {
        vec2 Xi = Hammersley2D(i, numSamples);
        vec3 H = ImportanceSample_GGX(Xi, roughness, normal);
        vec3 lightDir = 2.0 * dot(viewDir, H) * H - viewDir;
        float NdotL = clamp(dot(normal, lightDir), 0.0, 1.0);
        if(NdotL > 0.0) {

            float NdotH = clamp(dot(normal, H), 0.0, 1.0);
            float VdotH = clamp(dot(viewDir, H), 0.0, 1.0);

            // Probability Distribution Function
            float pdf = NDF_GGX(NdotH, roughness) * NdotH / (4.0 * VdotH) + 0.0001;
            // Solid angle of current smple
            float omegaS = 1.0 / (float(numSamples) * pdf);
            // Solid angle of 1 pixel across all cube faces
            float omegaP = 4.0 * PI / (6.0 * envMapDim * envMapDim);
            // Biased (+1.0) mip level for better result
            float mipLevel = roughness == 0.0 ? 0.0 : max(0.5 * log2(omegaS / omegaP) + 1.0, 0.0f);
            color += textureLod(cubemapTextures[int(data[0])], lightDir, mipLevel).rgb * NdotL;
            totalWeight += NdotL;

        }
    }
    outColor = vec4(color / totalWeight, 1.0);
}
