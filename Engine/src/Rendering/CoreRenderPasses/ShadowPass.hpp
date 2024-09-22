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
    struct PushConstants
    {
        int32_t lightIndex;
        uint64_t shadowMatricesBuffer;
        uint64_t transformBufferPtr;
        uint64_t objectIDMapPtr;
    };
    void RegisterPass(RenderGraph& rg)
    {
        auto& shadowPass    = rg.AddRenderPass("shadowPass", QueueTypeFlagBits::Graphics);
        // auto& shadowMatricesBuffer = shadowPass.AddStorageBufferReadOnly("shadowMatrices", VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, true);
        auto& drawObjBuffer = shadowPass.AddDrawCommandBuffer("drawObjBuffer");
        auto& drawBuffer    = shadowPass.AddDrawCommandBuffer("drawBuffer");

        auto& shadowMapRessource = shadowPass.AddTextureArrayOutput("shadowMaps", VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        shadowMapRessource.SetFormat(VK_FORMAT_D32_SFLOAT);
        shadowPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                PushConstants pc        = {};
                pc.transformBufferPtr   = m_ecs->GetSingleton<TransformBuffers>()->buffers[imageIndex].GetDeviceAddress(0);
                pc.shadowMatricesBuffer = m_ecs->GetSingleton<ShadowBuffers>()->matricesBuffers[imageIndex].GetDeviceAddress(0);
                pc.objectIDMapPtr       = drawObjBuffer.GetBufferPointer()->GetDeviceAddress();
                for(uint32_t i = 0; i < shadowMapRessource.GetImagePointers().size(); ++i)
                {
                    const auto* img = shadowMapRessource.GetImagePointers()[i];

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

                    pc.lightIndex = static_cast<int32_t>(i);

                    m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));

                    uint32_t maxCount = static_cast<uint32_t>(drawBuffer.GetBufferPointer()->GetSize() / sizeof(VkDrawIndexedIndirectCommand));
                    vkCmdDrawIndexedIndirectCount(cb.GetCommandBuffer(), drawBuffer.GetBufferPointer()->GetVkBuffer(), 0, drawObjBuffer.GetBufferPointer()->GetVkBuffer(), 0, maxCount, sizeof(VkDrawIndexedIndirectCommand));
                    vkCmdEndRendering(cb.GetCommandBuffer());
                }
            });
    }


    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
};
