#pragma once

#include "ECS/Component.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


struct Transform : public Component<Transform>
{
	uint32_t ub_id;

	glm::vec3 lPosition = glm::vec3(0);
	glm::quat lRotation = glm::quat(1.0, 0.0, 0.0, 0.0);
	glm::vec3 lScale = glm::vec3(1);

	glm::vec3 wPosition = glm::vec3(0);
	glm::quat wRotation = glm::quat(1.0, 0.0, 0.0, 0.0);
	glm::vec3 wScale = glm::vec3(1);
};