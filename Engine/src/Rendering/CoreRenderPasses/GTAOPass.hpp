#pragma once

#include "Application.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Pipeline.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include <glm/glm.hpp>
class GTAOPass
{
public:
    GTAOPass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS())
    {
        PipelineCreateInfo compute = {};
        compute.type               = PipelineType::COMPUTE;
        m_pipeline                 = std::make_unique<Pipeline>("gtao", compute);

        RegisterPass(rg);
    }

private:
    struct PushConstants
    {
        uint32_t depthTexture;
        uint32_t outAoTexture;  // R32 uint texture
        uint32_t hilbertLUT;
        uint32_t normalsTexture;

        glm::vec2 clipToViewSpaceConsts;
        glm::vec2 screenSize;
        float zNear;
    };


    void RegisterPass(RenderGraph& rg)
    {
        auto& pass         = rg.AddRenderPass("gtaoPass", QueueTypeFlagBits::Compute);
        auto& depthTexture = pass.AddTextureInput("depthImage");
        auto& aoTexture    = pass.AddStorageImageOutput("aoImage", VK_FORMAT_R32_SFLOAT);
        auto& normals      = pass.AddTextureInput("vsNormals");
        // auto& hilbertLUT   = pass.AddStorageImageReadOnly("hilbertLUT", true);


        pass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                const auto* mainCamera  = m_ecs->GetSingleton<MainCameraData>();
                const auto viewportSize = glm::vec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);

                m_pipeline->Bind(cb);

                PushConstants pc         = {};
                pc.zNear                 = mainCamera->zNear;
                pc.clipToViewSpaceConsts = mainCamera->clipToViewSpaceConsts;
                pc.screenSize            = viewportSize;

                pc.depthTexture   = depthTexture.GetImagePointer()->GetSampledSlot();
                pc.outAoTexture   = aoTexture.GetImagePointer()->GetStorageSlot();
                pc.hilbertLUT     = pc.outAoTexture;  // hilbertLUT.GetImagePointer()->GetSlot();
                pc.normalsTexture = normals.GetImagePointer()->GetSampledSlot();

                m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));


                auto tileNums = glm::ivec2(ceil(viewportSize.x / 8.0f), ceil(viewportSize.y / 8.0f));
                vkCmdDispatch(cb.GetCommandBuffer(), tileNums.x, tileNums.y, 1);
            });
    }

    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
};
