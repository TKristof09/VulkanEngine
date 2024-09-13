#pragma once

#include "Core/Events/Event.hpp"

struct Scene;
struct SceneSwitchedEvent : public Event
{
    Scene* newScene;
};
