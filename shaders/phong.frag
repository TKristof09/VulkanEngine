#version 450
#extension GL_ARB_separate_shader_objects : enable

#define TILE_SIZE 16
#define NUM_LIGHTS_PER_TILE 1024

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

struct Attenuation
{
	// quadratic*x^2+linear*x+constant
	float quadratic;
	float linear;
	float constant;
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2
struct Light
{
	uint type;

	vec3 color;
	float intensity;
	Attenuation attenuation;

	vec3 direction; //only for directional and spotlights
	float cutoff; //only for spotlights
}

layout(binding = 1) readonly buffer LightBuffer
{
	Light data[];
} lightBuffer;
layout(binding = 2) readonly buffer VisibleLightsBuffer
{
	uint data[];
} visibleLightsBuffer;

layout(binding = 3) uniform UBO
{
	uint numberOfTilesX;
};

void main() {
	ivec2 pixelLoc = ivec2(gl_FragCoord.xy);
	ivec2 tile = pixelLoc / ivec2(TILE_SIZE, TILE_SIZE);
	uint tileIndex = tile.y * numberOfTilesX + tile.x;

	// do texture stuff here

	uint offset = tileIndex * NUM_LIGHTS_PER_TILE;
	for(uint i = 0; i < NUM_LIGHTS_PER_TILE && visibleLightsBuffer.data[offset + i] != -1; i++) //-1 means that there are no more lights affecting that tile, so we dont need to loop more
	{
		//calculate light
		//
		// finalColor += calculatedValue
	}

	// outColor = vec4(finalColor, 1.0f);

    outColor = vec4(1.f, 0.f, 0.f, 1.f);
    //outColor = texture(texSampler, fragTexCoord);
}
