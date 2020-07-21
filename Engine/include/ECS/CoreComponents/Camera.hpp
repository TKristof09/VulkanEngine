#pragma once

#include "ECS/Component.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera : public Component<Camera>
{
	float fovDegrees;
	float aspect;
	float zNear;
	float zFar;

	Camera(float fovDeg, float aspect, float zNear, float zFar):
	fovDegrees(fovDeg), aspect(aspect), zNear(zNear), zFar(zFar) {}
	glm::mat4 GetProjection() const
	{
		return glm::perspective(glm::radians(fovDegrees), aspect, zNear, zFar);
	}
};
