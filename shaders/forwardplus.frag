#version 460
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#include "bindings.glsl"
#include "common.glsl"

layout(location = 0) in mat3 TBN;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in vec3 worldPos;
layout(location = 5) in vec3 cameraPos;


layout(location = 0) out vec4 outColor;

layout(buffer_reference, buffer_reference_align=4) readonly buffer ShaderData {
    ivec2 viewportSize;
    ivec2 tileNums;
    int debugMode;
    uint64_t lightBuffer;
    uint64_t visibleLightsBuffer;

    uint64_t shadowMapIds;
    uint32_t shadowMapCount;
};

/*
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

    //vec4 cascadeSplits; // TODO change from vec4
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
layout(set = 0, binding = 3) uniform sampler2D shadowMaps[MAX_LIGHTS_PER_TILE * NUM_CASCADES];
layout(set = 0, binding = 4) uniform sampler2DShadow shadowMapsPCF[MAX_LIGHTS_PER_TILE * NUM_CASCADES];

layout(binding = 1, set = 1) uniform sampler2D albedo[32];
//layout(binding = 2, set = 1) uniform sampler2D specular[32];
layout(binding = 2, set = 1) uniform sampler2D normal[32];
*/
#if 0
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
//TODO understand
//calulate the eye space depth from clip space depth
float ZClipToEye(float z, float zNear, float zFar) {
    return zNear * zFar / (zFar - z * (zFar - zNear));
}
float CalculateShadow(Light light)
{

    int cascadeIndex = 0;
    float cascadeSplits[4] = {0.1, 500.0, 1000.0, 1500.0};
    for(int i = 0; i < NUM_CASCADES-1; ++i)
    {
        if(gl_FragCoord.z <= 0.1 / cascadeSplits[i] && gl_FragCoord.z > 0.1 / cascadeSplits[i+1])
        {
            cascadeIndex = i;
            break;
        }
    }
    int slot = light.shadowSlot * NUM_CASCADES + cascadeIndex;
    float lightSize = 0.001; //0.1 / light.lightSpaceMatrices[cascadeIndex][0][0];
    float NEAR_PLANE = light.zPlanes[cascadeIndex].y;
    float FAR_PLANE = light.zPlanes[cascadeIndex].x;

    vec4 lsPos = light.lightSpaceMatrices[cascadeIndex] * vec4(worldPos, 1.0);

    vec3 projectedCoords = (lsPos.xyz / lsPos.w) * vec3(0.5,0.5,1) + vec3(0.5,0.5, 0.005); // bias
    projectedCoords.y = 1 - projectedCoords.y;// not sure why we need the 1-coods.y but seems to work. Maybe beacuse of vulkan's weird y axis?
    /*
    vec4 pos_vs = (light.lightViewMatrices[cascadeIndex] * vec4(worldPos, 1.0));//ZClipToEye(projectedCoords.z, NEAR_PLANE, FAR_PLANE);
    float z_vs = pos_vs.z / pos_vs.w;
    float searchWidth = lightSize * (z_vs - NEAR_PLANE) / z_vs;
    float blockerDist = FindBlockerDistance(projectedCoords, shadowMaps[nonuniformEXT(slot)], searchWidth);
    if(blockerDist == -1)
        return 1;
    float blockerDistVS = ZClipToEye(blockerDist, NEAR_PLANE, FAR_PLANE);
    float penumbraSize =  (z_vs - blockerDistVS)/ blockerDistVS;
    float kernelSize = lightSize * penumbraSize * NEAR_PLANE / z_vs;
    */

    return PCF(projectedCoords, shadowMapsPCF[nonuniformEXT(slot)], 0.001);
}
#endif

vec3 GetNormalFromMap(){
    vec3 n = vec3(0,0,1);//texture(normal[uint(material.textureIndex.x)],fragTexCoord).rgb;
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
    float specularFactor = pow(max(dot(viewDir, reflectDir), 0.0), 1);//material.specularExponent);
    vec3 specularV = specularFactor * light.color /* texture(specular[nonuniformEXT(uint(material.textureIndex.x))], fragTexCoord).rgb*/;

    return diffuse + 0*specularV;
}

vec3 CalculateDirectionalLight(Light light)
{
    return CalculateBaseLight(light, light.direction);//  * CalculateShadow(light);
}
vec3 CalculatePointLight(Light light)
{
    vec3 lightDir = worldPos - light.position;
    float distanceToLight = length(lightDir);

    if(distanceToLight > light.range)
        return vec3(0,0,0);
    lightDir = normalize(lightDir);
    lightDir.xyz;

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

/*
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
    }*/
    vec3 illuminance = vec3(0.2); // ambient
    //            illuminance += CalculateDirectionalLight(lights[0]);
    outColor = vec4(illuminance /** texture(albedo[uint(material.textureIndex.x)], fragTexCoord).rgb*/, 1.0);
    if(cameraPos.x == 0)
        outColor = vec4(1,0,0,1);
    else if(cameraPos.x == 1)
        outColor = vec4(0,1,0,1);
}
