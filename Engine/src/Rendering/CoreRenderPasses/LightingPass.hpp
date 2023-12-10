#pragma once

#include "Application.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Pipeline.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include "Core/Events/EventHandler.hpp"

class LightingPass
{
public:
    LightingPass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS())
    {
        // TODO: Create pipeline here?
        PipelineCreateInfo ci;
        ci.type             = PipelineType::GRAPHICS;
        ci.allowDerivatives = true;
        ci.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
        ci.depthWriteEnable = false;
        ci.useDepth         = true;
        ci.stages           = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        ci.useMultiSampling = true;
        ci.useColorBlend    = true;

        ci.viewportExtent = VulkanContext::GetSwapchainExtent();

        m_pipeline = std::make_unique<Pipeline>("forwardplus", ci, 0);


        RegisterPass(rg);
        Application::GetInstance()->GetEventHandler()->Subscribe(this, &LightingPass::OnDirectionalLightAdded);
    }


    void OnDirectionalLightAdded(ComponentAdded<DirectionalLight> e)
    {
        // updata only the data that concerncs shadows when a new light is added
        const auto* shadowBuffers = m_ecs->GetSingleton<ShadowBuffers>();
        ShaderData data           = {};
        data.shadowMapCount       = shadowBuffers->numIndices;

        for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
        {
            data.shadowMapIds         = shadowBuffers->indicesBuffers[i].GetDeviceAddress(0);
            data.shadowMatricesBuffer = shadowBuffers->matricesBuffers[i].GetDeviceAddress(0);
            m_pipeline->UploadShaderData(&data, i, offsetof(ShaderData, shadowMatricesBuffer), 2 * sizeof(uint64_t) + sizeof(uint32_t));
        }
    }

private:
    struct ShaderData
    {
        glm::ivec2 viewportSize;
        glm::ivec2 tileNums;
        uint64_t lightBuffer;
        uint64_t visibleLightsBuffer;
        uint64_t shadowMatricesBuffer;

        uint64_t shadowMapIds;
        uint32_t shadowMapCount;

        uint32_t irradianceMapIndex;
        uint32_t prefilteredEnvMapIndex;
        uint32_t BRDFLUTIndex;
    };

    struct PushConstants
    {
        glm::mat4 viewProj;   // this is set by the renderer, not each pass (might change in the future if we start needing multiple cameras for things like reflections or idk)
        glm::vec3 cameraPos;  // this is set by the renderer, not each pass (might change in the future if we start needing multiple cameras for things like reflections or idk)
        int32_t debugMode;
        uint64_t shaderDataPtr;
        uint64_t transformBufferPtr;
        uint64_t objectIdMapPtr;
        uint64_t materialDataPtr;
    };
    void RegisterPass(RenderGraph& rg)
    {
        auto& lightingPass         = rg.AddRenderPass("lightingPass", QueueTypeFlagBits::Graphics);
        AttachmentInfo colorInfo   = {};
        colorInfo.clear            = true;
        colorInfo.clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
        lightingPass.AddColorOutput("colorImage", colorInfo);
        lightingPass.AddDepthInput("depthImage");
        auto& visibleLightsBuffer = lightingPass.AddStorageBufferReadOnly("visibleLightsBuffer", VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, true);
        auto* lightBuffers        = m_ecs->GetSingletonMut<LightBuffers>();
        visibleLightsBuffer.SetBufferPointer(&lightBuffers->visibleLightsBuffer);
        auto& lightingDrawCommands = lightingPass.AddDrawCommandBuffer("lightingDrawCommands");
        auto& shadowMaps           = lightingPass.AddTextureArrayInput("shadowMaps", VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

        lightingPass.SetInitialiseCallback(
            [&](RenderGraph& rg)
            {
                ShaderData data             = {};
                data.viewportSize           = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums               = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                // TODO temp
                const auto* pbrEnv          = m_ecs->GetSingleton<PBREnvironment>();
                data.irradianceMapIndex     = pbrEnv->irradianceMap.GetSlot();
                data.prefilteredEnvMapIndex = pbrEnv->prefilteredEnvMap.GetSlot();
                data.BRDFLUTIndex           = pbrEnv->BRDFLUT.GetSlot();


                const auto* shadowBuffers = m_ecs->GetSingleton<ShadowBuffers>();
                data.shadowMapCount       = shadowBuffers->numIndices;


                data.visibleLightsBuffer = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                const auto* lightBuffers = m_ecs->GetSingleton<LightBuffers>();
                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer          = lightBuffers->buffers[i].GetDeviceAddress(0);
                    data.shadowMapIds         = shadowBuffers->indicesBuffers[i].GetDeviceAddress(0);
                    data.shadowMatricesBuffer = shadowBuffers->matricesBuffers[i].GetDeviceAddress(0);
                    m_pipeline->UploadShaderData(&data, i);
                }
            });
        lightingPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), lightingPass.GetRenderingInfo());


                PushConstants pc = {};

                m_pipeline->Bind(cb);

                const auto* mainCamera = m_ecs->GetSingleton<MainCameraData>();
                pc.viewProj            = mainCamera->viewProj;
                pc.cameraPos           = mainCamera->pos;
                pc.transformBufferPtr  = m_ecs->GetSingleton<TransformBuffers>()->buffers[imageIndex].GetDeviceAddress(0);
                pc.shaderDataPtr       = m_pipeline->GetShaderDataBufferPtr(imageIndex);
                pc.materialDataPtr     = m_pipeline->GetMaterialBufferPtr();

                m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));

                const auto* drawCmds = m_ecs->GetSingleton<DrawCommandBuffer>();
                vkCmdDrawIndexedIndirect(cb.GetCommandBuffer(), drawCmds->buffer.GetVkBuffer(), 0, drawCmds->count, sizeof(DrawCommand));

                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }

    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
};
