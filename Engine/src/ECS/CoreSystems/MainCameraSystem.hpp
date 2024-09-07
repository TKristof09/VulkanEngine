#pragma once

#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/InternalTransform.hpp"
#include "ECS/System.hpp"

class MainCameraSystem : public System
{
public:
    void Initialize() override
    {
        StartSystemBuilder<const Camera, const InternalTransform, MainCameraData&>(
            SystemPhase::PreRender,
            [](auto& builder)
            {
                builder.term_at(3).singleton().each(Update);
            });
    }

    // In the future if we want to support multiple cameras we will probably want to use a tag with the camera component
    static void Update(const Camera& camera, const InternalTransform& transform, MainCameraData& mainCameraData)
    {
        mainCameraData.view     = glm::inverse(transform.worldTransform);
        mainCameraData.viewProj = GetProjection(camera) * mainCameraData.view;
        mainCameraData.pos      = transform.worldTransform[3];
        mainCameraData.zNear    = camera.zNear;

        float f                              = tan(camera.fovRadians / 2.0f);
        mainCameraData.clipToViewSpaceConsts = glm::vec2(camera.aspect * f, f);
    }

private:
    const char* GetName() const override { return "MainCameraSystem"; }
};
