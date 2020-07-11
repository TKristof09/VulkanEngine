#pragma once
//TODO
#include "ECS/Types.hpp"

class IComponent
{
	friend class ComponentManager;
public:
	IComponent():
		m_owner(INVALID_ENTITY_ID),
		m_enabled(true)
	{};
	virtual ~IComponent() {};

	inline const ComponentID GetComponentID() const { return m_id; }
	inline const EntityID GetOwner() const { return m_owner; }

	inline void SetEnabled(bool state) { m_enabled = state; }
	inline bool IsEnabled() const { return m_enabled; }
protected:
	ComponentID m_id;
	ComponentTypeID m_typeID;
	EntityID m_owner;
	bool m_enabled;

};
