#pragma once

#include "ECS/IComponent.hpp"
#include "ECS/Util.hpp"

template<typename T>
class Component : public IComponent
{
public:

	static const ComponentTypeID STATIC_COMPONENT_TYPE_ID;
	inline const ComponentTypeID GetTypeID() const { return STATIC_COMPONENT_TYPE_ID; }

//private:
//	// Component destrucion happens through ComponentManager
//	void operator delete(void*) = delete;
//	void operator delete[](void*) = delete;
};

template<typename T>
const ComponentTypeID Component<T>::STATIC_COMPONENT_TYPE_ID = Util::TypeIDManager<IComponent>::Get<T>();

