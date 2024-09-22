#version 460
#extension GL_GOOGLE_include_directive : require

#include "bindings.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 worldPos;
layout(location = 3) out flat uint ID;


layout(push_constant) uniform PC
{
    mat4 viewProj;
    vec3 cameraPos;
    int debugMode;

    ShaderData shaderDataPtr;
    Transforms transformsPtr; // accessed with objectId
    MaterialData materialsPtr; // accessed with objectid
    ObjectIDMap objectIDMap;
};

void main() {
    ID  = objectIDMap.data[gl_DrawID + 1];
    mat4 model = transformsPtr.m[ID];

    outNormal = normalize(vec3(model * vec4(inNormal, 0.0)));

    fragTexCoord = inTexCoord;
    worldPos = (model * vec4(inPosition, 1.0)).xyz;

    gl_Position = viewProj * model * vec4(inPosition, 1.0); // can't just use the tempWorldPos because we loose precision or something and depth tests start to fail
}
