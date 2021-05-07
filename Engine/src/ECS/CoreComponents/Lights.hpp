#pragma once

#include <glm/glm.hpp>
#include "Utils/Color.hpp"
#include "ECS/Component.hpp"

// We need to declare the lights like this to keep the data aligned with GPU side data
// if we just derive from Component<Light> then the Component adds data members to the light
// struct (such as owner, typeid, componentID, isEnabled)

struct alignas(16) _DirectionalLight
{
	Color color;
	float intensity;

};

struct alignas(16) _PointLight
{
	Color color;
	float radius;
	float intensity;
};

struct DirectionalLight : public Component<DirectionalLight>, _DirectionalLight
{};

struct PointLight : public Component<PointLight>, _PointLight
{};
