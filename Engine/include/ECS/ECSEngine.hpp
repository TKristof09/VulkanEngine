#pragma once

class EntityManager;
class ComponentManager;
class SystemManager;
class EventHandler;

class ECSEngine
{
public:
	static ECSEngine& GetInstance()
	{
		static ECSEngine   instance;
		return instance;
	}
	ECSEngine(ECSEngine const&) = delete;
	void operator=(ECSEngine const&) = delete;

	ECSEngine();
	~ECSEngine();

	void Update(double dt);
	void FixedUpdate(double dt);

	EntityManager* entityManager;
	ComponentManager* componentManager;
	SystemManager* systemManager;
	EventHandler* eventHandler;

};

