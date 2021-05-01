#pragma once

#include "ECS/IEvent.hpp"
class IEventDelegate
{
public:
	virtual ~IEventDelegate() {};
	virtual void Invoke(const IEvent* const e) = 0;
	virtual bool operator==(const IEventDelegate* other) const = 0;
};
