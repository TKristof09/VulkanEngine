#include "ECS/ECSEngine.hpp"
#include "ECS/EventHandler.hpp"
#include "ECS/EntityManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/SystemManager.hpp"

ECSEngine::ECSEngine()
{
	m_eventHandler = new EventHandler();
	m_componentManager = new ComponentManager(this);
	m_systemManager = new SystemManager(this);
	m_entityManager = new EntityManager(this);
}

ECSEngine::~ECSEngine()
{
	delete m_entityManager;
	delete m_componentManager;
	delete m_systemManager;
	delete m_eventHandler;
}

void ECSEngine::Update(float dt)
{
	m_systemManager->Update(dt);
}
