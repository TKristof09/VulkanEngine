#pragma once
#include "flecs.h"

template<typename... Components>
using Query = flecs::query<Components...>;

template<typename... Components>
using QueryBuilder = flecs::query_builder<Components...>;
