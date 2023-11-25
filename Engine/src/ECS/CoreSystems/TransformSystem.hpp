#pragma once

#include "ECS/System.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/InternalTransform.hpp"
class TransformSystem : public System
{
public:
    static void CalculateWorldTransforms(const Transform& transform, const InternalTransform* parentTransform, InternalTransform& internalTransform);

    void Initialize() override;

private:
    const char* GetName() const override { return "TransformSystem"; }
};
