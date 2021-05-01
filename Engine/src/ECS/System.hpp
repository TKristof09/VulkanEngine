#pragma once

#include "ECS/ISystem.hpp"
#include "Utils/TypeIDManager.hpp"
#include "ECS/ECSEngine.hpp"

template<typename T>
class System : public ISystem
{
public:
	virtual const SystemTypeID GetTypeID() const override { return STATIC_SYSTEM_TYPE_ID; }

	static const SystemTypeID STATIC_SYSTEM_TYPE_ID;

	virtual void PreUpdate(double dt) override {};
	virtual void Update(double dt) override {};
	virtual void PostUpdate(double dt) override {};
	virtual void FixedUpdate(double dt) override {};

};

template<typename T>
const SystemTypeID System<T>::STATIC_SYSTEM_TYPE_ID = TypeIDManager<ISystem>::Get<T>();

