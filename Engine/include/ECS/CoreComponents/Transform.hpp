#pragma once

#include "ECS/Component.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform : public Component<Transform>
{
	glm::vec3 lPosition;
	glm::quat lRotation;
	glm::vec3 lScale;

	glm::vec3 wPosition;
	glm::quat wRotation;
	glm::vec3 wScale;
};
