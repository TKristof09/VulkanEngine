#pragma once

#include "Application.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/Camera.hpp"
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
        depthPipeline.useColor         = true;
        // depthPipeline.parent			= const_cast<Pipeline*>(&(*mainIter));
        depthPipeline.depthCompareOp   = VK_COMPARE_OP_GREATER;
        depthPipeline.depthWriteEnable = true;
        depthPipeline.useDepth         = true;
        depthPipeline.depthClampEnable = false;
        depthPipeline.stages           = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        depthPipeline.useColorBlend    = false;
        depthPipeline.viewportExtent   = VulkanContext::GetSwapchainExtent();

        depthPipeline.isGlobal = true;

        // m_depthPipeline = AddPipeline("depth", depthPipeline, 0);
        m_depthPipeline = std::make_unique<Pipeline>("depth", depthPipeline, 0);

        m_ecs = Application::GetInstance()->GetScene()->GetECS();
        RegisterPass(rg);
    }

private:
    struct ShaderData
    {
        glm::mat4 viewProj;
        glm::mat4 view;
    };
    struct PushConstants
    {
        uint64_t transformBufferPtr;
        uint64_t shaderDataPtr;
    };

    void RegisterPass(RenderGraph& rg)
    {
        auto& depthPass                   = rg.AddRenderPass("depthPass", QueueTypeFlagBits::Graphics);
        AttachmentInfo depthInfo          = {};
        depthInfo.clear                   = true;
        depthInfo.clearValue.depthStencil = {0.0f, 0};
        depthPass.AddDepthOutput("depthImage", depthInfo);
        AttachmentInfo colorInfo   = {};
        colorInfo.clear            = true;
        colorInfo.clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
        depthPass.AddColorOutput("vsNormals", colorInfo);
        depthPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), depthPass.GetRenderingInfo());

                const MainCameraData* camera = m_ecs->GetSingleton<MainCameraData>();
                const ShaderData shaderData{
                    .viewProj = camera->viewProj,
                    .view     = camera->view,
                };

                m_depthPipeline->UploadShaderData(&shaderData, imageIndex);

                PushConstants pc{
                    .transformBufferPtr = m_ecs->GetSingleton<TransformBuffers>()->buffers[imageIndex].GetDeviceAddress(0),
                    .shaderDataPtr      = m_depthPipeline->GetShaderDataBufferPtr(imageIndex)};


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
