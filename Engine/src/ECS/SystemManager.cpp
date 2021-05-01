#include "ECS/SystemManager.hpp"
#include "ECS/ECSMemoryManager.hpp"
#include "ECS/ECSEngine.hpp"

SystemManager::SystemManager(Scene scene):
m_scene(scene)
{
	m_allocator = new LinearAllocator(ECS_SYSTEM_MEMORY_SIZE, ECSMemoryManager::GetInstance().Allocate(ECS_SYSTEM_MEMORY_SIZE));
}

SystemManager::~SystemManager()
{
	m_registry.clear();
	ECSMemoryManager::GetInstance().Free((void*)m_allocator->GetMemoryStartAddress());
	delete m_allocator;
	m_allocator = nullptr;
}

void SystemManager::Update(double dt)
{
	for (auto& it : m_registry)
	{
		
		ISystem* sys = it.second;
		if(sys->m_enabled)
			sys->PreUpdate(dt);
	}
	for (auto& it : m_registry)
	{
		
		ISystem* sys = it.second;
		if(sys->m_enabled)
			sys->Update(dt);
	}
	for (auto& it : m_registry)
	{
		
		ISystem* sys = it.second;
		if(sys->m_enabled)
			sys->PostUpdate(dt);
	}
}

void SystemManager::FixedUpdate(double dt)
{
	for(auto& it : m_registry)
	{
		ISystem* sys = it.second;
		if(sys->m_enabled)
			sys->FixedUpdate(dt);
	}
}
