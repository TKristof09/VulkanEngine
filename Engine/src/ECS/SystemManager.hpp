#pragma once

#include "Memory/LinearAllocator.hpp"
#include "ECS/Types.hpp"
#include "ECS/ISystem.hpp"
#include "ECS/ECSEngine.hpp"
#include "ECS/CoreEvents/SystemEvents.hpp"

#include <unordered_map>

#define ECS_SYSTEM_MEMORY_SIZE 8388608 // 8MB
//TODO system dependecies

class SystemManager
{
private:
	LinearAllocator* m_allocator;
	std::unordered_map<SystemTypeID, ISystem*> m_registry;
	SystemManager(const SystemManager&) = delete;
	SystemManager& operator=(SystemManager&) = delete;

	ECSEngine* m_ecsEngine;
public:
	SystemManager(ECSEngine* ecsEngine);
	~SystemManager();

	void Update(double dt);
	void FixedUpdate(double dt);

	template<typename T, typename... Args>
	T* AddSystem(Args... args)
	{
		SystemTypeID id = T::STATIC_SYSTEM_TYPE_ID;

		auto it = m_registry.find(id);
		if(it != m_registry.end())
			return (T*)it->second;



		void* pMemory = m_allocator->Allocate(sizeof(T), alignof(T));
		((T*)pMemory)->m_ecsEngine = m_ecsEngine;

		ISystem* system = new(pMemory) T(std::forward<Args>(args)...);
		system->m_enabled = true;


		m_registry[id] = system;

		SystemAdded<T> e;
		e.ptr = (T*)system;
		m_ecsEngine->eventHandler->Send<SystemAdded<T>>(e);

		return (T*)system;
	}

	template<typename T>
	T* GetSystem() const
	{
		auto it = m_registry.find(T::STATIC_SYSTEM_TYPE_ID);
		if(it == m_registry.end())
		{
			LOG_ERROR("System {0} isn't registered", typeid(T).name());
			return nullptr;

		}
		return (T*)it->second;;
	}

	template<typename T>
	void EnableSystem()
	{
		SystemTypeID id = T::STATIC_SYSTEM_TYPE_ID;
		auto it = m_registry.find(id);
		if(it != m_registry.end())
		{
			/* if(it->second->m_enabled == true)
				return; */
			it->second->m_enabled = true;
		}
	}

	template<typename T>
	void DisableSystem()
	{
		SystemTypeID id = T::STATIC_SYSTEM_TYPE_ID;
		auto it = m_registry.find(id);
		if(it != m_registry.end())
		{
			if(it->second->m_enabled == false)
				return;
			it->second->m_enabled = false;
		}
	}

};