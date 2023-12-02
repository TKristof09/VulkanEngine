#include "Rendering/Renderer.hpp"

#include <memory>
#include <numeric>
#include <sstream>
#include <optional>
#include <set>
#include <map>
#include <limits>
#include <chrono>
#include <array>
#include <string>
#include <math.h>
#include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <utility>

#include "Application.hpp"
#include "Core/Events/EventHandler.hpp"

#include "Rendering/TextureManager.hpp"
#include "Rendering/Image.hpp"
#include "Rendering/VulkanContext.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/RenderGraph/RenderPass.hpp"


#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Lights.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Renderable.hpp"
#include "ECS/CoreComponents/Material.hpp"


const uint32_t MAX_FRAMES_IN_FLIGHT = 2;


struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentationFamily;
    std::optional<uint32_t> computeFamily;

    [[nodiscard]] bool IsComplete() const
    {
        return graphicsFamily.has_value() && presentationFamily.has_value() && computeFamily.has_value();
    }
};
struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
std::vector<const char*> GetExtensions();
VkPhysicalDevice PickDevice(const std::vector<VkPhysicalDevice>& devices, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
int RateDevice(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice device);
QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                     VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData, void* /*pUserData*/);


Renderer::Renderer(std::shared_ptr<Window> window)
    : m_ecs(Application::GetInstance()->GetScene()->GetECS()),
      m_window(std::move(window)),
      m_instance(VulkanContext::m_instance),
      m_gpu(VulkanContext::m_gpu),
      m_device(VulkanContext::m_device),
      m_graphicsQueue(VulkanContext::m_graphicsQueue),
      m_commandPool(VulkanContext::m_commandPool),
      m_renderGraph(RenderGraph(this)),
      m_vertexBuffer(5e6, sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 5e5),
      m_indexBuffer(5e6, sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 5e6),

      m_shaderDataBuffer(1e5, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 1e3, true),  // objectSize = 1 byte because each shader data can be diff size so we "store them as bytes"

      m_pushConstants({}),
      m_freeTextureSlots(NUM_TEXTURE_DESCRIPTORS),
      m_shadowIndices(100, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 100, false)
{
    m_mainCameraQuery        = m_ecs->StartQueryBuilder<const Camera, const InternalTransform>("MainCameraQuery").build();
    m_transformsQuery        = m_ecs->StartQueryBuilder<const InternalTransform, const Renderable>("TransformsQuery").build();
    m_renderablesQuery       = m_ecs->StartQueryBuilder<const Renderable>("RenderablesQuery").build();
    m_directionalLightsQuery = m_ecs->StartQueryBuilder<const DirectionalLight, const InternalTransform>("DirectionalLightsQuery").build();
    m_pointLightsQuery       = m_ecs->StartQueryBuilder<const PointLight, const InternalTransform>("PointLightsQuery").build();
    m_spotLightsQuery        = m_ecs->StartQueryBuilder<const SpotLight, const InternalTransform>("SpotLightsQuery").build();


    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Renderer::OnSceneSwitched);
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Renderer::OnMeshComponentAdded);
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Renderer::OnMeshComponentRemoved);
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Renderer::OnMaterialComponentAdded);
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Renderer::OnDirectionalLightAdded);
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Renderer::OnPointLightAdded);
    Application::GetInstance()->GetEventHandler()->Subscribe(this, &Renderer::OnSpotLightAdded);

    // m_shadowMatricesBuffers.reserve(NUM_FRAMES_IN_FLIGHT);
    // m_transformBuffers.reserve(NUM_FRAMES_IN_FLIGHT);
    for(int32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
    {
        m_shadowMatricesBuffers.emplace_back(100, sizeof(ShadowMatrices), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 100, true);
        m_transformBuffers.emplace_back(5e5, sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 1e5, true);
    }

    // fill in the free texture slots
    std::iota(m_freeTextureSlots.begin(), m_freeTextureSlots.end(), 0);

    CreateInstance();
    SetupDebugMessenger();
    CreateDevice();
    CreateSwapchain();
    CreateVmaAllocator();

    m_changedLights.resize(m_swapchainImages.size());


    CreateCommandPool();

    TextureManager::LoadTexture("./textures/error.jpg");

    CreateColorResources();
    CreateDepthResources();
    CreateUniformBuffers();

    CreateSampler();

    CreateDescriptorPool();
    CreateDescriptorSet();
    CreatePushConstants();

    CreatePipeline();
    CreateDebugUI();
    CreateCommandBuffers();
    CreateSyncObjects();

    vkDeviceWaitIdle(m_device);
    CreateEnvironmentMap();


    m_rendererDebugWindow = std::make_unique<DebugUIWindow>("Renderer");
    AddDebugUIWindow(m_rendererDebugWindow.get());


    m_drawBuffer.Allocate(1000 * sizeof(DrawCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, true);  // TODO change to non mappable and use staging buffer

    // TODO: temporary, remove later
    {
        auto& depthPass                   = m_renderGraph.AddRenderPass("depthPass", QueueTypeFlagBits::Graphics);
        AttachmentInfo depthInfo          = {};
        depthInfo.clear                   = true;
        depthInfo.clearValue.depthStencil = {0.0f, 0};
        depthPass.AddDepthOutput("depthImage", depthInfo);
        depthPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), depthPass.GetRenderingInfo());


                vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_pipeline);
                PROFILE_SCOPE("Prepass draw call loop");
                m_pushConstants.transformBufferPtr = m_transformBuffers[imageIndex].GetDeviceAddress(0);

                vkCmdPushConstants(cb.GetCommandBuffer(), m_depthPipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);
                m_vertexBuffer.Bind(cb);
                m_indexBuffer.Bind(cb);
                vkCmdDrawIndexedIndirect(cb.GetCommandBuffer(), m_drawBuffer.GetVkBuffer(), 0, m_numDrawCommands, sizeof(DrawCommand));

                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }
    {
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
        auto& lightingPass         = m_renderGraph.AddRenderPass("lightingPass", QueueTypeFlagBits::Graphics);
        AttachmentInfo colorInfo   = {};
        colorInfo.clear            = true;
        colorInfo.clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
        lightingPass.AddColorOutput("colorImage", colorInfo);
        lightingPass.AddDepthInput("depthImage");
        auto& visibleLightsBuffer = lightingPass.AddStorageBufferReadOnly("visibleLightsBuffer", VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, true);
        visibleLightsBuffer.SetBufferPointer(m_visibleLightsBuffers[0].get());
        auto& lightingDrawCommands = lightingPass.AddDrawCommandBuffer("lightingDrawCommands");
        auto& shadowMaps           = lightingPass.AddTextureArrayInput("shadowMaps", VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

        lightingPass.SetInitialiseCallback(
            [&](RenderGraph& rg)
            {
                ShaderData data             = {};
                data.viewportSize           = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums               = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                data.visibleLightsBuffer    = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                // TODO temp
                data.shadowMapIds           = m_shadowIndices.GetDeviceAddress(0);
                data.irradianceMapIndex     = m_convEnvMap->GetSlot();
                data.prefilteredEnvMapIndex = m_prefilteredEnvMap->GetSlot();
                data.BRDFLUTIndex           = m_BRDFLUT->GetSlot();
                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer          = m_lightsBuffers[i]->GetDeviceAddress(0);
                    data.shadowMatricesBuffer = m_shadowMatricesBuffers[i].GetDeviceAddress(0);
                    //  data.shadowMapIds
                    //  data.shadowMapCount
                    uint64_t offset           = m_shaderDataBuffer.Allocate(sizeof(ShaderData));
                    m_shaderDataBuffer.UploadData(offset, &data);
                    // store the offset somewhere
                    m_shaderDataOffsets[i]["forwardplus"] = offset;
                }
            });
        lightingPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                // TODO temporary
                ShaderData data             = {};
                data.viewportSize           = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums               = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                data.visibleLightsBuffer    = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                // TODO temp
                data.shadowMapIds           = m_shadowIndices.GetDeviceAddress(0);
                data.irradianceMapIndex     = m_convEnvMap->GetSlot();
                data.prefilteredEnvMapIndex = m_prefilteredEnvMap->GetSlot();
                data.BRDFLUTIndex           = m_BRDFLUT->GetSlot();

                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer          = m_lightsBuffers[i]->GetDeviceAddress(0);
                    data.shadowMapCount       = m_numShadowIndices;
                    data.shadowMatricesBuffer = m_shadowMatricesBuffers[i].GetDeviceAddress(0);
                    m_shaderDataBuffer.UploadData(m_shaderDataOffsets[i]["forwardplus"], &data);
                }

                vkCmdBeginRendering(cb.GetCommandBuffer(), lightingPass.GetRenderingInfo());


                auto* pipeline = m_pipelinesRegistry["forwardplus"];
                // vkCmdPushConstants(m_mainCommandBuffers[imageIndex].GetCommandBuffer(), pipeline.m_layout,

                vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->m_pipeline);

                m_pushConstants.transformBufferPtr = m_transformBuffers[imageIndex].GetDeviceAddress(0);
                m_pushConstants.shaderDataPtr      = m_shaderDataBuffer.GetDeviceAddress(m_shaderDataOffsets[imageIndex]["forwardplus"]);
                m_pushConstants.materialDataPtr    = pipeline->GetMaterialBufferPtr();

                vkCmdPushConstants(cb.GetCommandBuffer(), pipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);

                VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
                vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->m_layout, 0, 1, &descSet, 0, nullptr);

                m_vertexBuffer.Bind(cb);
                m_indexBuffer.Bind(cb);

                vkCmdDrawIndexedIndirect(cb.GetCommandBuffer(), m_drawBuffer.GetVkBuffer(), 0, m_numDrawCommands, sizeof(DrawCommand));

                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }
    {
        struct ShaderData
        {
            uint64_t lightBuffer;
            uint64_t shadowMatricesBuffer;
        };
        auto& shadowPass = m_renderGraph.AddRenderPass("shadowPass", QueueTypeFlagBits::Graphics);
        // auto& shadowMatricesBuffer = shadowPass.AddStorageBufferReadOnly("shadowMatrices", VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, true);

        m_shadowMaps = &shadowPass.AddTextureArrayOutput("shadowMaps", VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        m_shadowMaps->SetFormat(VK_FORMAT_D32_SFLOAT);
        shadowPass.SetInitialiseCallback(
            [&](RenderGraph& rg)
            {
                ShaderData data = {};
                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer          = m_lightsBuffers[i]->GetDeviceAddress(0);
                    data.shadowMatricesBuffer = m_shadowMatricesBuffers[i].GetDeviceAddress(0);
                    //  data.shadowMapIds
                    //  data.shadowMapCount
                    bool tmp                  = false;
                    uint64_t offset           = m_shaderDataBuffer.Allocate(sizeof(ShaderData), tmp);
                    m_shaderDataBuffer.UploadData(offset, &data);
                    // store the offset somewhere
                    m_shaderDataOffsets[i]["shadow"] = offset;
                }
            });
        shadowPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                // TODO: how would this work with multiple lights? we somehow need to associate light with the corresponding image
                for(const auto* img : m_shadowMaps->GetImagePointers())
                {
                    // set up the renderingInfo struct
                    VkRenderingInfo rendering          = {};
                    rendering.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    rendering.renderArea.extent.width  = img->GetWidth();
                    rendering.renderArea.extent.height = img->GetHeight();
                    // rendering.layerCount               = 1;
                    rendering.viewMask                 = m_shadowPipeline->GetViewMask();

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

                    vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_pipeline);

                    m_pushConstants.debugMode          = 0;
                    m_pushConstants.transformBufferPtr = m_transformBuffers[imageIndex].GetDeviceAddress(0);
                    m_pushConstants.shaderDataPtr      = m_shaderDataBuffer.GetDeviceAddress(m_shaderDataOffsets[imageIndex]["shadow"]);
                    m_pushConstants.materialDataPtr    = 0;

                    vkCmdPushConstants(cb.GetCommandBuffer(), m_shadowPipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);

                    VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
                    vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_layout, 0, 1, &descSet, 0, nullptr);

                    m_vertexBuffer.Bind(cb);
                    m_indexBuffer.Bind(cb);

                    vkCmdDrawIndexedIndirect(cb.GetCommandBuffer(), m_drawBuffer.GetVkBuffer(), 0, m_numDrawCommands, sizeof(DrawCommand));
                    vkCmdEndRendering(cb.GetCommandBuffer());
                }
            });
    }
    {
        struct ShaderData
        {
            glm::ivec2 viewportSize;
            glm::ivec2 tileNums;
            uint64_t lightBuffer;
            uint64_t visibleLightsBuffer;

            int lightNum;
            int depthTextureId;
            int debugTextureId;
        };
        auto& lightCullPass       = m_renderGraph.AddRenderPass("lightCullPass", QueueTypeFlagBits::Compute);
        auto& visibleLightsBuffer = lightCullPass.AddStorageBufferOutput("visibleLightsBuffer", "", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, true);
        auto& depthTexture        = lightCullPass.AddTextureInput("depthImage");
        lightCullPass.SetInitialiseCallback(
            [&](RenderGraph& rg)
            {
                ShaderData data          = {};
                data.viewportSize        = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums            = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                data.visibleLightsBuffer = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                data.lightNum            = m_lightMap.size();
                data.depthTextureId      = depthTexture.GetImagePointer()->GetSlot();
                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer = m_lightsBuffers[i]->GetDeviceAddress(0);
                    //  data.shadowMapIds
                    //  data.shadowMapCount
                    uint64_t offset  = m_shaderDataBuffer.Allocate(sizeof(ShaderData));
                    m_shaderDataBuffer.UploadData(offset, &data);
                    // store the offset somewhere
                    m_shaderDataOffsets[i]["lightCull"] = offset;
                }
            });
        lightCullPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                ShaderData data          = {};
                data.viewportSize        = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums            = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                data.visibleLightsBuffer = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                data.lightNum            = m_lightMap.size();
                data.depthTextureId      = depthTexture.GetImagePointer()->GetSlot();
                data.lightBuffer         = m_lightsBuffers[imageIndex]->GetDeviceAddress(0);
                m_shaderDataBuffer.UploadData(m_shaderDataOffsets[imageIndex]["lightCull"], &data);

                uint32_t offset = 0;
                vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_pipeline);

                VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
                vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_layout, 0, 1, &descSet, 0, nullptr);
                m_pushConstants.transformBufferPtr = m_transformBuffers[imageIndex].GetDeviceAddress(0);
                m_pushConstants.shaderDataPtr      = m_shaderDataBuffer.GetDeviceAddress(m_shaderDataOffsets[imageIndex]["lightCull"]);


                auto viewportSize = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                auto tileNums     = glm::ivec2(ceil(viewportSize.x / 16.0f), ceil(viewportSize.y / 16.0f));
                vkCmdPushConstants(cb.GetCommandBuffer(), m_compute->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);
                vkCmdDispatch(cb.GetCommandBuffer(), tileNums.x, tileNums.y, 1);
            });
    }
    {
        auto& skyboxPass = m_renderGraph.AddRenderPass("skyboxPass", QueueTypeFlagBits::Graphics);
        skyboxPass.AddColorOutput("skyboxImage", {}, "colorImage");
        skyboxPass.AddDepthInput("depthImage");
        skyboxPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), skyboxPass.GetRenderingInfo());

                vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline->m_pipeline);
                VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
                vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline->m_layout, 0, 1, &descSet, 0, nullptr);
                m_pushConstants.data[0] = m_envMap->GetSlot();
                vkCmdPushConstants(cb.GetCommandBuffer(), m_skyboxPipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);
                vkCmdDraw(cb.GetCommandBuffer(), 6, 1, 0, 0);
                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }
    {
        auto& uiPass = m_renderGraph.AddRenderPass("uiPass", QueueTypeFlagBits::Graphics);
        uiPass.AddColorOutput(SWAPCHAIN_RESOURCE_NAME, {}, "skyboxImage");
        uiPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t frameIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), uiPass.GetRenderingInfo());
                m_debugUI->Draw(&cb);
                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }
    m_renderGraph.Build();


    vkDeviceWaitIdle(VulkanContext::GetDevice());
}

