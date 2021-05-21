#pragma once

class ECSEngine;
class EventHandler;

struct Scene
{
	std::string name = "DefaultScene";

	ECSEngine* ecs;
	EventHandler* eventHandler;
};
