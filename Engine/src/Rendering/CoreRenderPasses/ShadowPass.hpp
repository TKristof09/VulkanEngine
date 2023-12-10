#pragma once

#include "Application.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Pipeline.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include "Core/Events/EventHandler.hpp"
class ShadowPass
{
public:
    ShadowPass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS())
    {
        // Shadow prepass pipeline
        LOG_WARN("Creating shadow pipeline");
        PipelineCreateInfo shadowPipeline;
        shadowPipeline.type             = PipelineType::GRAPHICS;
        shadowPipeline.useColor         = false;
        // depthPipeline.parent			= const_cast<Pipeline*>(&(*mainIter));
        shadowPipeline.depthCompareOp   = VK_COMPARE_OP_GREATER;
        shadowPipeline.depthWriteEnable = true;
        shadowPipeline.useDepth         = true;
        shadowPipeline.depthClampEnable = false;
        shadowPipeline.msaaSamples      = VK_SAMPLE_COUNT_1_BIT;
        shadowPipeline.stages           = VK_SHADER_STAGE_VERTEX_BIT;
        shadowPipeline.useMultiSampling = true;
        shadowPipeline.useColorBlend    = false;
        shadowPipeline.viewportExtent   = {SHADOWMAP_SIZE, SHADOWMAP_SIZE};
        shadowPipeline.viewMask         = (1 << NUM_CASCADES) - 1;  // NUM_CASCADES of 1s
        shadowPipeline.isGlobal         = true;

        m_pipeline = std::make_unique<Pipeline>("shadow", shadowPipeline);

        RegisterPass(rg);
    }


private:
    struct ShaderData
    {
        uint64_t lightBuffer;
        uint64_t shadowMatricesBuffer;
    };

    struct PushConstants
    {
        int32_t lightIndex;
        uint64_t shaderDataPtr;
        uint64_t transformBufferPtr;
    };
    void RegisterPass(RenderGraph& rg)
    {
        auto& shadowPass = rg.AddRenderPass("shadowPass", QueueTypeFlagBits::Graphics);
        // auto& shadowMatricesBuffer = shadowPass.AddStorageBufferReadOnly("shadowMatrices", VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, true);

        auto& shadowMapRessource = shadowPass.AddTextureArrayOutput("shadowMaps", VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        shadowMapRessource.SetFormat(VK_FORMAT_D32_SFLOAT);
        shadowPass.SetInitialiseCallback(
            [&](RenderGraph& rg)
            {
                ShaderData data           = {};
                const auto* lightBuffers  = m_ecs->GetSingleton<LightBuffers>();
                const auto* shadowBuffers = m_ecs->GetSingleton<ShadowBuffers>();
                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer          = lightBuffers->buffers[i].GetDeviceAddress(0);
                    data.shadowMatricesBuffer = shadowBuffers->matricesBuffers[i].GetDeviceAddress(0);
                    m_pipeline->UploadShaderData(&data, i);
                }
            });
        shadowPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                // TODO: how would this work with multiple lights? we somehow need to associate light with the corresponding image
                for(const auto* img : shadowMapRessource.GetImagePointers())
                {
                    // set up the renderingInfo struct
                    VkRenderingInfo rendering          = {};
                    rendering.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    rendering.renderArea.extent.width  = img->GetWidth();
                    rendering.renderArea.extent.height = img->GetHeight();
                    // rendering.layerCount               = 1;
                    rendering.viewMask                 = m_pipeline->GetViewMask();

                    VkRenderingAttachmentInfo depthAttachment     = {};
                    depthAttachment.sType                         = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    depthAttachment.imageView                     = img->GetImageView();
                    depthAttachment.clearValue.depthStencil.depth = 0.0f;
                    depthAttachment.loadOp                        = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    depthAttachment.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
                    depthAttachment.imageLayout                   = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                    rendering.pDepthAttachment                    = &depthAttachment;

                    vkCmdBeginRendering(cb.GetCommandBuffer(), &rendering);

                    // render the corresponding light

                    m_pipeline->Bind(cb);

                    PushConstants pc      = {};
                    pc.lightIndex         = 0;
                    pc.transformBufferPtr = m_ecs->GetSingleton<TransformBuffers>()->buffers[imageIndex].GetDeviceAddress(0);
                    pc.shaderDataPtr      = m_pipeline->GetShaderDataBufferPtr(imageIndex);

                    m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));

                    const auto* drawCmds = m_ecs->GetSingleton<DrawCommandBuffer>();
                    vkCmdDrawIndexedIndirect(cb.GetCommandBuffer(), drawCmds->buffer.GetVkBuffer(), 0, drawCmds->count, sizeof(DrawCommand));
                    vkCmdEndRendering(cb.GetCommandBuffer());
                }
            });
    }


    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
};
