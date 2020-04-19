#pragma once

#include "ECS/IEntity.hpp"
#include "ECS/Util.hpp"

template<typename T>
class Entity : public IEntity
{
public:
	virtual const EntityTypeID GetTypeID() const override { return STATIC_ENTITY_TYPE_ID; }

	static const EntityTypeID STATIC_ENTITY_TYPE_ID;

private:
	// Entity destrucion happens through EntityManager
	void operator delete(void*) = delete;
	void operator delete[](void*) = delete;

};

template<typename T>
const EntityTypeID Entity<T>::STATIC_ENTITY_TYPE_ID = Util::TypeIDManager<IEntity>::Get<T>();
