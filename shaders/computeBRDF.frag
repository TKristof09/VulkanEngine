#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec2 uv;

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

// Geometric Shadowing function (Microfacet shadowing and masking)
float G_SchlickSmithGGX(float NdotL, float NdotV, float roughness)
{
    float k = (roughness*roughness) / 2.0;
    return NdotL / (NdotL * (1.0 - k) + k) * NdotV / (NdotV * (1.0 - k) + k);
}

void main()
{
    // Normal always points along z-axis for the 2D lookup
	const vec3 normal = vec3(0.0, 0.0, 1.0);

    float NdotV = max(uv.x, 0.001); // to avoid divisions by zero
    float roughness = uv.y;

	vec3 viewDir = vec3(sqrt(1.0 - NdotV*NdotV), 0.0, NdotV);


	vec2 LUT = vec2(0.0);
    const uint numSamples = 1024;
    for(uint i = 0u; i < numSamples; i++) {
		vec2 Xi = Hammersley2D(i, numSamples);
		vec3 H = ImportanceSample_GGX(Xi, roughness, normal);
		vec3 lightDir = 2.0 * dot(viewDir, H) * H - viewDir;

		float NdotL = max(dot(normal, lightDir), 0.0);
		// float NdotV = max(dot(normal, viewDir), 0.0); this is useless calculation
		float VdotH = max(dot(viewDir, H), 0.0);
		float NdotH = max(dot(normal, H), 0.0);

		if (NdotL > 0.0) {
			float G = G_SchlickSmithGGX(NdotL, NdotV, roughness);
			float G_Vis = (G * VdotH) / (NdotH * NdotV);
			float Fc = pow(1.0 - VdotH, 5.0);
			LUT += vec2((1.0 - Fc) * G_Vis, Fc * G_Vis);
		}
	}
	outColor = vec4(LUT / float(numSamples), 0.0, 1.0);
}
