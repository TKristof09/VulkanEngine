#pragma once
#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/SystemManager.hpp"
#include "ECS/EventHandler.hpp"

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
	EventHandler* m_eventHandler;

	ECSEngine(const ECSEngine&) = delete;
	ECSEngine& operator=(ECSEngine&) = delete;
};

