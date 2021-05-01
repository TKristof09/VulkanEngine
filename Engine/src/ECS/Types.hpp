#pragma once
#include <limits>

using EntityID = uint64_t;
const EntityID INVALID_ENTITY_ID = std::numeric_limits<uint64_t>::max();

using ComponentID = uint64_t;
using ComponentTypeID = uint64_t;

using SystemTypeID = uint64_t;


