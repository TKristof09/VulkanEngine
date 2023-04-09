#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable
#include "common.glsl"

layout(location = 0) in mat3 TBN;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in vec3 worldPos;
layout(location = 5) in vec3 cameraPos;


layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform Material {
    vec4 textureIndex;
    vec4 color;
    float specularExponent;
} material;

layout(push_constant) uniform PC
{
    ivec2 viewportSize;
    ivec2 tileNums;
    int lightNum;
    int debugMode;
};

// we use these buffers later in the frag shader, and they only change once per frame, so they can go with the camera in set 0
layout(set = 0, binding = 1, std430) readonly buffer LightBuffer
{
    Light lights[];
};

layout(set = 0, binding = 2) readonly buffer VisibleLightsBuffer
{
    TileLights visibleLights[];
};
layout(set = 0, binding = 3) uniform sampler2D shadowMaps[MAX_LIGHTS_PER_TILE];
layout(set = 0, binding = 4) uniform sampler2DShadow shadowMapsPCF[MAX_LIGHTS_PER_TILE];

layout(binding = 1, set = 1) uniform sampler2D albedo[32];
//layout(binding = 2, set = 1) uniform sampler2D specular[32];
layout(binding = 2, set = 1) uniform sampler2D normal[32];

float FindBlockerDistance(vec3 coords, sampler2D shadowMap, float radius)
{
    int numBlockers = 0;
    float avgDist = 0.0;

    vec2 uv = coords.xy;
    for(int i = 0; i < BLOCKER_SAMPLES; ++i)
    {
        float depth = texture(shadowMap, uv + Poisson64[i] * radius).x;
        if(coords.z < depth)
        {
            numBlockers++;
            avgDist += depth;
        }
    }
    return numBlockers > 0 ? avgDist / numBlockers : -1;
}
float PCF(vec3 coords, sampler2DShadow shadowMapPCF, float radius)
{
    float sum = 0.0;
    for(int i = 0; i < PCF_SAMPLES; ++i)
    {
        vec3 offset = vec3(Poisson64[i] * radius, 0);
        sum += texture(shadowMapPCF, coords + offset);
    }
    return sum / PCF_SAMPLES;
}
float CalculateShadow(Light light)
{

    float lightSize = 0.1;
    float NEAR_PLANE = 0.1;

    vec4 lsPos = light.lightSpaceMatrix * vec4(worldPos, 1.0);

    vec3 projectedCoords = (lsPos.xyz / lsPos.w) * vec3(0.5,0.5,1) + vec3(0.5,0.5, 0.005); // bias
    projectedCoords.y = 1 - projectedCoords.y;// not sure why we need the 1-coods.y but seems to work. Maybe beacuse of vulkan's weird y axis?

    float searchWidth = lightSize * (projectedCoords.z - NEAR_PLANE) / projectedCoords.z;
    float blockerDist = FindBlockerDistance(projectedCoords, shadowMaps[nonuniformEXT(light.shadowSlot)], 0.01);
    if(blockerDist == -1)
        return 1;

    float penumbraSize = (projectedCoords.z - blockerDist)  / blockerDist;
    float kernelSize = penumbraSize * lightSize * NEAR_PLANE / projectedCoords.z;

    return PCF(projectedCoords, shadowMapsPCF[nonuniformEXT(light.shadowSlot)], kernelSize);
}

vec3 GetNormalFromMap(){
    vec3 n = texture(normal[uint(material.textureIndex.x)],fragTexCoord).rgb;
    n = normalize(n * 2.0 - 1.0); //transform from [0,1] to [-1,1]
    n = normalize(TBN * n);
    return n;
}
vec3 CalculateBaseLight(Light light, vec3 direction)
{
    direction = normalize(-direction);


    // diffuse
    vec3 norm = GetNormalFromMap();
    float angle = max(dot(norm, direction), 0.0);
    vec3 diffuse = angle * light.color.xyz * light.intensity;

    // specular
    vec3 viewDir = normalize(cameraPos - worldPos);
    vec3 reflectDir = reflect(-direction, norm);
    float specularFactor = pow(max(dot(viewDir, reflectDir), 0.0), material.specularExponent);
    vec3 specularV = specularFactor * light.color /* texture(specular[nonuniformEXT(uint(material.textureIndex.x))], fragTexCoord).rgb*/;

    return diffuse + 0*specularV;
}

vec3 CalculateDirectionalLight(Light light)
{
    return CalculateBaseLight(light, light.direction)  * CalculateShadow(light);
}
vec3 CalculatePointLight(Light light)
{
    vec3 lightDir = worldPos - light.position;
    float distanceToLight = length(lightDir);

    if(distanceToLight > light.range)
        return vec3(0,0,0);
    lightDir = normalize(lightDir);

    float attenuation = 1 / (light.attenuation.quadratic * distanceToLight * distanceToLight +
            light.attenuation.linear * distanceToLight +
            light.attenuation.constant + 0.00001);
    // +0.00001 to prevent division with 0

    return CalculateBaseLight(light, lightDir) * attenuation;
}
vec3 CalculateSpotLight(Light light)
{
    vec3 lightDir = normalize(worldPos - light.position);
    float angle = dot(lightDir, normalize(light.direction));
    return  int(angle > light.cutoff) * CalculatePointLight(light) * (1.0 -(1.0 - angle) / (1.0 - light.cutoff));
}

void main() {

    ivec2 tileID = ivec2(gl_FragCoord.xy / TILE_SIZE);
    int tileIndex = tileID.y * tileNums.x + tileID.x;

    uint tileLightNum = visibleLights[tileIndex].count;

    vec3 illuminance = vec3(0.2); // ambient

    for(int i = 0; i < tileLightNum; ++i)
    {
        uint lightIndex = visibleLights[tileIndex].indices[i];

        switch(lights[lightIndex].type)
        {
            case DIRECTIONAL_LIGHT:
                illuminance += CalculateDirectionalLight(lights[lightIndex]);
                break;
            case POINT_LIGHT:
                illuminance += CalculatePointLight(lights[lightIndex]);
                break;
            case SPOT_LIGHT:
                illuminance += CalculateSpotLight(lights[lightIndex]);
                break;
        }
    }

    outColor = vec4(illuminance * vec3(1.0,0,0) /* texture(albedo[uint(material.textureIndex.x)], fragTexCoord).rgb*/, 1.0);
}
