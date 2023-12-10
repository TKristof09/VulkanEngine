#pragma once

#include "ECS/CoreComponents/SkyboxComponent.hpp"
#include "Rendering/Pipeline.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/VulkanContext.hpp"
#include "Application.hpp"
#include "ECS/CoreComponents/Camera.hpp"

#include <glm/glm.hpp>

class SkyboxPass
{
public:
    SkyboxPass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS()),
          m_envMap(&m_ecs->GetSingleton<SkyboxComponent>()->skybox)
    {
        // Skybox pipeline
        LOG_WARN("Creating skybox pipeline");
        PipelineCreateInfo ci;
        ci.type             = PipelineType::GRAPHICS;
        ci.useColor         = true;
        ci.depthCompareOp   = VK_COMPARE_OP_EQUAL;  // only write to pixels with depth = 0 which is infinitely far away
        ci.depthWriteEnable = false;
        ci.useDepth         = true;
        ci.depthClampEnable = false;
        ci.msaaSamples      = VK_SAMPLE_COUNT_1_BIT;
        ci.stages           = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ci.useMultiSampling = true;
        ci.useColorBlend    = true;
        ci.viewportExtent   = VulkanContext::GetSwapchainExtent();

        m_skyboxPipeline = std::make_unique<Pipeline>("skybox", ci);


        RegisterPass(rg);
    }

private:
    struct PushConstants
    {
        glm::mat4 viewProj;
        int32_t envMapSlot;
    };

    void RegisterPass(RenderGraph& rg)
    {
        auto& skyboxPass = rg.AddRenderPass("skyboxPass", QueueTypeFlagBits::Graphics);
        skyboxPass.AddColorOutput("skyboxImage", {}, "colorImage");
        skyboxPass.AddDepthInput("depthImage");
        skyboxPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), skyboxPass.GetRenderingInfo());

                m_skyboxPipeline->Bind(cb);

                PushConstants pc{m_ecs->GetSingleton<MainCameraData>()->viewProj, m_envMap->GetSlot()};
                m_skyboxPipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));

                vkCmdDraw(cb.GetCommandBuffer(), 6, 1, 0, 0);
                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }
    std::unique_ptr<Pipeline> m_skyboxPipeline;

    ECS* m_ecs;
    const Image* m_envMap;
};