Renderer::~Renderer()
{
    vkDeviceWaitIdle(m_device);
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    for(auto& pool : m_queryPools)
    {
        vkDestroyQueryPool(m_device, pool, nullptr);
    }
    TextureManager::ClearLoadedTextures();

    CleanupSwapchain();


    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
}

void Renderer::RecreateSwapchain()
{
    while(m_window->GetWidth() == 0 || m_window->GetHeight() == 0)
        glfwWaitEvents();


    vkDeviceWaitIdle(m_device);

    LOG_INFO("Recreating swapchain...");

    CleanupSwapchain();

    vkDeviceWaitIdle(m_device);

    CreateSwapchain();
    // CreatePipeline();
    CreateColorResources();
    CreateDepthResources();
    CreateCommandBuffers();


    DebugUIInitInfo initInfo = {};
    initInfo.pWindow         = m_window;


    QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
    initInfo.queueFamily        = families.graphicsFamily.value();
    initInfo.queue              = m_graphicsQueue;

    initInfo.pipelineCache  = nullptr;
    initInfo.descriptorPool = m_descriptorPool;
    initInfo.imageCount     = static_cast<uint32_t>(m_swapchainImages.size());
    initInfo.msaaSamples    = m_msaaSamples;
    initInfo.allocator      = nullptr;
    m_debugUI->ReInit(initInfo);

    // m_computePushConstants.viewportSize = {m_swapchainExtent.width, m_swapchainExtent.height};
    // m_computePushConstants.tileNums     = glm::ceil(glm::vec2(m_computePushConstants.viewportSize) / 16.f);
}

void Renderer::CleanupSwapchain()
{
    vkDeviceWaitIdle(m_device);

    m_depthImage.reset();
    m_colorImage.reset();


    m_mainCommandBuffers.clear();


    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    m_swapchainImages.clear();
}

void Renderer::CreateDebugUI()
{
    DebugUIInitInfo initInfo = {};
    initInfo.pWindow         = m_window;

    QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);
    initInfo.queueFamily        = families.graphicsFamily.value();
    initInfo.queue              = m_graphicsQueue;

    initInfo.pipelineCache  = nullptr;
    initInfo.descriptorPool = m_descriptorPool;
    initInfo.imageCount     = static_cast<uint32_t>(m_swapchainImages.size());
    initInfo.msaaSamples    = m_msaaSamples;
    initInfo.allocator      = nullptr;

    m_debugUI = std::make_shared<DebugUI>(initInfo);
}

