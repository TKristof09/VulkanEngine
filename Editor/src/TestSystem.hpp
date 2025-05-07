#pragma once
#include <glm/glm.hpp>

#include "ECS/CoreComponents/Lights.hpp"
#include "ECS/System.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "Utils/Time.hpp"

class TestSystem : public System
{
public:
    struct TestSystemContext
    {
        float distance;
        float speed;
    };
    TestSystem(float distance = 5, float speed = 10)
        : m_distance(distance),
          m_speed(speed)
    {
    }

    void Initialize() override
    {
        AddContextSingleton<TestSystemContext>({m_distance, m_speed});
        StartSystemBuilder<Transform, const TestSystemContext>(
            SystemPhase::OnUpdate,
            [](auto& builder)
            {
                builder
                    .template with<PointLight>()
                    .term_at(2)
                    .singleton()
                    .each(FixedUpdate);
            });
    }

    static void FixedUpdate(flecs::entity e, Transform& transform, const TestSystemContext& ctx)
    {
        float s           = glm::sin((float)Time::GetTime());
        uint64_t id       = e.id();
        uint64_t hashedId = hash(id);

        float angle = glm::two_pi<float>() * (hashedId % 100) / 100.f;


        glm::vec3 direction(glm::cos(angle), 0, glm::sin(angle));
        direction = glm::normalize(direction);

        transform.pos += s * direction * ctx.distance * (float)Time::GetDelta() * ctx.speed;
    }

private:
    const char* GetName() const override { return "TestSystem"; }

    static uint64_t hash(uint64_t x)
    {
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
    }

    float m_distance;
    float m_speed;
};
