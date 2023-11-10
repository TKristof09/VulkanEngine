#pragma once
#include <glm/glm.hpp>

#include "ECS/ComponentManager.hpp"
#include "ECS/System.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "Utils/Time.hpp"

class TestSystem : public System<TestSystem>
{
public:
	TestSystem(){}
	void FixedUpdate(double dt) override
	{
        float distance = 5;
        float speed    = 10;
        float s        = glm::sin((float)Time::GetTime());
		for (auto* light : m_scene.ecs->componentManager->GetComponents<PointLight>())
		{
            auto* transform = m_scene.ecs->componentManager->GetComponent<Transform>(light->GetOwner());
            uint64_t id          = light->GetOwner();
            std::hash<uint64_t> hasher;
            uint64_t hashedId = hasher(id);
            float angle     = glm::pi<float>() * (hashedId % 10) / 10.f;


            float cosSquared  = glm::cos(angle) * glm::cos(angle);
            float sinSquared = glm::sin(angle) * glm::sin(angle);
            glm::vec3 direction(cosSquared, 0, sinSquared);
            
            transform->pos +=  s * direction * distance * (float)dt * speed;
		}
	}

};
