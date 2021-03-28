#include "ECS/ECSEngine.hpp"
#include "ECS/EventHandler.hpp"
#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/SystemManager.hpp"

ECSEngine::ECSEngine()
{
	eventHandler = new EventHandler();
	componentManager = new ComponentManager(this);
	systemManager = new SystemManager(this);
	entityManager = new EntityManager(this);
}

ECSEngine::~ECSEngine()
{
	delete entityManager;
	delete componentManager;
	delete systemManager;
	delete eventHandler;
}

void ECSEngine::Update(float dt)
{
	eventHandler->DispatchEvents();
	componentManager->DestroyRemovedComponents();
	systemManager->Update(dt);
}
