#version 460
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_debug_printf : enable
#include "bindings.glsl"
#include "common.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 worldPos;
layout(location = 3) in flat int ID;

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
    uint metallicnessMap;

    vec3 albedo;
    float roughness;
    float metallicness;
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
    uint shadowMapCount;

    uint irradianceMapIndex;
    uint prefilteredEnvMapIndex;
    uint BRDFLUTIndex;
};

layout(push_constant) uniform PC
{
    mat4 viewProj;
    vec3 cameraPos;
    int debugMode;

    ShaderData shaderDataPtr;
    Transforms transformsPtr; // accessed with objectId
    uint64_t drawIdToObjectIdPtr; // used to access transforms, converts glDrawId to objectId
    MaterialData materialsPtr; // accessed with glDrawId
};

const float HORIZON_FADE_FACTOR = 1.3;

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

// code from http://www.thetenthplanet.de/archives/1180
mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv )
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );

    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, N );
}
vec3 perturb_normal( vec3 N, vec3 V, vec2 texcoord )
{
    vec3 n = textureLod(textures[materialsPtr[ID].normalMap],fragTexCoord, 0.0).xyz; // TODO see TODO in the main function
    n = n * 2.0 - 1.0;
    mat3 TBN = cotangent_frame( N, -V, texcoord );
    return normalize( TBN * n );
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
    ShadowMatrices shadowMatrices = shaderDataPtr.shadowMatricesBuffer.data[light.matricesSlot];
    for(int i = 0; i < NUM_CASCADES; ++i)
    {
        if(gl_FragCoord.z <= shadowMatrices.zPlanes[i].x && gl_FragCoord.z > shadowMatrices.zPlanes[i].y)
        {
            cascadeIndex = i;
            break;
        }
    }

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
        return CalculateShadow(light);


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
vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness)
{

    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CookTorrance(vec3 viewDir, vec3 normal, vec3 F0, vec3 albedo, float metallic, float roughness, uint tileIndex, uint lightNum)
{
    vec3 Lo = vec3(0.0);
    float NdotV = clamp(dot(normal, viewDir), 0.0, 1.0);
    // F = Fresnel factor (Reflectance depending on angle of incidence)


    for(int i = 0; i < lightNum; ++i)
    {
        uint lightIndex = shaderDataPtr.visibleLightsBuffer.data[tileIndex].indices[i];

        Light light = shaderDataPtr.lightBuffer.data[lightIndex];
        vec3 lightDir = light.type == DIRECTIONAL_LIGHT ? -light.direction : (light.position - worldPos);
        lightDir = normalize(lightDir);
        vec3 H = normalize(viewDir + lightDir);
        float NdotH = clamp(dot(normal, H), 0.0, 1.0);
        float NdotL = clamp(dot(normal, lightDir), 0.0, 1.0);
        float VdotH = clamp(dot(viewDir, H), 0.0, 1.0);

        // horizon fade https://marmosetco.tumblr.com/post/81245981087
        float horizonFade = clamp(1.0 + HORIZON_FADE_FACTOR * dot(normalize(inNormal), lightDir), 0.0, 1.0);
        horizonFade *= horizonFade;


        float attenuation = CalculateAttenuation(light);
        if (NdotL > 0.0 && attenuation > 0.0) {
            vec3 F = F_Schlick(VdotH, F0);
            float NDF = NDF_GGX(NdotH, roughness);
            float G = G_SchlickSmithGGX(NdotL, NdotV, roughness);

            vec3 spec = NDF * F * G / (4.0 * NdotL * NdotV + 0.0001);
            vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
            Lo += (kD * albedo * INVPI + spec * horizonFade) * NdotL * attenuation * light.color * light.intensity;
        }
    }
    return Lo;
}

vec3 CalcPrefilteredReflection(vec3 r, float roughness)
{
    const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(cubemapTextures[shaderDataPtr.prefilteredEnvMapIndex], r, lodf).rgb;
	vec3 b = textureLod(cubemapTextures[shaderDataPtr.prefilteredEnvMapIndex], r, lodc).rgb;
	return mix(a, b, lod - lodf);
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

    //vec3 normal = GetNormalFromMap();
    vec3 viewDir = normalize(cameraPos - worldPos);
    MaterialData mat = materialsPtr[ID];
    vec3 normal = mat.normalMap == 0 ? normalize(inNormal) : perturb_normal(normalize(inNormal), viewDir, fragTexCoord);

    float metallic = mat.metallicnessMap == 0 ? mat.metallicness : textureLod(textures[mat.metallicnessMap], fragTexCoord, 0.0).r; // TODO in the future we will stop generating mipmaps for every loaded texture, until then use textureLod 0.0
    float roughness = mat.roughnessMap == 0 ? mat.roughness : textureLod(textures[mat.roughnessMap], fragTexCoord, 0.0).r; // for textures that shouldnt have mips as a workaround
    vec3 albedo = mat.albedoMap == 0 ? pow(mat.albedo, vec3(2.2)) : pow(texture(textures[mat.albedoMap], fragTexCoord).rgb, vec3(2.2));

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = CookTorrance(viewDir, normal, F0, albedo, metallic, roughness, tileIndex, tileLightNum);

    vec3 irradiance = texture(cubemapTextures[shaderDataPtr.irradianceMapIndex], normal).rgb;
    vec3 r = reflect(-viewDir, normal);
    vec3 reflection = CalcPrefilteredReflection(r, roughness);
    vec2 brdf = texture(textures[shaderDataPtr.BRDFLUTIndex], vec2(clamp(dot(normal, viewDir), 0.0, 1.0), 1.0 - roughness)).rg; // use 1-roughness because of vulkan's flipped y uv coordinate


    // ambient lighting
    vec3 F = F_SchlickRoughness(clamp(dot(normal, viewDir), 0.0, 1.0), F0, roughness);
    vec3 kD = 1.0 - F;
    kD *= 1.0 - metallic;
    vec3 diffuse = irradiance * albedo;
    vec3 specular = reflection * (F0 * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse + specular);
    vec3 color = Lo + ambient;

    color = pow(uncharted2_filmic(color), vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
