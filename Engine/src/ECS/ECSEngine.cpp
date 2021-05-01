#include "ECS/ECSEngine.hpp"
#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/SystemManager.hpp"

ECSEngine::ECSEngine(Scene scene)
{
	scene.ecs = this;
	componentManager = new ComponentManager(scene);
	systemManager = new SystemManager(scene);
	entityManager = new EntityManager(scene);
}

ECSEngine::~ECSEngine()
{
	delete entityManager;
	delete componentManager;
	delete systemManager;
}

void ECSEngine::Update(double dt)
{
	entityManager->RemoveDestroyedEntities();
	componentManager->DestroyRemovedComponents();
	
	systemManager->Update(dt);
}

void ECSEngine::FixedUpdate(double dt)
{
	systemManager->FixedUpdate(dt);
}
