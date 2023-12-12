#pragma once

class ECS;

struct Scene
{
    std::string name = "DefaultScene";

    ECS* GetECS() { return ecs.get(); }

    std::unique_ptr<ECS> ecs;
};