void Renderer::CreateInstance()
{
    VkApplicationInfo appinfo = {};
    appinfo.sType             = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appinfo.pApplicationName  = "VulkanEngine";
    appinfo.apiVersion        = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo     = &appinfo;

    auto extensions                    = GetExtensions();
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());

#ifdef VDEBUG


    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    // Check if this layer is available at instance level
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> instanceLayerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, instanceLayerProperties.data());

    bool validationLayerPresent = false;
    LOG_TRACE("Available layers");
    for(VkLayerProperties layer : instanceLayerProperties)
    {
        LOG_TRACE("    {0}", layer.layerName);
        if(strcmp(layer.layerName, validationLayerName) == 0)
        {
            validationLayerPresent = true;
            break;
        }
    }
    if(validationLayerPresent)
    {
        createInfo.ppEnabledLayerNames = &validationLayerName;
        createInfo.enabledLayerCount   = 1;
    }
    else
    {
        std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
    }


    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    debugCreateInfo.sType                              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity                    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType                        = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback                    = debugUtilsMessengerCallback;


    // VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT}
    // Disable sync validation for now because of false positive when using bindless texture array but not actually accessing a texture, workaround to this in the validation layer is coming in a future SDK release, see https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/3450
    std::vector<VkValidationFeatureEnableEXT> enables = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT /*, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT*/};


    VkValidationFeaturesEXT features       = {};
    features.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.enabledValidationFeatureCount = enables.size();
    features.pEnabledValidationFeatures    = enables.data();

    debugCreateInfo.pNext = &features;
    createInfo.pNext      = &debugCreateInfo;
#endif

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create instance");

    VK_CHECK(glfwCreateWindowSurface(m_instance, m_window->GetWindow(), nullptr, &m_surface), "Failed ot create window surface!");
}

void Renderer::SetupDebugMessenger()
{
#ifndef VDEBUG
    return;
#endif
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType                              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity                    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType                        = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback                    = debugUtilsMessengerCallback;

    CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &VulkanContext::m_messenger);
}

void Renderer::CreateDevice()
{
    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef VDEBUG
    deviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif

    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    m_gpu = PickDevice(devices, m_surface, deviceExtensions);

    vkGetPhysicalDeviceProperties(m_gpu, &VulkanContext::m_gpuProperties);
    LOG_TRACE("Picked device: {0}", VulkanContext::m_gpuProperties.deviceName);

    m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;  // GetMaxUsableSampleCount(m_gpu); MSAA causes flickering bug

    QueueFamilyIndices families = FindQueueFamilies(m_gpu, m_surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {families.graphicsFamily.value(), families.presentationFamily.value(), families.computeFamily.value()};

    for(auto queue : uniqueQueueFamilies)
    {
        float queuePriority                     = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex        = queue;
        queueCreateInfo.queueCount              = 1;
        queueCreateInfo.pQueuePriorities        = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy        = VK_TRUE;
    deviceFeatures.sampleRateShading        = VK_TRUE;
    deviceFeatures.shaderInt64              = VK_TRUE;
    deviceFeatures.multiDrawIndirect        = VK_TRUE;

    // deviceFeatures.depthBounds = VK_TRUE; //doesnt work on my surface 2017

    VkPhysicalDeviceVulkan11Features device11Features = {};
    device11Features.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    device11Features.shaderDrawParameters             = VK_TRUE;
    device11Features.multiview                        = VK_TRUE;

    VkPhysicalDeviceVulkan12Features device12Features             = {};
    device12Features.sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    // these are for dynamic descriptor indexing
    deviceFeatures.shaderSampledImageArrayDynamicIndexing         = VK_TRUE;
    deviceFeatures.shaderStorageBufferArrayDynamicIndexing        = VK_TRUE;
    device12Features.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
    device12Features.runtimeDescriptorArray                       = VK_TRUE;
    device12Features.descriptorBindingPartiallyBound              = VK_TRUE;
    device12Features.descriptorBindingUpdateUnusedWhilePending    = VK_TRUE;
    device12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    device12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    device12Features.bufferDeviceAddress                          = VK_TRUE;

    VkPhysicalDeviceVulkan13Features device13Features = {};
    device13Features.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    device13Features.dynamicRendering                 = VK_TRUE;
    device13Features.maintenance4                     = VK_TRUE;  // need it because the spirv compiler uses localsizeid even though it doesnt need to for now, but i might switch to spec constants in the future anyway
    device13Features.synchronization2                 = VK_TRUE;


    VkDeviceCreateInfo createInfo      = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());

    createInfo.pNext       = &device11Features;
    device11Features.pNext = &device12Features;
    device12Features.pNext = &device13Features;

    VK_CHECK(vkCreateDevice(m_gpu, &createInfo, nullptr, &m_device), "Failed to create device");
    vkGetDeviceQueue(m_device, families.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, families.presentationFamily.value(), 0, &m_presentQueue);
    vkGetDeviceQueue(m_device, families.computeFamily.value(), 0, &m_computeQueue);


    m_queryPools.resize(MAX_FRAMES_IN_FLIGHT);
    m_queryResults.resize(MAX_FRAMES_IN_FLIGHT);
    for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkQueryPoolCreateInfo queryCI = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        queryCI.queryType             = VK_QUERY_TYPE_TIMESTAMP;
        queryCI.queryCount            = 4;
        vkCreateQueryPool(m_device, &queryCI, nullptr, &m_queryPools[i]);
    }
    m_timestampPeriod = VulkanContext::m_gpuProperties.limits.timestampPeriod;
}

void Renderer::CreateVmaAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = m_gpu;
    allocatorInfo.device                 = m_device;
    allocatorInfo.instance               = m_instance;
    allocatorInfo.vulkanApiVersion       = VK_API_VERSION_1_3;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &VulkanContext::m_vmaImageAllocator), "Failed to create vma allocator");
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &VulkanContext::m_vmaBufferAllocator), "Failed to create vma allocator");
}

void Renderer::CreateSwapchain()
{
    SwapchainSupportDetails details = QuerySwapChainSupport(m_gpu, m_surface);

    auto surfaceFormat = ChooseSwapchainFormat(details.formats);
    auto extent        = ChooseSwapchainExtent(details.capabilities, m_window->GetWindow());
    auto presentMode   = ChooseSwapchainPresentMode(details.presentModes);

    VulkanContext::m_swapchainExtent = extent;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface                  = m_surface;
    createInfo.imageFormat              = surfaceFormat.format;
    createInfo.minImageCount            = details.capabilities.minImageCount + 1;
    createInfo.imageColorSpace          = surfaceFormat.colorSpace;
    createInfo.imageExtent              = extent;
    createInfo.presentMode              = presentMode;
    createInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageArrayLayers         = 1;

    QueueFamilyIndices indices    = FindQueueFamilies(m_gpu, m_surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentationFamily.value()};

    if(indices.graphicsFamily != indices.presentationFamily)
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;  // if the two queues are different then we need to share between them
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;  // this is the better performance
        createInfo.queueFamilyIndexCount = 0;                          // Optional
        createInfo.pQueueFamilyIndices   = nullptr;                    // Optional
    }
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.preTransform   = details.capabilities.currentTransform;
    createInfo.clipped        = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "Failed to create swapchain");

    uint32_t imageCount;
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr), "Failed to get swapchain images");
    std::vector<VkImage> tempImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, tempImages.data()), "Failed to get swapchain images");
    m_swapchainExtent                     = extent;
    m_swapchainImageFormat                = surfaceFormat.format;
    VulkanContext::m_swapchainImageFormat = m_swapchainImageFormat;

    m_renderGraph.SetupSwapchainImages(tempImages);
    // TODO remove
    for(size_t i = 0; i < tempImages.size(); i++)
    {
        ImageCreateInfo ci = {};
        ci.image           = tempImages[i];
        ci.aspectFlags     = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.format          = m_swapchainImageFormat;

        m_swapchainImages.emplace_back(std::make_shared<Image>(extent, ci));
    }

    LOG_TRACE("Created swapchain and images");
}


