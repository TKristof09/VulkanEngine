#pragma once

#include "Application.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Pipeline.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include <glm/glm.hpp>
class DenoisePass
{
public:
    DenoisePass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS())
    {
        PipelineCreateInfo compute = {};
        compute.type               = PipelineType::COMPUTE;
        m_pipeline                 = std::make_unique<Pipeline>("denoise", compute);

        RegisterPass(rg);
    }

private:
    struct PushConstants
    {
        uint32_t depthTexture;
        uint32_t aoTexture;      // R32 uint texture
        uint32_t outputTexture;  // R32 uint texture
        float zNear;
        glm::vec2 texelSize;

        float sigmaSpatial;
        float sigmaIntensity;
    };


    void RegisterPass(RenderGraph& rg)
    {
        auto& pass          = rg.AddRenderPass("denoisePass", QueueTypeFlagBits::Compute);
        auto& depthTexture  = pass.AddTextureInput("depthImage");
        auto& aoTexture     = pass.AddTextureInput("aoImage");
        auto& outputTexture = pass.AddStorageImageOutput("finalAOImage", VK_FORMAT_R32_SFLOAT);


        pass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                const auto* mainCamera  = m_ecs->GetSingleton<MainCameraData>();
                const auto viewportSize = glm::vec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);

                m_pipeline->Bind(cb);

                PushConstants pc = {};
                pc.zNear         = mainCamera->zNear;
                pc.texelSize     = glm::vec2(1.0f / viewportSize.x, 1.0f / viewportSize.y);

                pc.sigmaSpatial   = 1.0f;
                pc.sigmaIntensity = 1.0f;

                pc.depthTexture  = depthTexture.GetImagePointer()->GetSampledSlot();
                pc.aoTexture     = aoTexture.GetImagePointer()->GetSampledSlot();
                pc.outputTexture = outputTexture.GetImagePointer()->GetStorageSlot();

                m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));


                auto tileNums = glm::ivec2(ceil(viewportSize.x / 2.0f / 8.0f), ceil(viewportSize.y / 2.0f / 8.0f));
                vkCmdDispatch(cb.GetCommandBuffer(), tileNums.x, tileNums.y, 1);
            });
    }

    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
};
