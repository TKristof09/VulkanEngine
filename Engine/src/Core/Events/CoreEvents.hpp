#pragma once

#include "Core/Events/Event.hpp"

class Scene;
struct SceneSwitchedEvent : public Event
{
    Scene* newScene;
};
