#pragma once

#include "Application.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Pipeline.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include <glm/glm.hpp>
class LightCullPass
{
public:
    LightCullPass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS())
    {
        PipelineCreateInfo compute = {};
        compute.type               = PipelineType::COMPUTE;
        m_pipeline                 = std::make_unique<Pipeline>("lightCulling", compute);

        RegisterPass(rg);
    }

private:
    struct ShaderData
    {
        glm::ivec2 viewportSize;
        glm::ivec2 tileNums;
        uint64_t visibleLightsBuffer;
        uint64_t lightBuffer;

        int lightNum;
        int depthTextureId;
        int debugTextureId;
    };
    struct PushConstants
    {
        glm::mat4 viewProj;   // this is set by the renderer, not each pass (might change in the future if we start needing multiple cameras for things like reflections or idk)
        glm::vec3 cameraPos;  // this is set by the renderer, not each pass (might change in the future if we start needing multiple cameras for things like reflections or idk)
        int32_t debugMode;
        uint64_t shaderDataPtr;
    };


    void RegisterPass(RenderGraph& rg)
    {
        auto& lightCullPass       = rg.AddRenderPass("lightCullPass", QueueTypeFlagBits::Compute);
        auto& visibleLightsBuffer = lightCullPass.AddStorageBufferOutput("visibleLightsBuffer", "", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, true);
        auto& depthTexture        = lightCullPass.AddTextureInput("depthImage");
        auto& debugTexture        = lightCullPass.AddStorageImageOutput("debugImage", VK_FORMAT_R8_UNORM);

        lightCullPass.SetInitialiseCallback(
            [&](RenderGraph& rg)
            {
                ShaderData data          = {};
                data.viewportSize        = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums            = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                data.visibleLightsBuffer = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                data.depthTextureId      = depthTexture.GetImagePointer()->GetSampledSlot();
                data.debugTextureId      = debugTexture.GetImagePointer()->GetSampledSlot();

                const auto* lightBuffers = m_ecs->GetSingleton<LightBuffers>();
                data.lightNum            = lightBuffers->lightNum;
                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer = lightBuffers->buffers[i].GetDeviceAddress(0);
                    m_pipeline->UploadShaderData(&data, i);
                }
            });
        lightCullPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                const auto* lightBuffers = m_ecs->GetSingleton<LightBuffers>();
                ShaderData data          = {};
                data.lightNum            = lightBuffers->lightNum;
                data.lightBuffer         = lightBuffers->buffers[imageIndex].GetDeviceAddress(0);
                m_pipeline->UploadShaderData(&data, imageIndex, offsetof(ShaderData, lightBuffer), sizeof(uint64_t) + sizeof(int));

                m_pipeline->Bind(cb);

                PushConstants pc = {};


                const auto* mainCamera = m_ecs->GetSingleton<MainCameraData>();
                pc.viewProj            = mainCamera->viewProj;
                pc.cameraPos           = mainCamera->pos;
                pc.shaderDataPtr       = m_pipeline->GetShaderDataBufferPtr(imageIndex);
                pc.debugMode           = 1;

                m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));

                auto viewportSize = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);

                auto tileNums = glm::ivec2(ceil(viewportSize.x / 16.0f), ceil(viewportSize.y / 16.0f));
                vkCmdDispatch(cb.GetCommandBuffer(), tileNums.x, tileNums.y, 1);
            });
    }

    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
};
