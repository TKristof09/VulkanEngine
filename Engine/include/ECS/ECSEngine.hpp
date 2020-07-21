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

	EntityManager* entityManager;
	ComponentManager* componentManager;
	SystemManager* systemManager;
	EventHandler* eventHandler;
private:

	ECSEngine(const ECSEngine&) = delete;
	ECSEngine& operator=(ECSEngine&) = delete;
};

