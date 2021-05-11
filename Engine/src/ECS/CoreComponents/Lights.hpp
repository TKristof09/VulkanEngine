#pragma once

#include <glm/glm.hpp>
#include "Utils/Color.hpp"
#include "ECS/Component.hpp"

struct DirectionalLight : public Component<DirectionalLight>
{
	Color color;
	float intensity;

	
	uint32_t _slot; //internal
};

struct Attenuation
{
	// quadratic*x^2 + linear*x + constant
	float quadratic;
	float linear;
	float constant;
};

struct PointLight : public Component<PointLight>
{
	Color color;
	float range;
	float intensity;

	Attenuation attenuation;

	
	uint32_t _slot; //internal
};

struct SpotLight : public Component<SpotLight>
{
	Color color;
	float range;
	float intensity;
	float cutoff; // angle in radians

	Attenuation attenuation;

	
	uint32_t _slot; //internal
};