Pipeline* Renderer::AddPipeline(const std::string& name, PipelineCreateInfo createInfo, uint32_t priority)
{
    LOG_TRACE("Creating pipeline for {0}", name);

    createInfo.viewportExtent = m_swapchainExtent;
    createInfo.msaaSamples    = m_msaaSamples;
    auto it                   = m_pipelines.emplace(name, createInfo, priority);
    Pipeline* pipeline        = const_cast<Pipeline*>(&(*it));
    m_pipelinesRegistry[name] = pipeline;

    bool didResize = false;
    // uint64_t offset = m_shaderDataBuffer.Allocate(pipeline->m_shaderDataSize, didResize, pipeline);

    // m_shaderDataOffsets[pipeline->m_name] = offset;

    if(didResize)
    {
        RefreshShaderDataOffsets();
    }

    return pipeline;
}
void Renderer::CreatePipeline()
{
    {
        // Compute
        PipelineCreateInfo compute = {};
        compute.type               = PipelineType::COMPUTE;
        m_compute                  = std::make_unique<Pipeline>("lightCulling", compute);
    }
    {
        // Depth prepass pipeline
        LOG_WARN("Creating depth pipeline");
        PipelineCreateInfo depthPipeline;
        depthPipeline.type             = PipelineType::GRAPHICS;
        depthPipeline.useColor         = false;
        // depthPipeline.parent			= const_cast<Pipeline*>(&(*mainIter));
        depthPipeline.depthCompareOp   = VK_COMPARE_OP_GREATER;
        depthPipeline.depthWriteEnable = true;
        depthPipeline.useDepth         = true;
        depthPipeline.depthClampEnable = false;
        depthPipeline.msaaSamples      = m_msaaSamples;
        depthPipeline.stages           = VK_SHADER_STAGE_VERTEX_BIT;
        depthPipeline.useMultiSampling = true;
        depthPipeline.useColorBlend    = false;
        depthPipeline.viewportExtent   = m_swapchainExtent;

        depthPipeline.isGlobal = true;

        // m_depthPipeline = AddPipeline("depth", depthPipeline, 0);
        m_depthPipeline = std::make_unique<Pipeline>("depth", depthPipeline, 0);
    }
    uint32_t totaltiles = ceil(m_swapchainExtent.width / 16.f) * ceil(m_swapchainExtent.height / 16.f);

    for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        for(auto& [name, bufferInfo] : m_depthPipeline->m_uniformBuffers)
        {
            /*
            if(m_ubAllocators[m_depthPipeline->m_name + name + std::to_string(i)] == nullptr)
                m_ubAllocators[m_depthPipeline->m_name + name + std::to_string(i)] = std::move(std::make_unique<BufferAllocator>(bufferInfo.size, OBJECTS_PER_DESCRIPTOR_CHUNK, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
            */
        }

        // TODO
        m_lightsBuffers.emplace_back(std::make_unique<DynamicBufferAllocator>(100, sizeof(Light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0, true));  // MAX_LIGHTS_PER_TILE
        m_visibleLightsBuffers.emplace_back(std::make_unique<Buffer>(totaltiles * sizeof(TileLights), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));       // MAX_LIGHTS_PER_TILE
    }

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

        m_shadowPipeline = std::make_unique<Pipeline>("shadow", shadowPipeline);  // depth shader is fine for the shadow maps too since we only need the depth info from it
    }

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
    }

    {
        // Equirectangular to cubemap pipeline
        LOG_WARN("Creating equiToCube pipeline");
        PipelineCreateInfo ci;
        ci.type             = PipelineType::GRAPHICS;
        ci.useColor         = true;
        ci.colorFormats     = {VK_FORMAT_R32G32B32A32_SFLOAT};
        ci.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
        ci.depthWriteEnable = false;
        ci.useDepth         = false;
        ci.depthClampEnable = false;
        ci.msaaSamples      = VK_SAMPLE_COUNT_1_BIT;
        ci.stages           = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ci.useMultiSampling = false;
        ci.useColorBlend    = false;
        ci.viewportExtent   = {512, 512};
        ci.viewMask         = 0b111111;  // one for each face of the cube

        m_equiToCubePipeline = std::make_unique<Pipeline>("equiToCube", ci);
    }
    {
        // environment map convolution pipeline
        LOG_WARN("Creating convolution pipeline");
        PipelineCreateInfo ci;
        ci.type             = PipelineType::GRAPHICS;
        ci.useColor         = true;
        ci.colorFormats     = {VK_FORMAT_R32G32B32A32_SFLOAT};
        ci.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
        ci.depthWriteEnable = false;
        ci.useDepth         = false;
        ci.depthClampEnable = false;
        ci.msaaSamples      = VK_SAMPLE_COUNT_1_BIT;
        ci.stages           = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ci.useMultiSampling = false;
        ci.useColorBlend    = false;
        ci.viewportExtent   = {512, 512};
        ci.viewMask         = 0b111111;  // one for each face of the cube

        m_convoltionPipeline = std::make_unique<Pipeline>("convolution", ci);
    }

    {
        // environment map prefilter pipeline
        LOG_WARN("Creating prefilter pipeline");
        PipelineCreateInfo ci;
        ci.type               = PipelineType::GRAPHICS;
        ci.useColor           = true;
        ci.colorFormats       = {VK_FORMAT_R32G32B32A32_SFLOAT};
        ci.depthCompareOp     = VK_COMPARE_OP_ALWAYS;
        ci.depthWriteEnable   = false;
        ci.useDepth           = false;
        ci.depthClampEnable   = false;
        ci.msaaSamples        = VK_SAMPLE_COUNT_1_BIT;
        ci.stages             = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ci.useMultiSampling   = false;
        ci.useColorBlend      = false;
        ci.useDynamicViewport = true;
        ci.viewMask           = 0b111111;  // one for each face of the cube

        m_prefilterPipeline = std::make_unique<Pipeline>("prefilterEnv", ci);
    }

    {
        // BRDF LUT pipeline
        LOG_WARN("Creating BRDF LUT pipeline");
        PipelineCreateInfo ci;
        ci.type             = PipelineType::GRAPHICS;
        ci.useColor         = true;
        ci.colorFormats     = {VK_FORMAT_R16G16_SFLOAT};
        ci.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
        ci.depthWriteEnable = false;
        ci.useDepth         = false;
        ci.depthClampEnable = false;
        ci.msaaSamples      = VK_SAMPLE_COUNT_1_BIT;
        ci.stages           = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ci.useMultiSampling = false;
        ci.useColorBlend    = false;
        ci.viewportExtent   = {512, 512};

        m_computeBRDFPipeline = std::make_unique<Pipeline>("computeBRDF", ci);
    }
}


void Renderer::CreateCommandPool()
{
    QueueFamilyIndices families        = FindQueueFamilies(m_gpu, m_surface);
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex        = families.graphicsFamily.value();
    createInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool), "Failed to create command pool");
}

void Renderer::CreateCommandBuffers()
{
    for(size_t i = 0; i < m_swapchainImages.size(); i++)
    {
        m_mainCommandBuffers.push_back(CommandBuffer());
    }
}

void Renderer::CreateSyncObjects()
{
    m_imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinished.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(m_swapchainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailable[i]), "Failed to create semaphore");
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinished[i]), "Failed to create semaphore");

        VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]), "Failed to create fence");
    }
}

void Renderer::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0].type                             = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount                  = NUM_TEXTURE_DESCRIPTORS + 100;  // +100 for imgui
    poolSizes[1].type                             = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount                  = NUM_TEXTURE_DESCRIPTORS;


    VkDescriptorPoolCreateInfo createInfo = {};
    createInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount              = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes                 = poolSizes.data();
    createInfo.maxSets                    = 100;  // we only use 1 but imgui also allocates 1 for each of our DebugUIImage elements
    createInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    VK_CHECK(vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool), "Failed to create descriptor pool");
}

void Renderer::CreateDescriptorSet()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = NUM_TEXTURE_DESCRIPTORS;
    bindings[0].stageFlags      = VK_SHADER_STAGE_ALL;
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = NUM_TEXTURE_DESCRIPTORS;
    bindings[1].stageFlags      = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags = {};
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
    flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;

    flags.pBindingFlags = bindingFlags.data();
    flags.bindingCount  = bindingFlags.size();

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount                    = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings                       = bindings.data();
    createInfo.flags                           = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    createInfo.pNext = &flags;

    VK_CHECK(vkCreateDescriptorSetLayout(VulkanContext::GetDevice(), &createInfo, nullptr, &VulkanContext::m_globalDescSetLayout), "Failed to create descriptor set layout");


    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool              = m_descriptorPool;
    allocInfo.descriptorSetCount          = 1;
    VkDescriptorSetLayout layout          = VulkanContext::m_globalDescSetLayout;
    allocInfo.pSetLayouts                 = &layout;

    VK_CHECK(vkAllocateDescriptorSets(VulkanContext::GetDevice(), &allocInfo, &VulkanContext::m_globalDescSet), "Failed to allocate descriptor set for textures");
}

void Renderer::CreatePushConstants()
{
    VulkanContext::m_globalPushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
    VulkanContext::m_globalPushConstantRange.offset     = 0;
    VulkanContext::m_globalPushConstantRange.size       = sizeof(PushConstants);
}

void Renderer::CreateUniformBuffers()
{
    /*
    for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        m_ubAllocators["transforms" + std::to_string(i)] = std::move(std::make_unique<BufferAllocator>(sizeof(glm::mat4), OBJECTS_PER_DESCRIPTOR_CHUNK, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
        m_ubAllocators["camera" + std::to_string(i)]     = std::move(std::make_unique<BufferAllocator>(sizeof(CameraStruct), OBJECTS_PER_DESCRIPTOR_CHUNK, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

        m_ubAllocators["camera" + std::to_string(i)]->Allocate();
    }*/
}

void Renderer::CreateColorResources()
{
    ImageCreateInfo ci;
    ci.format      = m_swapchainImageFormat;
    ci.usage       = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.layout      = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.msaaSamples = m_msaaSamples;
    ci.useMips     = false;
    m_colorImage   = std::make_shared<Image>(m_swapchainExtent, ci);
}

void Renderer::CreateDepthResources()
{
    ImageCreateInfo ci    = {};
    ci.format             = VK_FORMAT_B8G8R8A8_UNORM;
    ci.usage              = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.aspectFlags        = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.layout             = VK_IMAGE_LAYOUT_GENERAL;
    ci.useMips            = false;
    m_lightCullDebugImage = std::make_shared<Image>(m_swapchainExtent, ci);
}

