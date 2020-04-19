#pragma once

#include "ECS/Types.hpp"
#include "ComponentManager.hpp"

class IEntity
{
	friend class EntityManager;
public:
	IEntity():
		m_active(true)
	{};
	virtual ~IEntity() {};

	template<typename T>
	T* GetComponent() const
	{
		return m_componentManager->GetComponent<T>(m_id);
	}

	template<typename T, typename ...ARGS>
	T* AddComponent(ARGS&&... args)
	{
		return m_componentManager->AddComponent<T>(m_id, std::forward<ARGS>(args)...);
	}

	template<typename T>
	void RemoveComponent()
	{
		m_componentManager->RemoveComponent<T>(m_id);
	}

	inline bool operator==(const IEntity& rhs) const { return m_id == rhs.m_id; }
	inline bool operator!=(const IEntity& rhs) const { return m_id != rhs.m_id; }
	inline bool operator==(const IEntity* rhs) const { return m_id == rhs->m_id; }
	inline bool operator!=(const IEntity* rhs) const { return m_id != rhs->m_id; }

	virtual const EntityTypeID GetTypeID() const = 0;
	inline const EntityID GetEntityID() const { return m_id; }

	void SetActive(bool state) { m_active = state; }
	inline bool IsActive() const { return m_active; }
protected:
	EntityID m_id;
	bool m_active;

private:
	ComponentManager* m_componentManager;

};
