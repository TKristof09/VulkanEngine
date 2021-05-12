#pragma once
#include <glm/glm.hpp>

#include "ECS/ComponentManager.hpp"
#include "ECS/System.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "Utils/Time.hpp"

class TestSystem : public System<TestSystem>
{
public:
	TestSystem(EntityID entity) : m_entity(entity){}
	void FixedUpdate(double dt) override
	{
		m_scene.ecs->componentManager->GetComponent<Transform>(m_entity)->pos = glm::vec3(10,0,0) * (float)glm::sin(Time::GetTime() * 20);		
	}
private:
	EntityID m_entity;
};