void Renderer::CreateSampler()
{
    VkSamplerCreateInfo createInfo     = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter               = VK_FILTER_LINEAR;
    createInfo.minFilter               = VK_FILTER_LINEAR;
    createInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.anisotropyEnable        = VK_TRUE;
    createInfo.maxAnisotropy           = 16.f;
    createInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    createInfo.compareEnable           = VK_FALSE;
    createInfo.compareOp               = VK_COMPARE_OP_NEVER;
    createInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.mipLodBias              = 0.0f;
    createInfo.minLod                  = 0.0f;
    createInfo.maxLod                  = VK_LOD_CLAMP_NONE;

    VK_CHECK(vkCreateSampler(m_device, &createInfo, nullptr, &VulkanContext::m_textureSampler), "Failed to create sampler");

    createInfo                         = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter               = VK_FILTER_LINEAR;
    createInfo.minFilter               = VK_FILTER_LINEAR;
    createInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    createInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    createInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    createInfo.anisotropyEnable        = VK_FALSE;
    createInfo.maxAnisotropy           = 1.f;
    createInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    createInfo.compareEnable           = VK_TRUE;
    createInfo.compareOp               = VK_COMPARE_OP_GREATER;
    createInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    createInfo.mipLodBias              = 0.0f;
    createInfo.minLod                  = 0.0f;
    createInfo.maxLod                  = 1.0f;

    VK_CHECK(vkCreateSampler(m_device, &createInfo, nullptr, &VulkanContext::m_shadowSampler), "Failed to create sampler");
}

void Renderer::RefreshDrawCommands()
{
    std::vector<DrawCommand> drawCommands;
    m_renderablesQuery.each(
        [&](const Renderable& renderable)
        {
            DrawCommand dc{};
            dc.indexCount    = renderable.indexCount;
            dc.instanceCount = 1;
            dc.firstIndex    = renderable.indexOffset;
            dc.vertexOffset  = static_cast<int32_t>(renderable.vertexOffset);
            dc.firstInstance = 0;
            // dc.objectID      = renderable->objectID;

            drawCommands.push_back(dc);
        });
    m_drawBuffer.Fill(drawCommands.data(), drawCommands.size() * sizeof(DrawCommand));
    m_numDrawCommands = drawCommands.size();

    m_needDrawBufferReupload = false;
}

void Renderer::RefreshShaderDataOffsets()
{
}

// TODO: make it possible to specifiy sampler parameters per texture (for example we want to use clamp to edge for the brdf texture while using repeat for the material textures)
void Renderer::AddTexture(Image* texture, bool isShadow)
{
    if(m_freeTextureSlots.empty())
    {
        LOG_ERROR("No free texture slots");
        return;
    }
    int32_t slot = m_freeTextureSlots.front();
    m_freeTextureSlots.pop_front();

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = texture->GetLayout();
    imageInfo.imageView   = texture->GetImageView();
    imageInfo.sampler     = isShadow ? VulkanContext::m_shadowSampler : VulkanContext::m_textureSampler;


    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = VulkanContext::m_globalDescSet;
    descriptorWrite.dstArrayElement = slot;
    if(texture->GetUsage() & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        descriptorWrite.dstBinding     = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    else if(texture->GetUsage() & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        descriptorWrite.dstBinding     = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    else
        LOG_ERROR("Texture usage not supported for descriptor");
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    // TODO batch these together (like at the end of the frame or something)
    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);

    texture->SetSlot(slot);
}

void Renderer::RemoveTexture(Image* texture)
{
    // find the sorted position and insert there
    int32_t slot = texture->GetSlot();
    if(slot == -1)
    {
        LOG_ERROR("Texture hasn't been added to the renderer, can't remove it");
        return;
    }
    auto it = std::lower_bound(m_freeTextureSlots.begin(), m_freeTextureSlots.end(), slot);
    m_freeTextureSlots.insert(it, slot);

    Texture& errorTexture = TextureManager::GetTexture("./textures/error.jpg");
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = errorTexture.GetLayout();
    imageInfo.imageView   = errorTexture.GetImageView();
    imageInfo.sampler     = VulkanContext::m_textureSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = VulkanContext::m_globalDescSet;
    descriptorWrite.dstArrayElement = 1;
    if(texture->GetUsage() & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        descriptorWrite.dstBinding     = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    else if(texture->GetUsage() & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        descriptorWrite.dstBinding     = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    else
        LOG_ERROR("Texture usage not supported for descriptor");
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    // TODO batch these together (like at the end of the frame or something)
    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
}


void Renderer::CreateEnvironmentMap()
{
    {
        // CONVERT TO CUBEMAP

        ImageCreateInfo ci{};
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.layout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.isCubeMap   = true;
        ci.debugName   = "Environment Map";

        m_envMap = std::make_unique<Image>(512, 512, ci);

        CommandBuffer cb;
        cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // set up the renderingInfo struct
        VkRenderingInfo rendering          = {};
        rendering.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent.width  = 512;
        rendering.renderArea.extent.height = 512;
        // rendering.layerCount               = 1;
        rendering.viewMask                 = m_equiToCubePipeline->GetViewMask();

        VkRenderingAttachmentInfo attachment = {};
        attachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView                 = m_envMap->CreateImageView(0);
        attachment.clearValue.color          = {0.0f, 0.0f, 0.0f, 0.0f};
        attachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        rendering.pColorAttachments          = &attachment;
        rendering.colorAttachmentCount       = 1;

        TextureManager::LoadTexture("textures/env.hdr");
        Texture& img = TextureManager::GetTexture("textures/env.hdr");
        AddTexture(&img);

        vkCmdBeginRendering(cb.GetCommandBuffer(), &rendering);
        vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_equiToCubePipeline->m_pipeline);
        VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
        vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_equiToCubePipeline->m_layout, 0, 1, &descSet, 0, nullptr);
        m_pushConstants.data[0] = img.GetSlot();
        vkCmdPushConstants(cb.GetCommandBuffer(), m_equiToCubePipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);
        vkCmdDraw(cb.GetCommandBuffer(), 6, 1, 0, 0);
        vkCmdEndRendering(cb.GetCommandBuffer());
        cb.SubmitIdle();

        m_envMap->GenerateMipmaps(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

        AddTexture(m_envMap.get());
    }

    {
        // CONVOLUTION
        ImageCreateInfo ci{};
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.layout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.isCubeMap   = true;
        ci.useMips     = false;
        ci.debugName   = "Convoluted Environment Map";

        m_convEnvMap = std::make_unique<Image>(512, 512, ci);

        CommandBuffer cb;
        cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // set up the renderingInfo struct
        VkRenderingInfo rendering          = {};
        rendering.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent.width  = 512;
        rendering.renderArea.extent.height = 512;
        // rendering.layerCount               = 1;
        rendering.viewMask                 = m_convoltionPipeline->GetViewMask();

        VkRenderingAttachmentInfo attachment = {};
        attachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView                 = m_convEnvMap->GetImageView();
        attachment.clearValue.color          = {0.0f, 0.0f, 0.0f, 0.0f};
        attachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        rendering.pColorAttachments          = &attachment;
        rendering.colorAttachmentCount       = 1;


        vkCmdBeginRendering(cb.GetCommandBuffer(), &rendering);
        vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_convoltionPipeline->m_pipeline);
        VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
        vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_convoltionPipeline->m_layout, 0, 1, &descSet, 0, nullptr);
        m_pushConstants.data[0] = m_envMap->GetSlot();
        vkCmdPushConstants(cb.GetCommandBuffer(), m_convoltionPipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);
        vkCmdDraw(cb.GetCommandBuffer(), 6, 1, 0, 0);
        vkCmdEndRendering(cb.GetCommandBuffer());
        cb.SubmitIdle();

        AddTexture(m_convEnvMap.get());
    }
    {
        // PREFILTER ENVIRONMENT MAP
        ImageCreateInfo ci{};
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.layout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.isCubeMap   = true;
        ci.useMips     = true;
        ci.debugName   = "Prefiltered Environment Map";

        m_prefilteredEnvMap = std::make_unique<Image>(512, 512, ci);

        uint32_t mipLevels = m_prefilteredEnvMap->GetMipLevels();

        CommandBuffer cb;
        cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // for the first mip we just copy the environment map
        // pipeline barrier 2 to transfer layout of first mip level to transfer dst optimal
        /*
        VkImageMemoryBarrier2 barrier           = {};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask                    = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        barrier.srcAccessMask                   = VK_ACCESS_2_NONE;
        barrier.dstStageMask                    = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = m_prefilteredEnvMap->GetImage();
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 6;

        VkDependencyInfo dependency        = {};
        dependency.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers    = &barrier;

        vkCmdPipelineBarrier2(cb.GetCommandBuffer(), &dependency);

        VkImageBlit blit                   = {};
        blit.srcOffsets[0]                 = {0, 0, 0};
        blit.srcOffsets[1]                 = {512, 512, 1};
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = 0;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 6;

        blit.dstOffsets[0]                 = {0, 0, 0};
        blit.dstOffsets[1]                 = {512, 512, 1};
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = 0;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 6;

        vkCmdBlitImage(cb.GetCommandBuffer(),
                       m_envMap->GetImage(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                       m_prefilteredEnvMap->GetImage(), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);
        */


        for(int i = 0; i < mipLevels; ++i)
        {
            uint32_t mipWidth                  = (512 >> i);
            uint32_t mipHeight                 = (512 >> i);
            VkRenderingInfo rendering          = {};
            rendering.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
            rendering.renderArea.extent.width  = mipWidth;
            rendering.renderArea.extent.height = mipHeight;
            // rendering.layerCount               = 1;
            rendering.viewMask                 = m_prefilterPipeline->GetViewMask();

            VkRenderingAttachmentInfo attachment = {};
            attachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            attachment.imageView                 = m_prefilteredEnvMap->CreateImageView(i);
            attachment.clearValue.color          = {0.0f, 0.0f, 0.0f, 0.0f};
            attachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            rendering.pColorAttachments          = &attachment;
            rendering.colorAttachmentCount       = 1;


            vkCmdBeginRendering(cb.GetCommandBuffer(), &rendering);
            vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_prefilterPipeline->m_pipeline);

            VkViewport viewport = VulkanContext::GetViewport(mipWidth, mipHeight);

            vkCmdSetViewport(cb.GetCommandBuffer(), 0, 1, &viewport);
            vkCmdSetScissor(cb.GetCommandBuffer(), 0, 1, &rendering.renderArea);

            VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
            vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_prefilterPipeline->m_layout, 0, 1, &descSet, 0, nullptr);
            m_pushConstants.data[0] = m_envMap->GetSlot();
            m_pushConstants.data[1] = (float)i / (float)(mipLevels - 1);
            vkCmdPushConstants(cb.GetCommandBuffer(), m_prefilterPipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);
            vkCmdDraw(cb.GetCommandBuffer(), 6, 1, 0, 0);
            vkCmdEndRendering(cb.GetCommandBuffer());
        }

        cb.SubmitIdle();

        AddTexture(m_prefilteredEnvMap.get());
    }

    {
        // GENERATE BRDF LUT
        ImageCreateInfo ci{};
        ci.format      = VK_FORMAT_R16G16_SFLOAT;
        ci.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.layout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        ci.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.useMips     = false;
        ci.debugName   = "BRDF LUT";

        m_BRDFLUT = std::make_unique<Image>(512, 512, ci);

        CommandBuffer cb;
        cb.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // set up the renderingInfo struct
        VkRenderingInfo rendering          = {};
        rendering.sType                    = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent.width  = 512;
        rendering.renderArea.extent.height = 512;
        rendering.layerCount               = 1;

        VkRenderingAttachmentInfo attachment = {};
        attachment.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView                 = m_BRDFLUT->GetImageView();
        attachment.clearValue.color          = {0.0f, 0.0f, 0.0f, 0.0f};
        attachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        rendering.pColorAttachments          = &attachment;
        rendering.colorAttachmentCount       = 1;


        vkCmdBeginRendering(cb.GetCommandBuffer(), &rendering);
        vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_computeBRDFPipeline->m_pipeline);
        VkDescriptorSet descSet = VulkanContext::GetGlobalDescSet();
        vkCmdBindDescriptorSets(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_computeBRDFPipeline->m_layout, 0, 1, &descSet, 0, nullptr);
        m_pushConstants.data[0] = m_envMap->GetSlot();
        vkCmdPushConstants(cb.GetCommandBuffer(), m_computeBRDFPipeline->m_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &m_pushConstants);
        vkCmdDraw(cb.GetCommandBuffer(), 3, 1, 0, 0);
        vkCmdEndRendering(cb.GetCommandBuffer());
        cb.SubmitIdle();

        AddTexture(m_BRDFLUT.get());
    }
}

