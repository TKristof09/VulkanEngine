#pragma once

#include "Application.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include "Rendering/Pipeline.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include <glm/glm.hpp>

class DepthPass
{
public:
    DepthPass(RenderGraph& rg)
    {
        LOG_WARN("Creating depth pipeline");
        PipelineCreateInfo depthPipeline;
        depthPipeline.type             = PipelineType::GRAPHICS;
        depthPipeline.useColor         = false;
        // depthPipeline.parent			= const_cast<Pipeline*>(&(*mainIter));
        depthPipeline.depthCompareOp   = VK_COMPARE_OP_GREATER;
        depthPipeline.depthWriteEnable = true;
        depthPipeline.useDepth         = true;
        depthPipeline.depthClampEnable = false;
        depthPipeline.stages           = VK_SHADER_STAGE_VERTEX_BIT;
        depthPipeline.useColorBlend    = false;
        depthPipeline.viewportExtent   = VulkanContext::GetSwapchainExtent();

        depthPipeline.isGlobal = true;

        // m_depthPipeline = AddPipeline("depth", depthPipeline, 0);
        m_depthPipeline = std::make_unique<Pipeline>("depth", depthPipeline, 0);

        m_ecs = Application::GetInstance()->GetScene()->GetECS();
        RegisterPass(rg);
    }

private:
    struct PushConstants
    {
        glm::mat4 viewProj;
        uint64_t transformBufferPtr;
    };

    void RegisterPass(RenderGraph& rg)
    {
        auto& depthPass                   = rg.AddRenderPass("depthPass", QueueTypeFlagBits::Graphics);
        AttachmentInfo depthInfo          = {};
        depthInfo.clear                   = true;
        depthInfo.clearValue.depthStencil = {0.0f, 0};
        depthPass.AddDepthOutput("depthImage", depthInfo);
        depthPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), depthPass.GetRenderingInfo());


                PushConstants pc{m_ecs->GetSingleton<MainCameraData>()->viewProj, 0};
                pc.transformBufferPtr = m_ecs->GetSingleton<TransformBuffers>()->buffers[imageIndex].GetDeviceAddress(0);

                m_depthPipeline->Bind(cb);
                m_depthPipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));

                const auto* drawCmds = m_ecs->GetSingleton<DrawCommandBuffer>();
                vkCmdDrawIndexedIndirect(cb.GetCommandBuffer(), drawCmds->buffer.GetVkBuffer(), 0, drawCmds->count, sizeof(DrawCommand));

                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }

    std::unique_ptr<Pipeline> m_depthPipeline;
    ECS* m_ecs;
};
