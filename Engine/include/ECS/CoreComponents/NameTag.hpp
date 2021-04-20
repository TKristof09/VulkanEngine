#pragma once
#include "ECS/Component.hpp"
struct NameTag : public Component<NameTag>
{
	std::string name;
};