std::tuple<std::array<glm::mat4, NUM_CASCADES>, std::array<glm::mat4, NUM_CASCADES>, std::array<glm::vec2, NUM_CASCADES>> GetCascadeMatricesOrtho(const glm::mat4& invCamera, const glm::vec3& lightDir, float zNear, float maxDepth)
{
    std::array<glm::mat4, NUM_CASCADES> resVP;
    std::array<glm::mat4, NUM_CASCADES> resV;
    std::array<glm::vec2, NUM_CASCADES> zPlanes;

    float maxDepthNDC  = zNear / maxDepth;
    float depthNDCStep = (1 - maxDepthNDC) / NUM_CASCADES;
    float step         = maxDepth / NUM_CASCADES;

    const float lambda = 0.3f;

    // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
    std::vector<float> cascadeSplits;
    for(int i = 0; i < NUM_CASCADES; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(NUM_CASCADES);

        // Logarithmic split for near cascades, linear split for far cascades
        float logSplit    = zNear * std::pow(maxDepth / zNear, t);
        float linearSplit = zNear + t * (maxDepth - zNear);

        // Blend between logarithmic and linear splits
        float splitDistance = logSplit + lambda * (linearSplit - logSplit);
        cascadeSplits.push_back(splitDistance);
    }
    cascadeSplits.push_back(maxDepth);

    for(int i = 0; i < NUM_CASCADES; ++i)
    {
        float cascadeDepthStart = zNear / cascadeSplits[i];
        float cascadeDepthEnd   = zNear / cascadeSplits[i + 1];

        glm::vec3 min(std::numeric_limits<float>::infinity());
        glm::vec3 max(-std::numeric_limits<float>::infinity());
        // cube between -1,-1,0 and 1,1,1
        std::vector<glm::vec4> boundingVertices = {
            {-1.0f, -1.0f, cascadeDepthStart, 1.0f}, // near bottom left
            {-1.0f,  1.0f, cascadeDepthStart, 1.0f}, // near top left
            { 1.0f, -1.0f, cascadeDepthStart, 1.0f}, // near bottom right
            { 1.0f,  1.0f, cascadeDepthStart, 1.0f}, // near top right
            {-1.0f, -1.0f,   cascadeDepthEnd, 1.0f}, // far bottom left
            {-1.0f,  1.0f,   cascadeDepthEnd, 1.0f}, // far top left
            { 1.0f, -1.0f,   cascadeDepthEnd, 1.0f}, // far bottom right
            { 1.0f,  1.0f,   cascadeDepthEnd, 1.0f}  // far top right
        };
        // clip space to world space
        for(glm::vec4& vert : boundingVertices)
        {
            vert  = invCamera * vert;
            vert /= vert.w;
        }

        glm::vec3 center(0.0f);
        for(const glm::vec4& vert : boundingVertices)
        {
            center += glm::vec3(vert);
        }
        center /= boundingVertices.size();

        glm::vec3 up(lightDir.y, -lightDir.x, 0.0f);  // TODO
        glm::mat4 lightView = glm::lookAt(center - lightDir, center, glm::normalize(up));

        // world space to light space
        for(glm::vec4& vert : boundingVertices)
        {
            vert  = lightView * vert;
            min.x = glm::min(min.x, vert.x);
            min.y = glm::min(min.y, vert.y);
            min.z = glm::min(min.z, vert.z);
            max.x = glm::max(max.x, vert.x);
            max.y = glm::max(max.y, vert.y);
            max.z = glm::max(max.z, vert.z);
        }

        // make the cascade constant size in world space
        float radius = glm::distance(boundingVertices[0], boundingVertices[7]) / 2.0f;

        min.x = center.x - radius;
        min.y = center.y - radius;
        max.x = center.x + radius;
        max.y = center.y + radius;

        /*
        // round the boundaries to pixel size
        float pixelSize = radius * 2.0f / SHADOWMAP_SIZE;
        min.x = std::round(min.y / pixelSize) * pixelSize;
        max.x = std::round(max.y / pixelSize) * pixelSize;
        min.y = std::round(min.y / pixelSize) * pixelSize;
        max.y = std::round(max.y / pixelSize) * pixelSize;
        */
        // glm::mat4 proj = glm::ortho(-500.f, 500.f, -500.f, 500.f, 500.f, -500.f);
        resV[i]    = lightView;
        resVP[i]   = glm::ortho(min.x, max.x, min.y, max.y, max.z, min.z) * lightView;  // for z we do max for near and min for far because of reverse z
        zPlanes[i] = glm::vec2(cascadeDepthStart, cascadeDepthEnd);
    }
    return {resVP, resV, zPlanes};
}

