#include "ECS/ECSEngine.hpp"

ECSEngine::ECSEngine()
{
	m_systemManager = new SystemManager();
	m_componentManager = new ComponentManager();
	m_entityManager = new EntityManager(m_componentManager);
}

ECSEngine::~ECSEngine()
{
	delete m_entityManager;
	delete m_componentManager;
	delete m_systemManager;
}

void ECSEngine::Update(float dt)
{
	m_systemManager->Update(dt);
}
