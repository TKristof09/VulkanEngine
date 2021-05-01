#pragma once

class ECSEngine;
class EventHandler;

struct Scene
{
	std::string name;
	
	ECSEngine* ecs;
	EventHandler* eventHandler;
};
