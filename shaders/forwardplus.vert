#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out mat3 TBN;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out vec3 worldPos;
layout(location = 5) out vec3 cameraPos;

layout(binding = 0, set = 0) uniform ViewProjMatrix {
    mat4 viewProj;
	vec3 cameraPosition;
};
layout(binding = 0, set = 2) uniform  ModelMatrix {
    mat4 model;
};

void main() {
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
	cameraPos = cameraPosition;

    gl_Position = viewProj * model * vec4(inPosition, 1.0);
}