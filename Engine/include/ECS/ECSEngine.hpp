#pragma once

class EntityManager;
class ComponentManager;
class SystemManager;
class EventHandler;

class ECSEngine
{
public:
	ECSEngine();
	~ECSEngine();

	void Update(float dt);

	EntityManager* m_entityManager;
	ComponentManager* m_componentManager;
	SystemManager* m_systemManager;
	EventHandler* m_eventHandler;
private:

	ECSEngine(const ECSEngine&) = delete;
	ECSEngine& operator=(ECSEngine&) = delete;
};

