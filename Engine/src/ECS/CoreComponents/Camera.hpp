#pragma once

#include "ECS/Component.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera : public Component<Camera>
{
	float fovRadians;
	float aspect;
	float zNear;
	float zFar;

	Camera(float fovDeg, float aspect, float zNear, float zFar):
	fovRadians(glm::radians(fovDeg)), aspect(aspect), zNear(zNear), zFar(zFar) {}
	glm::mat4 GetProjection() const
	{
		float f = 1.0f / tan(fovRadians / 2.0f);
	    return glm::mat4(
	        f / aspect, 0.0f,  0.0f,  0.0f,
	                  0.0f,    f,  0.0f,  0.0f,
	                  0.0f, 0.0f,  0.0f, 1.0f,
	                  0.0f, 0.0f, zNear,  0.0f);
		//return glm::perspective(glm::radians(fovDegrees), aspect, zNear, zFar);
	}
};
