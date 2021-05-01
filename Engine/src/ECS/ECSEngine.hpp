#pragma once

struct Scene;
class EntityManager;
class ComponentManager;
class SystemManager;

class ECSEngine
{
public:

	ECSEngine(ECSEngine const&) = delete;
	void operator=(ECSEngine const&) = delete;

	ECSEngine(Scene scene);
	~ECSEngine();

	void Update(double dt);
	void FixedUpdate(double dt);

	EntityManager* entityManager;
	ComponentManager* componentManager;
	SystemManager* systemManager;

};

