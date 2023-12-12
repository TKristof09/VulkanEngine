#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


struct Transform
{
	glm::vec3 pos = glm::vec3(0);
	glm::quat rot = glm::quat(1.0, 0.0, 0.0, 0.0);
	glm::vec3 scale = glm::vec3(1);
};
