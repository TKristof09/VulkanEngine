#version 460
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#include "bindings.glsl"
#include "common.glsl"

layout(location = 0) in mat3 TBN;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in vec3 worldPos;
layout(location = 5) in flat int ID;


layout(location = 0) out vec4 outColor;

layout(buffer_reference, std430, buffer_reference_align=4) readonly buffer LightBuffer {
    Light data[];
};
layout(buffer_reference, std430, buffer_reference_align=4) readonly buffer VisibleLightsBuffer {
    TileLights data[];
};

layout(buffer_reference, std430, buffer_reference_align=4) readonly buffer MaterialData
{
    uint albedoMap;
    uint normalMap;
    uint roughnessMap;
    uint metalicnessMap;
};
layout(buffer_reference, std430, buffer_reference_align=4) readonly buffer ShadowMapIndices
{
    int id;
};
layout(buffer_reference, std430, buffer_reference_align=4) readonly buffer ShadowMatricesBuffer {
    ShadowMatrices data[];
};

layout(buffer_reference, std430, buffer_reference_align=4) readonly buffer ShaderData {
    ivec2 viewportSize;
    ivec2 tileNums;
    LightBuffer lightBuffer;
    VisibleLightsBuffer visibleLightsBuffer;
    ShadowMatricesBuffer shadowMatricesBuffer;

    ShadowMapIndices shadowMapIds;
    uint32_t shadowMapCount;
};

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
    vec3 curr = uncharted2_tonemap_partial(v * exposure_bias);

    vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(vec3(11.2f));
    return curr * white_scale;
}



float PCF(vec3 coords, int slot, int cascadeIndex, float radius)
{
    float sum = 0.0;
    const vec4 p = vec4(coords.xy, cascadeIndex, coords.z);
    for(int i = 0; i < PCF_SAMPLES; ++i)
    {
        vec4 pOffset = vec4(Poisson64[i] * radius, 0.0, 0.0);
        sum += texture(shadowTextures[slot], p + pOffset); // TODO try to use textureOffset
    }
    return sum / PCF_SAMPLES;
}

float CalculateShadow(Light light)
{
    int cascadeIndex = 0;
    const float cascadeSplits[4] = {0.1, 500.0, 1000.0, 1500.0};
    for(int i = 0; i < NUM_CASCADES-1; ++i)
    {
        if(gl_FragCoord.z <= 0.1 / cascadeSplits[i] && gl_FragCoord.z > 0.1 / cascadeSplits[i+1])
        {
            cascadeIndex = i;
            break;
        }
    }
    ShadowMatrices shadowMatrices = shaderDataPtr.shadowMatricesBuffer.data[light.matricesSlot];

    //float lightSize = 0.001; //0.1 / light.lightSpaceMatrices[cascadeIndex][0][0];
    //float NEAR_PLANE = shadowMatrices.zPlanes[cascadeIndex].y;
    //float FAR_PLANE = shadowMatrices.zPlanes[cascadeIndex].x;

    vec4 lsPos = shadowMatrices.lightSpaceMatrices[cascadeIndex] * vec4(worldPos, 1.0);

    vec3 projectedCoords = (lsPos.xyz / lsPos.w) * vec3(0.5,0.5,1) + vec3(0.5,0.5, 0.005); // bias
    projectedCoords.y = 1 - projectedCoords.y;// not sure why we need the 1-coods.y but seems to work. Maybe beacuse of vulkan's weird y axis?


    return PCF(projectedCoords, shaderDataPtr.shadowMapIds[light.shadowSlot].id, cascadeIndex, 0.001);
}



