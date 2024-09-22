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
        glm::mat4 proj          = GetProjection(camera);
        mainCameraData.viewProj = proj * mainCameraData.view;
        mainCameraData.pos      = transform.worldTransform[3];
        mainCameraData.zNear    = camera.zNear;

        float f                              = tan(camera.fovRadians / 2.0f);
        mainCameraData.clipToViewSpaceConsts = glm::vec2(camera.aspect * f, f);

        // left (x > -w)
        mainCameraData.frustumPlanesVS[0].x = proj[0].w + proj[0].x;
        mainCameraData.frustumPlanesVS[0].y = proj[1].w + proj[1].x;
        mainCameraData.frustumPlanesVS[0].z = proj[2].w + proj[2].x;
        mainCameraData.frustumPlanesVS[0].w = proj[3].w + proj[3].x;
        // right (x < w)
        mainCameraData.frustumPlanesVS[1].x = proj[0].w - proj[0].x;
        mainCameraData.frustumPlanesVS[1].y = proj[1].w - proj[1].x;
        mainCameraData.frustumPlanesVS[1].z = proj[2].w - proj[2].x;
        mainCameraData.frustumPlanesVS[1].w = proj[3].w - proj[3].x;
        // bottom (y > -w)
        mainCameraData.frustumPlanesVS[2].x = proj[0].w + proj[0].y;
        mainCameraData.frustumPlanesVS[2].y = proj[1].w + proj[1].y;
        mainCameraData.frustumPlanesVS[2].z = proj[2].w + proj[2].y;
        mainCameraData.frustumPlanesVS[2].w = proj[3].w + proj[3].y;
        // top (y < w)
        mainCameraData.frustumPlanesVS[3].x = proj[0].w - proj[0].y;
        mainCameraData.frustumPlanesVS[3].y = proj[1].w - proj[1].y;
        mainCameraData.frustumPlanesVS[3].z = proj[2].w - proj[2].y;
        mainCameraData.frustumPlanesVS[3].w = proj[3].w - proj[3].y;
        // near (we use reverse z so near plane is z=1) (z < w)
        mainCameraData.frustumPlanesVS[4].x = proj[0].w - proj[0].z;
        mainCameraData.frustumPlanesVS[4].y = proj[1].w - proj[1].z;
        mainCameraData.frustumPlanesVS[4].z = proj[2].w - proj[2].z;
        mainCameraData.frustumPlanesVS[4].w = proj[3].w - proj[3].z;
    }

private:
    const char* GetName() const override { return "MainCameraSystem"; }
};
