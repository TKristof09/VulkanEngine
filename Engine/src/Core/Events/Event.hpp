#pragma once

#include <typeindex>

class Event
{
public:
    virtual ~Event() = default;
    [[nodiscard]] virtual std::type_index GetEventTypeID() const final { return {typeid(*this)}; }
};