void Renderer::UpdateLights(uint32_t index)
{
    PROFILE_FUNCTION();
    {
        PROFILE_SCOPE("Directional Lights update");

        m_directionalLightsQuery.each(
            [&](const DirectionalLight& lightComp, const InternalTransform& transform)
            {
                auto newDir        = glm::normalize(-glm::vec3(transform.worldTransform[2]));
                float newIntensity = lightComp.intensity;
                auto newColor      = lightComp.color.ToVec3();


                Light& light = m_lightMap[lightComp._slot];
                bool changed = newDir != light.direction || newIntensity != light.intensity || newColor != light.color;
                if(changed)
                {
                    light.direction = newDir;
                    light.intensity = newIntensity;
                    light.color     = newColor;

                    for(auto& dict : m_changedLights)
                        dict[lightComp._slot] = &light;  // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
                }
                // light.lightSpaceMatrices = GetCascadeMatricesOrtho(glm::mat4(1.0), newDir,
            });
    }
    {
        PROFILE_SCOPE("Point Lights update");

        m_pointLightsQuery.each(
            [&](const PointLight& lightComp, const InternalTransform& transform)
            {
                auto newPos = glm::vec3(transform.worldTransform[3]);

                float newRange     = lightComp.range;
                float newIntensity = lightComp.intensity;
                glm::vec3 newColor = lightComp.color.ToVec3();
                Attenuation newAtt = lightComp.attenuation;

                Light& light = m_lightMap[lightComp._slot];
                bool changed = newPos != light.position || newIntensity != light.intensity
                            || newColor != light.color || newRange != light.range
                            || newAtt.quadratic != light.attenuation[0] || newAtt.linear != light.attenuation[1] || newAtt.constant != light.attenuation[2];
                if(changed)
                {
                    light.position  = newPos;
                    light.range     = newRange;
                    light.intensity = newIntensity;
                    light.color     = newColor;

                    light.attenuation[0] = newAtt.quadratic;
                    light.attenuation[1] = newAtt.linear;
                    light.attenuation[2] = newAtt.constant;

                    for(auto& dict : m_changedLights)
                        dict[lightComp._slot] = &light;  // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
                }
            });
    }
    {
        PROFILE_SCOPE("Spot Lights update");

        m_spotLightsQuery.each(
            [&](const SpotLight& lightComp, const InternalTransform& transform)
            {
                auto newPos        = glm::vec3(transform.worldTransform[3]);
                auto newDir        = glm::normalize(glm::vec3(transform.worldTransform[2]));
                float newRange     = lightComp.range;
                float newIntensity = lightComp.intensity;
                float newCutoff    = lightComp.cutoff;
                glm::vec3 newColor = lightComp.color.ToVec3();

                Attenuation newAtt = lightComp.attenuation;


                Light& light = m_lightMap[lightComp._slot];
                bool changed = newPos != light.position || newDir != light.direction || newIntensity != light.intensity
                            || newColor != light.color || newRange != light.range || newCutoff != light.cutoff
                            || newAtt.quadratic != light.attenuation[0] || newAtt.linear != light.attenuation[1] || newAtt.constant != light.attenuation[2];

                if(changed)
                {
                    light.position  = newPos;
                    light.direction = newDir;
                    light.range     = newRange;
                    light.intensity = newIntensity;
                    light.cutoff    = newCutoff;
                    light.color     = newColor;

                    light.attenuation[0] = newAtt.quadratic;
                    light.attenuation[1] = newAtt.linear;
                    light.attenuation[2] = newAtt.constant;

                    for(auto& dict : m_changedLights)
                        dict[lightComp._slot] = &light;  // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
                }
            });
    }

    std::vector<std::pair<uint32_t, void*>> slotsAndDatas;
    for(auto& [slot, newLight] : m_changedLights[index])
    {
        slotsAndDatas.emplace_back(slot, newLight);
        m_lightsBuffers[index]->UploadData(slot, newLight);  // TODO batch the upload together
    }
    m_changedLights[index].clear();

    // m_lightsBuffers[index]->UpdateBuffer(slotsAndDatas);
}

void Renderer::UpdateLightMatrices(uint32_t index)
{
    glm::mat4 inverseVP;
    float zNear = NAN;
    // TODO implement a function for inverse projection matrix instead of using generic inverse
    m_mainCameraQuery.each(
        [&](const Camera& camera, const InternalTransform& transform)
        {
            // TODO implement a function for inverse projection matrix instead of using generic inverse
            inverseVP = transform.worldTransform * glm::inverse(GetProjection(camera));
            zNear     = camera.zNear;
        });
    for(const auto& [_, internalLight] : m_lightMap)
    {
        if(internalLight.type != LightType::Directional)
            continue;

        auto [cascadesVP, cascadesV, zPlanes] = GetCascadeMatricesOrtho(inverseVP, internalLight.direction, zNear, MAX_SHADOW_DEPTH);

        ShadowMatrices data{};
        data.lightSpaceMatrices = cascadesVP;
        data.lightViewMatrices  = cascadesV;
        data.zPlanes            = zPlanes;

        m_shadowMatricesBuffers[index].UploadData(internalLight.matricesSlot, &data);
    }
}

void Renderer::Render(double dt)
{
    // PROFILE_FUNCTION();
    uint32_t imageIndex;
    VkResult result;
    {
        PROFILE_SCOPE("Pre frame stuff");
        {
            PROFILE_SCOPE("Wait for fences");
            vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
        }

        result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);


        if(result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain();
            return;
        }
        else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("Failed to acquire swap chain image");

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if(m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
            vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        // Mark the image as now being in use by this frame
        m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];


        VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]), "Failed to reset in flight fences");


        // m_debugUI->SetupFrame(imageIndex, 0, &m_renderPass);	//subpass is 0 because we only have one subpass for now

        glm::mat4 vp;
        glm::vec3 cameraPos;

        m_mainCameraQuery.each(
            [&](const Camera& camera, const InternalTransform& transform)
            {
                vp        = GetProjection(camera) * glm::inverse(transform.worldTransform);
                cameraPos = transform.worldTransform[3];
            });

        //
        UpdateLights(imageIndex);
        UpdateLightMatrices(imageIndex);
        m_pushConstants.cameraPos = cameraPos;
        m_pushConstants.viewProj  = vp;
        // m_ubAllocators["camera" + std::to_string(imageIndex)]->UpdateBuffer(0, &cs);

        if(m_needDrawBufferReupload)
        {
            RefreshDrawCommands();
        }
        m_transformsQuery.each(
            [&](const InternalTransform& transform, const Renderable& renderable)
            {
                glm::mat4 model = transform.worldTransform;
                for(auto& buffer : m_transformBuffers)
                    buffer.UploadData(renderable.objectID, &model);
            });
    }

    m_mainCommandBuffers[imageIndex].Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    m_renderGraph.Execute(m_mainCommandBuffers[imageIndex], imageIndex);

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    m_mainCommandBuffers[imageIndex].Submit(m_graphicsQueue, m_imageAvailable[m_currentFrame], wait, m_renderFinished[m_currentFrame], m_inFlightFences[m_currentFrame]);

    {
        PROFILE_SCOPE("Present");
        VkPresentInfoKHR presentInfo   = {};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &m_renderFinished[m_currentFrame];


        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains    = &m_swapchain;
        presentInfo.pImageIndices  = &imageIndex;
        result                     = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    }
    std::array<uint64_t, 4> queryResults;
    {
        PROFILE_SCOPE("Query results");
        // VK_CHECK(vkGetQueryPoolResults(m_device, m_queryPools[m_currentFrame], 0, 4, sizeof(uint64_t) * 4, queryResults.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "Query isn't ready");
    }
    if(result == VK_SUBOPTIMAL_KHR || m_window->IsResized())
    {
        m_window->SetResized(false);
        RecreateSwapchain();
    }
    else
        VK_CHECK(result, "Failed to present the swapchain image");

    m_queryResults[m_currentFrame] = (queryResults[3] - queryResults[0]) * m_timestampPeriod;
    // LOG_INFO("GPU took {0} ms", m_queryResults[m_currentFrame] * 1e-6);

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::OnSceneSwitched(SceneSwitchedEvent e)
{
    m_ecs = e.newScene->GetECS();
}

void Renderer::OnMeshComponentAdded(ComponentAdded<Mesh> e)
{
    // TODO don't keep the vertex and index vectors after sending them to gpu
    const Mesh* mesh         = e.entity.GetComponent<Mesh>();
    VkDeviceSize vBufferSize = sizeof(mesh->vertices[0]) * mesh->vertices.size();
    Renderable comp{};


    // create the vertex and index buffers on the gpu
    bool didVBResize    = false;
    uint64_t vertexSlot = m_vertexBuffer.Allocate(mesh->vertices.size(), didVBResize, (void*)&comp);
    m_vertexBuffer.UploadData(vertexSlot, mesh->vertices.data());

    comp.vertexOffset = vertexSlot;
    comp.vertexCount  = mesh->vertices.size();

    bool didIBResize   = false;
    uint64_t indexSlot = m_indexBuffer.Allocate(mesh->indices.size(), didIBResize, &comp);
    m_indexBuffer.UploadData(indexSlot, mesh->indices.data());

    comp.indexOffset = indexSlot;
    comp.indexCount  = mesh->indices.size();

    if(didVBResize)
    {
        for(const auto& [slot, info] : m_vertexBuffer.GetAllocationInfos())
        {
            auto* renderable         = (Renderable*)info.pUserData;  // TODO
            renderable->vertexOffset = slot;
        }
    }

    if(didIBResize)
    {
        for(const auto& [slot, info] : m_indexBuffer.GetAllocationInfos())
        {
            auto* renderable        = (Renderable*)info.pUserData;
            renderable->indexOffset = slot;
        }
    }

    if(didVBResize || didIBResize)
    {
        // RecreateDrawBuffers();
    }

    uint32_t slot = 0;
    for(auto& buffer : m_transformBuffers)
    {
        slot = buffer.Allocate(1);
    }
    comp.objectID = slot;

    comp.objectID = slot;

    m_needDrawBufferReupload = true;

    e.entity.SetComponent<Renderable>(comp);
}

void Renderer::OnMeshComponentRemoved(ComponentRemoved<Mesh> e)
{
    // TODO remove the vertex and index buffers from the gpu
    e.entity.RemoveComponent<Renderable>();
}

