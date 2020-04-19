#pragma once
#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/SystemManager.hpp"

class ECSEngine
{
public:
	ECSEngine();
	~ECSEngine();

	void Update(float dt);

private:
	EntityManager* m_entityManager;
	ComponentManager* m_componentManager;
	SystemManager* m_systemManager;

	ECSEngine(const ECSEngine&) = delete;
	ECSEngine& operator=(ECSEngine&) = delete;
};

