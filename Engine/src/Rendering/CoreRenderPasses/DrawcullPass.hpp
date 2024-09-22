#pragma once

#include "Application.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include "Rendering/Pipeline.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include <glm/glm.hpp>

class DrawcullPass
{
public:
    DrawcullPass(RenderGraph& rg)
    {
        LOG_WARN("Creating culling pipeline");
        PipelineCreateInfo cullPipeline;
        cullPipeline.type   = PipelineType::COMPUTE;
        cullPipeline.stages = VK_SHADER_STAGE_COMPUTE_BIT;
        m_cullPipeline      = std::make_unique<Pipeline>("drawcull", cullPipeline, 0);

        m_ecs = Application::GetInstance()->GetScene()->GetECS();
        RegisterPass(rg);

        auto button = std::make_shared<Button>("Freeze Frustum");
        button->RegisterCallback(
            [this](Button* button)
            {
                if(m_frozenFrustum)
                {
                    m_frozenFrustum = false;
                    button->SetName("Freeze Frustum");
                }
                else
                {
                    m_frozenFrustum = true;
                    button->SetName("Unfreeze Frustum");
                }
            });
        Application::GetInstance()->GetRenderer()->AddDebugUIElement(button);
    }

private:
    struct ShaderData
    {
        glm::vec4 frustumPlanes[5];
    };
    struct PushConstants
    {
        uint32_t inDrawCmdCount;
        uint64_t inDrawCmdPtr;
        uint64_t outDrawCmdPtr;

        uint64_t drawObjPtr;

        uint64_t boundingBoxes;

        uint64_t transformBufferPtr;

        glm::mat4 viewProj;
    };

    void RegisterPass(RenderGraph& rg)
    {
        auto& cullingPass   = rg.AddRenderPass("cullingPass", QueueTypeFlagBits::Compute);
        auto& outDrawBuffer = cullingPass.AddStorageBufferOutput("drawBuffer");
        auto& drawObjBuffer = cullingPass.AddStorageBufferOutput("drawObjBuffer");

        cullingPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                const MainCameraData* camera = m_ecs->GetSingleton<MainCameraData>();
                const ShaderData shaderData{
                    .frustumPlanes = {
                                      camera->frustumPlanesVS[0],
                                      camera->frustumPlanesVS[1],
                                      camera->frustumPlanesVS[2],
                                      camera->frustumPlanesVS[3],
                                      camera->frustumPlanesVS[4],
                                      }
                };

                m_cullPipeline->UploadShaderData(&shaderData, imageIndex);

                const auto* drawCmds          = m_ecs->GetSingleton<DrawCommandBuffer>();
                const auto* boundingBoxBuffer = m_ecs->GetSingleton<BoundingBoxBuffer>();
                PushConstants pc{
                    .inDrawCmdCount     = drawCmds->count,
                    .inDrawCmdPtr       = drawCmds->buffer.GetDeviceAddress(0),
                    .outDrawCmdPtr      = outDrawBuffer.GetBufferPointer()->GetDeviceAddress(),
                    .drawObjPtr         = drawObjBuffer.GetBufferPointer()->GetDeviceAddress(),
                    .boundingBoxes      = boundingBoxBuffer->buffer.GetDeviceAddress(0),
                    .transformBufferPtr = m_ecs->GetSingleton<TransformBuffers>()
                                              ->buffers[imageIndex]
                                              .GetDeviceAddress(0),
                    .viewProj = m_frozenFrustum ? m_lastVP : camera->viewProj,
                };


                m_cullPipeline->Bind(cb);
                m_cullPipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));

                vkCmdDispatch(cb.GetCommandBuffer(), 10, 1, 1);

                if(!m_frozenFrustum)
                    m_lastVP = camera->viewProj;
            });
    }

    std::unique_ptr<Pipeline> m_cullPipeline;
    ECS* m_ecs;
    glm::mat4 m_lastVP;
    bool m_frozenFrustum = false;
};