// TODO what is this
void Renderer::OnMaterialComponentAdded(ComponentAdded<Material> e)
{
    /*auto it = m_pipelinesRegistry.find(e->component->shaderName);
    if(it == m_pipelinesRegistry.end())
    {
        LOG_ERROR("Pipeline for {0} material doesn't exist", e->component->shaderName);
        assert(false);
    }

    m_ecs->componentManager->Sort<Material>([&](Material* lhs, Material* rhs)
            {
                return *m_pipelinesRegistry[lhs->shaderName] < *m_pipelinesRegistry[rhs->shaderName];
            });
    */
}

void Renderer::OnDirectionalLightAdded(ComponentAdded<DirectionalLight> e)
{
    uint32_t slot = 0;
    for(auto& buffer : m_lightsBuffers)
        slot = buffer->Allocate(1);  // slot should be the same for all of these, since we allocate to every buffer every time

    auto* comp  = e.entity.GetComponentMut<DirectionalLight>();
    comp->_slot = slot;

    m_lightMap[slot] = {LightType::Directional};  // 0 = DirectionalLight
    Light* light     = &m_lightMap[slot];

    uint32_t matricesSlot = 0;
    for(auto& buffer : m_shadowMatricesBuffers)
        matricesSlot = buffer.Allocate(1);  // slot should be the same for all of these, since we allocate to every buffer every time

    light->matricesSlot = matricesSlot;

    for(auto& dict : m_changedLights)
    {
        dict[slot] = light;
    }


    ImageCreateInfo ci;
    ci.format      = VK_FORMAT_D32_SFLOAT;
    ci.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    ci.layout      = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;  // the render pass will transfer to good layout
    ci.useMips     = false;
    ci.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.layerCount  = NUM_CASCADES;

    slot              = m_shadowmaps.size();
    comp->_shadowSlot = slot;


    std::vector<std::unique_ptr<Image>> vec(NUM_CASCADES);
    for(int i = 0; i < NUM_CASCADES; ++i)
    {
        vec[i] = std::make_unique<Image>(SHADOWMAP_SIZE, SHADOWMAP_SIZE, ci);
    }

    // TODO very temp code
    m_shadowmaps.push_back(std::move(vec));
    Image* img = m_shadowmaps[0].back().get();
    AddTexture(img, true);
    m_shadowMaps->AddImagePointer(img);
    uint32_t shadowSlot = m_shadowIndices.Allocate(1);
    uint32_t imgSlot    = img->GetSlot();
    m_shadowIndices.UploadData(shadowSlot, &imgSlot);
    m_numShadowIndices++;

    // TODO

    light->shadowSlot = slot;
    std::array<VkDescriptorImageInfo, NUM_CASCADES> imageInfos;
    std::array<VkDescriptorImageInfo, NUM_CASCADES> imageInfosPCF;

    // debugimages
    /*
    int i = 0;
    for(auto* img : m_shadowMaps->GetImagePointers())
    {
        auto debugImage = std::make_shared<UIImage>(img);
        debugImage->SetName(std::string("Cascade #") + std::to_string(i));
        m_rendererDebugWindow->AddElement(debugImage);
        i++;
    }
    */
}

void Renderer::OnPointLightAdded(ComponentAdded<PointLight> e)
{
    uint32_t slot = 0;
    for(auto& buffer : m_lightsBuffers)
        slot = buffer->Allocate(1);  // slot should be the same for all of these, since we allocate to every buffer every time

    auto* comp  = e.entity.GetComponentMut<PointLight>();
    comp->_slot = slot;

    m_lightMap[slot] = {1};  // 1 = PointLight
    Light* light     = &m_lightMap[slot];

    for(auto& dict : m_changedLights)
    {
        dict[slot] = light;
    }
}

void Renderer::OnSpotLightAdded(ComponentAdded<SpotLight> e)
{
    uint32_t slot = 0;
    for(auto& buffer : m_lightsBuffers)
        slot = buffer->Allocate(1);  // slot should be the same for all of these, since we allocate to every buffer every time

    auto* comp  = e.entity.GetComponentMut<SpotLight>();
    comp->_slot = slot;

    m_lightMap[slot] = {2};  // 2 = SpotLight
    Light* light     = &m_lightMap[slot];

    for(auto& dict : m_changedLights)
    {
        dict[slot] = light;
    }
}


// #################################################################################

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}


std::vector<const char*> GetExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#ifdef VDEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    return extensions;
}

VkPhysicalDevice PickDevice(const std::vector<VkPhysicalDevice>& devices, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions)
{
    std::multimap<int, VkPhysicalDevice> ratedDevices;
    for(auto& device : devices)
    {
        int score = RateDevice(device, surface, deviceExtensions);
        ratedDevices.insert(std::make_pair(score, device));
    }

    if(ratedDevices.rbegin()->first > 0)
        return ratedDevices.rbegin()->second;
    else
        throw std::runtime_error("Didn't find suitable GPU");
}

int RateDevice(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions)
{
    uint32_t extensionPropCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionPropCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for(const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    bool swapChainGood = false;
    if(requiredExtensions.empty())
    {
        SwapchainSupportDetails details = QuerySwapChainSupport(device, surface);
        swapChainGood                   = !details.formats.empty() && !details.presentModes.empty();
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);


    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    int score = 0;
    if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        score += 1000;

    score     += deviceProperties.limits.maxImageDimension2D;
    bool temp  = FindQueueFamilies(device, surface).IsComplete();
    // example: if the application cannot function without a geometry shader
    if(!deviceFeatures.geometryShader || !temp || !requiredExtensions.empty() || !swapChainGood || !deviceFeatures.samplerAnisotropy)
    {
        return 0;
    }
    LOG_TRACE("Device: {0}    Score: {1}", deviceProperties.deviceName, score);
    return score;
}

VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if(counts & VK_SAMPLE_COUNT_64_BIT)
    {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if(counts & VK_SAMPLE_COUNT_32_BIT)
    {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if(counts & VK_SAMPLE_COUNT_16_BIT)
    {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if(counts & VK_SAMPLE_COUNT_8_BIT)
    {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if(counts & VK_SAMPLE_COUNT_4_BIT)
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if(counts & VK_SAMPLE_COUNT_2_BIT)
    {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilies.data());
    int i = 0;
    for(const auto& queueFamily : queueFamilies)
    {
        if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }
        VkBool32 presentationSupport;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
        if(queueFamily.queueCount > 0 && presentationSupport && !indices.presentationFamily.has_value())
        {
            indices.presentationFamily = i;
        }

        if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.computeFamily = i;

        if(indices.IsComplete())
        {
            break;
        }

        i++;
    }
    return indices;
}

SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    details.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.formats.data());

    count = 0;  // this is probably not needed but i will put it in just in case for now
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    details.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, details.presentModes.data());

    return details;
}

VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    // if the surface has no preferred format vulkan returns one entity of Vk_FORMAT_UNDEFINED
    // we can then chose whatever we want
    if(availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        VkSurfaceFormatKHR format;
        format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        format.format     = VK_FORMAT_B8G8R8A8_UNORM;
        return format;
    }
    for(const auto& format : availableFormats)
    {
        if(format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    // if we can't find one that we like we could rank them based on how good they are
    // but we will just settle for the first one(apparently in most cases it's okay)
    return availableFormats[0];
}

VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for(const auto& presentMode : availablePresentModes)
    {
        if(presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
        if(presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return presentMode;
    }

    // FIFO is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
    // swap extent is the resolution of the swapchain images

    // if we can set an extent manually the width and height values will be uint32t max
    // else we can't set it so just return it
    if(capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())  // () around max to prevent macro expansion by windows.h max macro
        return capabilities.currentExtent;
    else
    {
        // choose an extent within the minImageExtent and maxImageExtent bounds
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        actualExtent.width      = glm::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height     = glm::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                     VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData, void* /*pUserData*/)
{
    std::ostringstream message;
    message << "\n";

    std::string messageidName = "";
    if(pCallbackData->pMessageIdName)
        messageidName = pCallbackData->pMessageIdName;

    message << "\t"
            << "messageIDName   = <" << messageidName << ">\n";


    message << "\t"
            << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";

    message << "\t"
            << "messageType     = <" << string_VkDebugUtilsMessageTypeFlagsEXT(messageTypes) << ">\n";

    if(pCallbackData->pMessage)
        message << "\t"
                << "message         = <" << pCallbackData->pMessage << ">\n";


    if(0 < pCallbackData->queueLabelCount)
    {
        message << "\t"
                << "Queue Labels:\n";
        for(uint8_t i = 0; i < pCallbackData->queueLabelCount; i++)
        {
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if(0 < pCallbackData->cmdBufLabelCount)
    {
        message << "\t"
                << "CommandBuffer Labels:\n";
        for(uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
        {
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if(0 < pCallbackData->objectCount)
    {
        message << "\t"
                << "Objects:\n";
        for(uint8_t i = 0; i < pCallbackData->objectCount; i++)
        {
            message << "\t\t"
                    << "Object " << i << "\n";
            message << "\t\t\t"
                    << "objectType   = " << string_VkObjectType(pCallbackData->pObjects[i].objectType) << "\n";
            message << "\t\t\t"
                    << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
            if(pCallbackData->pObjects[i].pObjectName)
            {
                message << "\t\t\t"
                        << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
        }
    }

    if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        LOG_TRACE(message.str());
    }
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        LOG_INFO(message.str());
    }
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LOG_WARN(message.str());
    }
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LOG_ERROR(message.str());
    }


    return VK_TRUE;
}
