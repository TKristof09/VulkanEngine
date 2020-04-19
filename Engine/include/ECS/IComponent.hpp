#pragma once
//TODO
#include "ECS/Types.hpp"

class IComponent
{
	friend class ComponentManager;
public:
	IComponent():
		m_parent(INVALID_ENTITY_ID),
		m_enabled(true)
	{};
	virtual ~IComponent() {};

	inline const ComponentID GetComponentID() const { return m_id; }
	inline const EntityID GetParent() const { return m_parent; }

	inline void SetEnabled(bool state) { m_enabled = state; }
	inline bool IsEnabled() const { return m_enabled; }
protected:
	ComponentID m_id;
	ComponentTypeID m_typeID;
	EntityID m_parent;
	bool m_enabled;

};
