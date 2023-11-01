#version 460
#extension GL_GOOGLE_include_directive : require

#include "bindings.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out mat3 TBN;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out vec3 worldPos;
layout(location = 5) out flat int ID;


void main() {
    ID = int(gl_DrawID);
    mat4 model = transformsPtr.m[gl_DrawID];

    vec3 T = normalize(vec3(model * vec4(inTangent, 0.0)));
    vec3 N = normalize(vec3(model * vec4(inNormal, 0.0)));
    // re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    // then retrieve perpendicular vector B with the cross product of T and N
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);

    vec4 tempWorldPos = (model * vec4(inPosition, 1.0));
    fragTexCoord = inTexCoord;
    worldPos = tempWorldPos.xyz;

    gl_Position = viewProj * model * vec4(inPosition, 1.0); // can't just use the tempWorldPos because we loose precision or something and depth tests start to fail
}
