#define MAX_LIGHTS_PER_TILE 1024
#define TILE_SIZE 16

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Attenuation
{
	// quadratic*x^2+linear*x+constant
	float quadratic;
	float linear;
	float constant;
};
struct Light
{
	int type;
	Attenuation attenuation;

	vec3 color; // use vec3 to have the struct take up 128 bytes
	float intensity;

	vec3 direction; // only for directional or spot
	float cutoff; // only for spot

	vec3 position;
	float range; // only for spot and point

    uint shadowSlot;

};
struct TileLights
{
	uint count;
	uint indices[MAX_LIGHTS_PER_TILE];
};

