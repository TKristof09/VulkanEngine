#pragma once

#include "Application.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Pipeline.hpp"
#include "ECS/Core.hpp"
#include "ECS/CoreComponents/RendererComponents.hpp"
#include "Utils/Math/Hilbert.hpp"
#include <glm/glm.hpp>
class GTAOPass
{
public:
    GTAOPass(RenderGraph& rg)
        : m_ecs(Application::GetInstance()->GetScene()->GetECS())
    {
        Buffer staging(64 * 64 * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
        staging.Fill(MathUtils::GenerateHilbertLUT(64).data(), 64 * 64 * sizeof(uint32_t));
        ImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.format          = VK_FORMAT_R32_UINT;
        imageCreateInfo.usage           = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageCreateInfo.aspectFlags     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCreateInfo.layout          = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageCreateInfo.debugName       = "hilbertLUT";
        m_hilbertLUT                    = std::make_unique<Image>(64, 64, imageCreateInfo);
        staging.CopyToImage(m_hilbertLUT->GetImage(), 64, 64);
        m_hilbertLUT->TransitionLayout(VK_IMAGE_LAYOUT_GENERAL);

        Application::GetInstance()->GetRenderer()->AddStorageImage(m_hilbertLUT.get());

        PipelineCreateInfo compute = {};
        compute.type               = PipelineType::COMPUTE;
        m_pipeline                 = std::make_unique<Pipeline>("gtao", compute);


        RegisterPass(rg);
    }

private:
    struct PushConstants
    {
        uint32_t depthTexture;
        uint32_t outAoTexture;  // R32 uint texture
        uint32_t hilbertLUT;
        uint32_t normalsTexture;

        glm::vec2 clipToViewSpaceConsts;
        glm::vec2 screenSize;
        float zNear;
    };


    void RegisterPass(RenderGraph& rg)
    {
        auto& pass         = rg.AddRenderPass("gtaoPass", QueueTypeFlagBits::Compute);
        auto& depthTexture = pass.AddTextureInput("depthImage");
        auto& aoTexture    = pass.AddStorageImageOutput("aoImage", VK_FORMAT_R32_SFLOAT);
        auto& normals      = pass.AddTextureInput("vsNormals");
        auto& hilbertLUT   = pass.AddStorageImageReadOnly("hilbertLUT", 0, true);

        hilbertLUT.SetImagePointer(m_hilbertLUT.get());

        Application::GetInstance()->GetRenderer()->AddDebugUIImage(aoTexture);


        pass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                const auto* mainCamera  = m_ecs->GetSingleton<MainCameraData>();
                const auto viewportSize = glm::vec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);

                m_pipeline->Bind(cb);

                PushConstants pc         = {};
                pc.zNear                 = mainCamera->zNear;
                pc.clipToViewSpaceConsts = mainCamera->clipToViewSpaceConsts;
                pc.screenSize            = viewportSize;

                pc.depthTexture   = depthTexture.GetImagePointer()->GetSampledSlot();
                pc.outAoTexture   = aoTexture.GetImagePointer()->GetStorageSlot();
                pc.hilbertLUT     = hilbertLUT.GetImagePointer()->GetStorageSlot();
                pc.normalsTexture = normals.GetImagePointer()->GetSampledSlot();

                m_pipeline->SetPushConstants(cb, &pc, sizeof(PushConstants));


                auto tileNums = glm::ivec2(ceil(viewportSize.x / 8.0f), ceil(viewportSize.y / 8.0f));
                vkCmdDispatch(cb.GetCommandBuffer(), tileNums.x, tileNums.y, 1);
            });
    }

    std::unique_ptr<Image> m_hilbertLUT;
    std::unique_ptr<Pipeline> m_pipeline;
    ECS* m_ecs;
};
