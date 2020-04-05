#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

class Camera 
{
public:
	Camera(const float fov, const float aspect, const float zNear, const float zFar):
		m_fov(fov),
		m_aspect(aspect),
		m_zNear(zNear),
		m_zFar(zFar),
		m_position(glm::vec3(0,0,0)),
		m_rotation(glm::quat(1,0,0,0)) //identity quaternion
	{ 

	}


	glm::mat4 GetViewProjection() const
	{
		glm::mat4 pos = glm::translate(m_position);
		glm::mat4 rot = glm::toMat4(glm::conjugate(m_rotation));
		glm::mat4 proj = glm::perspective(glm::radians(m_fov), m_aspect, m_zNear, m_zFar);

		return proj * rot * pos;
	}

	void Rotate(float angle, const glm::vec3& axis)
	{
		m_rotation = glm::rotate(m_rotation, glm::radians(angle), axis);
	}
	void Translate(const glm::vec3& v)
	{
		m_position += v;
	}


private:
	glm::vec3 m_position;
	glm::quat m_rotation;
	float m_fov;
	float m_aspect;
	float m_zNear;
	float m_zFar;
};

