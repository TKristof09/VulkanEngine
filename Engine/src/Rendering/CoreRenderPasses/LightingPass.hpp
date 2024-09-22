#pragma once

#include "Application.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Pipeline.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include "Core/Events/EventHandler.hpp"

class LightingPass
{
public:
    LightingPass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS()),
          m_statsText(std::make_shared<Text>("LightingPass stats"))
    {
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


        VkQueryPoolCreateInfo queryPoolInfo = {};
        queryPoolInfo.sType                 = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType             = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        queryPoolInfo.pipelineStatistics    = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
                                         | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
                                         | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
                                         | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
                                         | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;

        m_queryNames = {"Input vertices", "Input primitives", "Vertex shader invocations", "Clipping primitives", "Clipping invocations"};
        m_queryResults.resize(m_queryNames.size());

        queryPoolInfo.queryCount = NUM_FRAMES_IN_FLIGHT;
        VK_CHECK(vkCreateQueryPool(VulkanContext::GetDevice(), &queryPoolInfo, nullptr, &m_queryPool), "Failed to create query pool");
        // vkResetQueryPool(VulkanContext::GetDevice(), m_queryPool, 0, NUM_FRAMES_IN_FLIGHT);


        Application::GetInstance()->GetRenderer()->AddDebugUIElement(m_statsText);
    }

    ~LightingPass()
    {
        vkDestroyQueryPool(VulkanContext::GetDevice(), m_queryPool, nullptr);
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

        uint32_t aoTextureIndex;
    };

    struct PushConstants
    {
        glm::mat4 viewProj;
        glm::vec3 cameraPos;
        int32_t debugMode;
        uint64_t shaderDataPtr;
        uint64_t transformBufferPtr;
        uint64_t materialDataPtr;
        uint64_t objectIdMapPtr;
    };
    void RegisterPass(RenderGraph& rg)
    {
        auto& lightingPass         = rg.AddRenderPass("lightingPass", QueueTypeFlagBits::Graphics);
        AttachmentInfo colorInfo   = {};
        colorInfo.clear            = true;
        colorInfo.clearValue.color = {
            {0.0f, 0.0f, 0.0f, 1.0f}
        };
        lightingPass.AddColorOutput("colorImage", colorInfo);
        lightingPass.AddDepthInput("depthImage");
        lightingPass.AddTextureInput("debugImage");
        auto& drawObjBuffer       = lightingPass.AddDrawCommandBuffer("drawObjBuffer");
        auto& drawBuffer          = lightingPass.AddDrawCommandBuffer("drawBuffer");
        auto& visibleLightsBuffer = lightingPass.AddStorageBufferReadOnly("visibleLightsBuffer", VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, true);
        auto* lightBuffers        = m_ecs->GetSingletonMut<LightBuffers>();
        visibleLightsBuffer.SetBufferPointer(&lightBuffers->visibleLightsBuffer);
        lightingPass.AddTextureArrayInput("shadowMaps", VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        auto& aoTexture = lightingPass.AddTextureInput("finalAOImage");

        lightingPass.SetInitialiseCallback(
            [&](RenderGraph& /*rg*/)
            {
                ShaderData data   = {};
                data.viewportSize = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums     = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));

                const auto* pbrEnv          = m_ecs->GetSingleton<PBREnvironment>();
                data.irradianceMapIndex     = pbrEnv->irradianceMap.GetSampledSlot();
                data.prefilteredEnvMapIndex = pbrEnv->prefilteredEnvMap.GetSampledSlot();
                data.BRDFLUTIndex           = pbrEnv->BRDFLUT.GetSampledSlot();


                const auto* shadowBuffers = m_ecs->GetSingleton<ShadowBuffers>();
                data.shadowMapCount       = shadowBuffers->numIndices;


                data.visibleLightsBuffer = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                data.aoTextureIndex      = aoTexture.GetImagePointer()->GetSampledSlot();
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
                vkCmdResetQueryPool(cb.GetCommandBuffer(), m_queryPool, m_queryIndex, 1);
                vkCmdBeginRendering(cb.GetCommandBuffer(), lightingPass.GetRenderingInfo());


                PushConstants pc = {};


                const auto* mainCamera = m_ecs->GetSingleton<MainCameraData>();
                pc.viewProj            = mainCamera->viewProj;
                pc.cameraPos           = mainCamera->pos;
                pc.transformBufferPtr  = m_ecs->GetSingleton<TransformBuffers>()->buffers[imageIndex].GetDeviceAddress(0);
                pc.shaderDataPtr       = m_pipeline->GetShaderDataBufferPtr(imageIndex);
                pc.materialDataPtr     = m_pipeline->GetMaterialBufferPtr();
                pc.objectIdMapPtr      = drawObjBuffer.GetBufferPointer()->GetDeviceAddress();


                vkCmdBeginQuery(cb.GetCommandBuffer(), m_queryPool, m_queryIndex, 0);
                m_pipeline->Bind(cb);
                m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));
                uint32_t maxCount = static_cast<uint32_t>(drawBuffer.GetBufferPointer()->GetSize() / sizeof(VkDrawIndexedIndirectCommand));
                vkCmdDrawIndexedIndirectCount(cb.GetCommandBuffer(), drawBuffer.GetBufferPointer()->GetVkBuffer(), 0, drawObjBuffer.GetBufferPointer()->GetVkBuffer(), 0, maxCount, sizeof(VkDrawIndexedIndirectCommand));

                vkCmdEndQuery(cb.GetCommandBuffer(), m_queryPool, m_queryIndex);

                vkCmdEndRendering(cb.GetCommandBuffer());

                if(m_canQuery)
                {
                    uint32_t dataSize = static_cast<uint32_t>(m_queryResults.size() * sizeof(uint64_t));
                    uint32_t stride   = dataSize;

                    vkGetQueryPoolResults(VulkanContext::GetDevice(), m_queryPool, (m_queryIndex - 1 + NUM_FRAMES_IN_FLIGHT) % NUM_FRAMES_IN_FLIGHT, 1, dataSize, m_queryResults.data(), stride, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

                    std::stringstream stats;
                    stats << "Lighting Pass Stats:\n";
                    for(size_t i = 0; i < m_queryResults.size(); ++i)
                    {
                        stats << "\t" + m_queryNames[i] + ": " + std::to_string(m_queryResults[i]) + "\n";
                    }
                    m_statsText->SetText(stats.str());
                }
                m_canQuery   = true;
                m_queryIndex = (m_queryIndex + 1) % NUM_FRAMES_IN_FLIGHT;
            });
    }

    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
    VkQueryPool m_queryPool;
    std::shared_ptr<Text> m_statsText;
    std::vector<uint64_t> m_queryResults;
    std::vector<std::string> m_queryNames;
    bool m_canQuery      = false;
    uint8_t m_queryIndex = 0;
};
