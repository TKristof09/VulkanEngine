#pragma once
#include <glm/glm.hpp>

#include "ECS/ComponentManager.hpp"
#include "ECS/System.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "Utils/Time.hpp"

class TestSystem : public System<TestSystem>
{
public:
	TestSystem(float distance=5, float speed=10)
        : m_distance(distance),
          m_speed(speed)
    {}

	void FixedUpdate(double dt) override
	{
        float s        = glm::sin((float)Time::GetTime());
		for (auto* light : m_scene.ecs->componentManager->GetComponents<PointLight>())
		{
            auto* transform = m_scene.ecs->componentManager->GetComponent<Transform>(light->GetOwner());
            uint64_t id          = light->GetOwner();
            std::hash<uint64_t> hasher;
            uint64_t hashedId = hasher(id);
            float angle     = glm::two_pi<float>() * (id % 10) / 10.f;


            glm::vec3 direction(glm::cos(angle), 0, glm::sin(angle));
            direction = glm::normalize(direction);
            
            transform->pos += s * direction * m_distance * (float)dt * m_speed;
		}
	}

private:
    float m_distance;
    float m_speed;
};
