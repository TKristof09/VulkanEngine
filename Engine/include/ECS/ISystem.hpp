#pragma once

#include "ECS/Types.hpp"

class ISystem
{
	friend class SystemManager;
public:
	virtual ~ISystem() {};

	virtual void PreUpdate(float dt) = 0;
	virtual void Update(float dt) = 0;
	virtual void PostUpdate(float dt) = 0;

	virtual const SystemTypeID GetTypeID() const = 0;

	inline bool IsEnabled() const { return m_enabled; }


private:
	bool m_enabled;

};
