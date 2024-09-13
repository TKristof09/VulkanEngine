#pragma once
#include <flecs.h>

using Observer = flecs::observer;

enum class ECSEvent
{
    OnAdd,
    OnSet,
    OnRemove,
};