float CalculateAttenuation(Light light)
{
    if(light.type == DIRECTIONAL_LIGHT)
        return 1.0;

    vec3 lightDir = worldPos - light.position;
    float distanceToLight = length(lightDir);

    if(distanceToLight > light.range)
        return 0.0;

    lightDir = lightDir / distanceToLight;

    float attenuation = 1 / (light.attenuation.quadratic * distanceToLight * distanceToLight +
            light.attenuation.linear * distanceToLight +
            light.attenuation.constant + 0.00001);

    if(light.type == POINT_LIGHT)
        return attenuation;


    // SPOT_LIGHT TODO: make the spotlight's edges not be instant but smooth
    float angle = dot(lightDir, normalize(-light.direction));
    return angle > light.cutoff ? attenuation * (1.0 -(1.0 - angle) / (1.0 - light.cutoff)) : 0.0;


}

// Normal Distribution function (Distribution of microfacets that face the correct normal)
float NDF_GGX(float NdotH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(denom*denom)*INVPI;
}

// Geometric Shadowing function (Microfacet shadowing and masking)
float G_SchlickSmithGGX(float NdotL, float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	return NdotL / (NdotL * (1.0 - k) + k) * NdotV / (NdotV * (1.0 - k) + k);
}

// Fresnel function
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


vec3 CookTorrance(vec3 viewDir, vec3 normal, vec3 F0, vec3 albedo, float metallic, float roughness, uint tileIndex, uint lightNum)
{
    vec3 Lo = vec3(0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    // F = Fresnel factor (Reflectance depending on angle of incidence)
    vec3 F = F_Schlick(NdotV, F0);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);


    for(int i = 0; i < lightNum; ++i)
    {
        uint lightIndex = shaderDataPtr.visibleLightsBuffer.data[tileIndex].indices[i];

        Light light = shaderDataPtr.lightBuffer.data[lightIndex];


        vec3 lightDir = light.type == DIRECTIONAL_LIGHT ? -light.direction : (light.position - worldPos);
        lightDir = normalize(lightDir);
        vec3 H = normalize(viewDir + lightDir);
        float NdotH = max(dot(normal, H), 0.0);
        float NdotL = max(dot(normal, lightDir), 0.0);


        float attenuation = CalculateAttenuation(light);
        if (NdotL > 0.0 && attenuation > 0.0) {
            float NDF = NDF_GGX(NdotH, roughness);
            float G = G_SchlickSmithGGX(NdotL, NdotV, roughness);

            vec3 spec = NDF * F * G / (4.0 * NdotL * NdotV + 0.000001);
            Lo += (kD * albedo * INVPI + spec) * NdotL * attenuation * light.color * light.intensity;
        }
    }
    return Lo;
}
vec3 GetNormalFromMap(){
    vec3 n = texture(textures[materialsPtr[ID].normalMap],fragTexCoord).rgb;
    n = normalize(n * 2.0 - 1.0); //transform from [0,1] to [-1,1]
    n = normalize(TBN * n);
    return n;
}
void main() {
    ivec2 tileID = ivec2(gl_FragCoord.xy / TILE_SIZE);
    uint tileIndex = tileID.y * shaderDataPtr.tileNums.x + tileID.x;

    uint tileLightNum = shaderDataPtr.visibleLightsBuffer.data[tileIndex].count;

    /* TODO
    if(tileLightNum == 0)
    {
        outColor = vec4(0.0);
        return;
    }
    */

    vec3 normal = GetNormalFromMap();
    vec3 viewDir = normalize(cameraPos - worldPos);

    float metallic = texture(textures[materialsPtr[ID].metalicnessMap], fragTexCoord).r;
    float roughness = texture(textures[materialsPtr[ID].roughnessMap], fragTexCoord).r;

    vec3 albedo = pow(texture(textures[materialsPtr[ID].albedoMap], fragTexCoord).rgb, vec3(2.2));

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = CookTorrance(viewDir, normal, F0, albedo, metallic, roughness, tileIndex, tileLightNum);

    Lo += vec3(0.03) * albedo;

    Lo = pow(uncharted2_filmic(Lo), vec3(1.0/2.2));


    outColor = vec4(Lo, 1.0);
}
