#pragma once

#include "ECS/ISystem.hpp"
#include "ECS/Util.hpp"

template<typename T>
class System : public ISystem
{
public:
	virtual const SystemTypeID GetTypeID() const override { return STATIC_SYSTEM_TYPE_ID; }

	static const SystemTypeID STATIC_SYSTEM_TYPE_ID;

	virtual void PreUpdate(float dt) override {};
	virtual void Update(float dt) override {};
	virtual void PostUpdate(float dt) override {};

private:
	// System destrucion happens through SystemManager
	void operator delete(void*) = delete;
	void operator delete[](void*) = delete;
};

template<typename T>
const SystemTypeID System<T>::STATIC_SYSTEM_TYPE_ID = Util::TypeIDManager<ISystem>::Get<T>();

