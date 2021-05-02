#pragma once

#include "ECS/Component.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>


struct Transform : public Component<Transform>
{
	uint32_t ub_id;

	glm::vec3 pos = glm::vec3(0);
	glm::quat rot = glm::quat(1.0, 0.0, 0.0, 0.0);
	glm::vec3 scale = glm::vec3(1);

	glm::mat4 parentTransform = glm::mat4(1.0f);

	glm::mat4 GetTransform() const
	{
		
		return parentTransform * glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(rot) * glm::scale(glm::mat4(1.0f), scale);
	}
};