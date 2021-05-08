#pragma once

#include <glm/glm.hpp>
#include "Utils/Color.hpp"
#include "ECS/Component.hpp"

struct DirectionalLight : public Component<DirectionalLight>
{
	Color color;
	float intensity;
};

struct PointLight : public Component<PointLight>
{
	Color color;
	float radius;
	float intensity;
};
