#pragma once

#include "ECS/System.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"
#include "ECS/CoreComponents/Transform.hpp"

//TODO: sort other components that need parent to be updated before children too
// for now idk if that is actually needed, but add if needed
class TransformHierarchySystem : public System<TransformHierarchySystem>
{
public:
	TransformHierarchySystem();
	virtual void Update(double dt) override; //TODO: not sure if this should happen in update rather than pre/post
private:
	void OnTransformAdded(const ComponentAdded<Transform>* event);

};
