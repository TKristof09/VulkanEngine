#include "Rendering/Renderer.hpp"
#include "Rendering/Image.hpp"
#include "Rendering/VulkanContext.hpp"
#include "glm/ext/matrix_transform.hpp"
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
#include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "TextureManager.hpp"
#include "ECS/ComponentManager.hpp"
#include "ECS/CoreComponents/Camera.hpp"
#include "ECS/CoreComponents/Lights.hpp"
#include "ECS/CoreComponents/Transform.hpp"
#include "ECS/CoreComponents/Renderable.hpp"
#include "ECS/CoreComponents/Material.hpp"

#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/RenderGraph/RenderPass.hpp"


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


Renderer::Renderer(std::shared_ptr<Window> window, Scene* scene)
    : m_ecs(scene->ecs),
      m_window(window),
      m_instance(VulkanContext::m_instance),
      m_gpu(VulkanContext::m_gpu),
      m_device(VulkanContext::m_device),
      m_graphicsQueue(VulkanContext::m_graphicsQueue),
      m_commandPool(VulkanContext::m_commandPool),
      m_renderGraph(RenderGraph(this)),
      m_vertexBuffer(5e6, sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 5e5),
      m_indexBuffer(5e6, sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 5e6),

      m_shaderDataBuffer(1e5, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 1e3, false),  // objectSize = 1 byte because each shader data can be diff size so we "store them as bytes"

      m_pushConstants({}),
      m_freeTextureSlots(NUM_TEXTURE_DESCRIPTORS),
      m_shadowIndices(100, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 100, false)
{
    scene->eventHandler->Subscribe(this, &Renderer::OnMeshComponentAdded);
    scene->eventHandler->Subscribe(this, &Renderer::OnMeshComponentRemoved);
    scene->eventHandler->Subscribe(this, &Renderer::OnMaterialComponentAdded);
    scene->eventHandler->Subscribe(this, &Renderer::OnDirectionalLightAdded);
    scene->eventHandler->Subscribe(this, &Renderer::OnPointLightAdded);
    scene->eventHandler->Subscribe(this, &Renderer::OnSpotLightAdded);

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


    m_rendererDebugWindow = std::make_unique<DebugUIWindow>("Renderer");
    AddDebugUIWindow(m_rendererDebugWindow.get());


    m_drawBuffer.Allocate(1000 * sizeof(DrawCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, true);  // TODO change to non mappable and use staging buffer
    m_frustrumEntity                                = m_ecs->entityManager->CreateEntity();
    m_frustrumEntity->GetComponent<NameTag>()->name = "Frustum object";
    // auto mesh = m_frustrumEntity->AddComponent<Mesh>();
    {
        auto button = std::make_shared<Button>("Show frustrum");
        button->RegisterCallback(
            [=](Button* b)
            {
                if(m_frustrumEntity->GetComponent<Mesh>())
                {
                    m_frustrumEntity->RemoveComponent<Mesh>();
                }
                if(!m_frustrumEntity->GetComponent<Material>())
                {
                    auto mat        = m_frustrumEntity->AddComponent<Material>();
                    mat->shaderName = "forwardplus";
                }
                Camera camera              = *(*(m_ecs->componentManager->begin<Camera>()));
                Transform* cameraTransform = m_ecs->componentManager->GetComponent<Transform>(camera.GetOwner());

                glm::mat4 inverseVP = cameraTransform->GetTransform() * glm::inverse(camera.GetProjection());  // TODO implement a function for inverse projection matrix instead of using generic inverse

                float zNear                             = 0.1f;
                float cascadeDepthStart                 = 1;
                float cascadeDepthEnd                   = zNear / (500.0f);
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
                auto mesh = m_frustrumEntity->AddComponent<Mesh>();
                std::vector<Vertex> vertices;
                for(glm::vec4& vert : boundingVertices)
                {
                    vert  = inverseVP * vert;
                    vert /= vert.w;
                    vertices.push_back({vert, {}, {}});
                }
                mesh->vertices = vertices;
                mesh->indices  = {
                    0, 2, 1, 2, 3, 1,  // near
                    4, 6, 5, 6, 7, 5,  // far
                    4, 0, 5, 0, 1, 5,  // left
                    1, 3, 5, 7, 5, 3,  // top
                    3, 2, 6, 6, 7, 3,  // right
                    0, 2, 6, 0, 6, 4   // bottom
                };
            });
        m_rendererDebugWindow->AddElement(button);
    }
    {
        auto button = std::make_shared<Button>("Hide frustrum");
        button->RegisterCallback([=](Button* b)
                                 { m_frustrumEntity->RemoveComponent<Mesh>(); });
        m_rendererDebugWindow->AddElement(button);
    }


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

                ComponentManager* cm       = m_ecs->componentManager;
                Camera camera              = *(*(cm->begin<Camera>()));
                Transform* cameraTransform = cm->GetComponent<Transform>(camera.GetOwner());
                glm::mat4 proj             = camera.GetProjection();
                // glm::mat4 trf = glm::translate(-cameraTransform->wPosition);
                // glm::mat4 rot = glm::toMat4(glm::conjugate(cameraTransform->wRotation));
                // glm::mat4 vp = proj * rot * trf;
                glm::mat4 vp               = proj * glm::inverse(cameraTransform->GetTransform());  // TODO inversing the transform like this isnt too fast, consider only allowing camera on root level entity so we can just -pos and -rot
                uint32_t vpOffset          = 0;

                vkCmdBindPipeline(cb.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_pipeline);
                PROFILE_SCOPE("Prepass draw call loop");
                m_pushConstants.viewProj           = vp;
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
                ShaderData data          = {};
                data.viewportSize        = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums            = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                data.visibleLightsBuffer = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                // TODO temp
                data.shadowMapIds        = m_shadowIndices.GetDeviceAddress(0);
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
                    m_shaderDataOffsets[i]["forwardplus"] = offset;
                }
            });
        lightingPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                // TODO temporary
                ShaderData data          = {};
                data.viewportSize        = glm::ivec2(VulkanContext::GetSwapchainExtent().width, VulkanContext::GetSwapchainExtent().height);
                data.tileNums            = glm::ivec2(ceil(data.viewportSize.x / 16.0f), ceil(data.viewportSize.y / 16.0f));
                data.visibleLightsBuffer = visibleLightsBuffer.GetBufferPointer()->GetDeviceAddress();
                // TODO temp
                data.shadowMapIds        = m_shadowIndices.GetDeviceAddress(0);
                for(int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
                {
                    data.lightBuffer          = m_lightsBuffers[i]->GetDeviceAddress(0);
                    data.shadowMapCount       = m_numShadowIndices;
                    data.shadowMatricesBuffer = m_shadowMatricesBuffers[i].GetDeviceAddress(0);
                    m_shaderDataBuffer.UploadData(m_shaderDataOffsets[i]["forwardplus"], &data);
                }

                vkCmdBeginRendering(cb.GetCommandBuffer(), lightingPass.GetRenderingInfo());

                ComponentManager* cm  = m_ecs->componentManager;
                auto materialIterator = cm->begin<Material>();
                auto end              = cm->end<Material>();
                uint32_t offset       = 0;

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
                ComponentManager* cm = m_ecs->componentManager;
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
                    bool tmp         = false;
                    uint64_t offset  = m_shaderDataBuffer.Allocate(sizeof(ShaderData), tmp);
                    m_shaderDataBuffer.UploadData(offset, &data);
                    // store the offset somewhere
                    m_shaderDataOffsets[i]["lightCull"] = offset;
                }
            });
        lightCullPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t imageIndex)
            {
                /*
                // TODO this barrier should be done automatically by the rendergraph, also probably wrong src masks
                VkBufferMemoryBarrier2 barrierBefore = {};
                barrierBefore.sType                  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                barrierBefore.srcStageMask           = VK_PIPELINE_STAGE_2_HOST_BIT;
                barrierBefore.srcAccessMask          = VK_ACCESS_2_HOST_WRITE_BIT;
                barrierBefore.dstStageMask           = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                barrierBefore.dstAccessMask          = VK_ACCESS_2_SHADER_READ_BIT;
                barrierBefore.buffer                 = m_lightsBuffers[imageIndex]->GetBuffer(0);
                barrierBefore.size                   = m_lightsBuffers[imageIndex]->GetSize();

                VkDependencyInfo dependencyInfo         = {};
                dependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependencyInfo.bufferMemoryBarrierCount = 1;
                dependencyInfo.pBufferMemoryBarriers    = &barrierBefore;

                vkCmdPipelineBarrier2(cb.GetCommandBuffer(), &dependencyInfo);
                */
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
        auto& uiPass = m_renderGraph.AddRenderPass("uiPass", QueueTypeFlagBits::Graphics);
        uiPass.AddColorOutput(SWAPCHAIN_RESOURCE_NAME, {}, "colorImage");
        uiPass.AddTextureArrayInput("shadowMaps", VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        uiPass.SetExecutionCallback(
            [&](CommandBuffer& cb, uint32_t frameIndex)
            {
                vkCmdBeginRendering(cb.GetCommandBuffer(), uiPass.GetRenderingInfo());
                m_debugUI->Draw(&cb);
                vkCmdEndRendering(cb.GetCommandBuffer());
            });
    }
    m_renderGraph.Build();
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

    vkDestroySampler(m_device, m_computeSampler, nullptr);
    vkDestroySampler(m_device, m_shadowSampler, nullptr);
    vkDestroySampler(m_device, m_shadowSamplerPCF, nullptr);


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
    // Compute
    PipelineCreateInfo compute = {};
    compute.type               = PipelineType::COMPUTE;
    m_compute                  = std::make_unique<Pipeline>("lightCulling", compute);

    {
        VkSamplerCreateInfo ci     = {};
        ci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter               = VK_FILTER_LINEAR;
        ci.minFilter               = VK_FILTER_LINEAR;
        ci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.anisotropyEnable        = VK_TRUE;
        ci.maxAnisotropy           = 1.0f;
        ci.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        ci.unnormalizedCoordinates = VK_FALSE;  // this should always be false because UV coords are in [0,1) not in [0, width),etc...
        ci.compareEnable           = VK_FALSE;  // this is used for percentage closer filtering for shadow maps
        ci.compareOp               = VK_COMPARE_OP_ALWAYS;
        ci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        ci.mipLodBias              = 0.0f;
        ci.minLod                  = 0.0f;
        ci.maxLod                  = 1.0f;

        VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_computeSampler), "Failed to create depth texture sampler");
    }

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
    shadowPipeline.viewMask         = 0b1111;  // NUM_CASCADES of 1s TODO make it not hardcoded

    shadowPipeline.isGlobal = true;

    m_shadowPipeline = std::make_unique<Pipeline>("shadow", shadowPipeline);  // depth shader is fine for the shadow maps too since we only need the depth info from it

    {
        VkSamplerCreateInfo ci     = {};
        ci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter               = VK_FILTER_NEAREST;
        ci.minFilter               = VK_FILTER_NEAREST;
        ci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.anisotropyEnable        = VK_FALSE;
        ci.maxAnisotropy           = 1.0f;
        ci.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        ci.unnormalizedCoordinates = VK_FALSE;  // this should always be false because UV coords are in [0,1) not in [0, width),etc...
        ci.compareEnable           = VK_FALSE;  // this is used for percentage closer filtering for shadow maps
        ci.compareOp               = VK_COMPARE_OP_NEVER;
        ci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        ci.mipLodBias              = 0.0f;
        ci.minLod                  = 0.0f;
        ci.maxLod                  = 1.0f;

        VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_shadowSampler), "Failed to create shadow texture sampler");
    }
    {
        VkSamplerCreateInfo ci     = {};
        ci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter               = VK_FILTER_LINEAR;
        ci.minFilter               = VK_FILTER_LINEAR;
        ci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.anisotropyEnable        = VK_FALSE;
        ci.maxAnisotropy           = 1.0f;
        ci.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        ci.unnormalizedCoordinates = VK_FALSE;  // this should always be false because UV coords are in [0,1) not in [0, width),etc...
        ci.compareEnable           = VK_TRUE;   // this is used for percentage closer filtering for shadow maps
        ci.compareOp               = VK_COMPARE_OP_GREATER;
        ci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        ci.mipLodBias              = 0.0f;
        ci.minLod                  = 0.0f;
        ci.maxLod                  = 1.0f;

        VK_CHECK(vkCreateSampler(VulkanContext::GetDevice(), &ci, nullptr, &m_shadowSamplerPCF), "Failed to create shadow texture sampler");
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
    createInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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
    ComponentManager* cm = m_ecs->componentManager;
    std::vector<DrawCommand> drawCommands;
    for(auto* material : cm->GetComponents<Material>())
    {
        auto* renderable = cm->GetComponent<Renderable>(material->GetOwner());
        DrawCommand dc{};
        dc.indexCount    = renderable->indexCount;
        dc.instanceCount = 1;
        dc.firstIndex    = renderable->indexOffset;
        dc.vertexOffset  = static_cast<int32_t>(renderable->vertexOffset);
        dc.firstInstance = 0;
        // dc.objectID      = renderable->objectID;

        drawCommands.push_back(dc);
    }
    m_drawBuffer.Fill(drawCommands.data(), drawCommands.size() * sizeof(DrawCommand));
    m_numDrawCommands = drawCommands.size();

    m_needDrawBufferReupload = false;
}

void Renderer::RefreshShaderDataOffsets()
{
}

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
std::tuple<std::array<glm::mat4, NUM_CASCADES>, std::array<glm::mat4, NUM_CASCADES>, std::array<glm::vec2, NUM_CASCADES>> GetCascadeMatricesOrtho(const glm::mat4& invCamera, const glm::vec3& lightDir, float zNear, float maxDepth)
{
    std::array<glm::mat4, NUM_CASCADES> resVP;
    std::array<glm::mat4, NUM_CASCADES> resV;
    std::array<glm::vec2, NUM_CASCADES> zPlanes;

    float maxDepthNDC  = zNear / maxDepth;
    float depthNDCStep = (1 - maxDepthNDC) / NUM_CASCADES;
    float step         = maxDepth / NUM_CASCADES;
    for(int i = 0; i < NUM_CASCADES; ++i)
    {
        float cascadeDepthStart = zNear / glm::max(zNear, (i * step));
        float cascadeDepthEnd   = zNear / ((i + 1) * step);

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
        zPlanes[i] = glm::vec2(max.z, min.z);
    }
    return {resVP, resV, zPlanes};
}

void Renderer::UpdateLights(uint32_t index)
{
    PROFILE_FUNCTION();
    {
        PROFILE_SCOPE("Loop #1");

        for(auto* lightComp : m_ecs->componentManager->GetComponents<DirectionalLight>())
        {
            Transform* t   = m_ecs->componentManager->GetComponent<Transform>(lightComp->GetOwner());
            glm::mat4 tMat = t->GetTransform();

            glm::vec3 newDir   = glm::normalize(-glm::vec3(tMat[2]));
            float newIntensity = lightComp->intensity;
            glm::vec3 newColor = lightComp->color.ToVec3();


            Light& light = m_lightMap[lightComp->GetComponentID()];
            bool changed = newDir != light.direction || newIntensity != light.intensity || newColor != light.color;
            if(changed)
            {
                light.direction = newDir;
                light.intensity = newIntensity;
                light.color     = newColor;

                for(auto& dict : m_changedLights)
                    dict[lightComp->_slot] = &light;  // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
            }
            // light.lightSpaceMatrices = GetCascadeMatricesOrtho(glm::mat4(1.0), newDir,
        }
    }
    {
        PROFILE_SCOPE("Loop #2");

        for(auto* lightComp : m_ecs->componentManager->GetComponents<PointLight>())
        {
            Transform* t   = m_ecs->componentManager->GetComponent<Transform>(lightComp->GetOwner());
            glm::mat4 tMat = t->GetTransform();

            glm::vec3 newPos = glm::vec3(tMat[3]);

            float newRange     = lightComp->range;
            float newIntensity = lightComp->intensity;
            glm::vec3 newColor = lightComp->color.ToVec3();
            Attenuation newAtt = lightComp->attenuation;

            Light& light = m_lightMap[lightComp->GetComponentID()];
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
                    dict[lightComp->_slot] = &light;  // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
            }
        }
    }
    {
        PROFILE_SCOPE("Loop #3");

        for(auto* lightComp : m_ecs->componentManager->GetComponents<SpotLight>())
        {
            Transform* t   = m_ecs->componentManager->GetComponent<Transform>(lightComp->GetOwner());
            glm::mat4 tMat = t->GetTransform();

            glm::vec3 newPos   = glm::vec3(tMat[3]);
            glm::vec3 newDir   = glm::normalize(glm::vec3(tMat[2]));
            float newRange     = lightComp->range;
            float newIntensity = lightComp->intensity;
            float newCutoff    = lightComp->cutoff;
            glm::vec3 newColor = lightComp->color.ToVec3();

            Attenuation newAtt = lightComp->attenuation;


            Light& light = m_lightMap[lightComp->GetComponentID()];
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
                    dict[lightComp->_slot] = &light;  // if it isn't in the dict then put it in otherwise just replace it (which actually does nothing but we would have to check if the dict contains it anyway so shouldn't matter for perf)
            }
        }
    }

    std::vector<std::pair<uint32_t, void*>> slotsAndDatas;
    for(auto& [slot, newLight] : m_changedLights[index])
    {
        slotsAndDatas.push_back({slot, newLight});
        m_lightsBuffers[index]->UploadData(slot, newLight);  // TODO batch the upload together
    }
    m_changedLights[index].clear();

    // m_lightsBuffers[index]->UpdateBuffer(slotsAndDatas);
}

void Renderer::UpdateLightMatrices(uint32_t index)
{
    ComponentManager* cm       = m_ecs->componentManager;
    Camera camera              = *(*(cm->begin<Camera>()));
    Transform* cameraTransform = cm->GetComponent<Transform>(camera.GetOwner());
    glm::mat4 inverseVP        = cameraTransform->GetTransform() * glm::inverse(camera.GetProjection());  // TODO implement a function for inverse projection matrix instead of using generic inverse
    for(auto* light : cm->GetComponents<DirectionalLight>())
    {
        Light internalLight                   = m_lightMap[light->GetComponentID()];
        auto [cascadesVP, cascadesV, zPlanes] = GetCascadeMatricesOrtho(inverseVP, internalLight.direction, camera.zNear, MAX_SHADOW_DEPTH);

        ShadowMatrices data{};
        data.lightSpaceMatrices = cascadesVP;
        data.lightViewMatrices  = cascadesV;
        data.zPlanes            = zPlanes;

        m_shadowMatricesBuffers[index].UploadData(internalLight.matricesSlot, &data);
    }
}

void Renderer::Render(double dt)
{
    PROFILE_FUNCTION();
    uint32_t imageIndex;
    VkResult result;
    {
        PROFILE_SCOPE("Pre frame stuff");
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

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
        ComponentManager* cm       = m_ecs->componentManager;
        Camera camera              = *(*(cm->begin<Camera>()));
        Transform* cameraTransform = cm->GetComponent<Transform>(camera.GetOwner());
        glm::mat4 proj             = camera.GetProjection();

        glm::mat4 vp = proj * glm::inverse(cameraTransform->GetTransform());  // TODO inversing the transform like this isnt too fast, consider only allowing camera on root level entity so we can just -pos and -rot
        UpdateLights(imageIndex);
        UpdateLightMatrices(imageIndex);
        m_pushConstants.cameraPos = cameraTransform->pos;
        // m_ubAllocators["camera" + std::to_string(imageIndex)]->UpdateBuffer(0, &cs);

        if(m_needDrawBufferReupload)
        {
            for(auto* transform : cm->GetComponents<Transform>())
            {
                if(cm->HasComponent<Renderable>(transform->GetOwner()))
                {
                    glm::mat4 model = transform->GetTransform();

                    for(auto& buffer : m_transformBuffers)
                        buffer.UploadData(transform->ub_id, &model);
                }
            }
            RefreshDrawCommands();
        }
    }

    /*
        ComponentManager* cm = m_ecs->componentManager;


        // TODO: make something better for the cameras, now it only takes the first camera that was added to the componentManager
        // also a single camera has a whole chunk of memory which isnt good if we only use 1 camera per game
        Camera camera = *(*(cm->begin<Camera>()));
        Transform* cameraTransform = cm->GetComponent<Transform>(camera.GetOwner());
        auto materialIterator = cm->begin<Material>();
        auto end = cm->end<Material>();
        EntityID currentEntity = materialIterator->GetOwner();

        RenderPass* lastRenderPass = nullptr;
        m_mainCommandBuffers[imageIndex].Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VkCommandBuffer cb = m_mainCommandBuffers[imageIndex].GetCommandBuffer();
        {
            PROFILE_SCOPE("Command Recording and submitting");
            vkCmdResetQueryPool(cb, m_queryPools[m_currentFrame], 0, 4);

            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPools[m_currentFrame],0);




            glm::mat4 proj = camera.GetProjection();
            //glm::mat4 trf = glm::translate(-cameraTransform->wPosition);
            //glm::mat4 rot = glm::toMat4(glm::conjugate(cameraTransform->wRotation));
            //glm::mat4 vp = proj * rot * trf;
            glm::mat4 vp = proj * glm::inverse(cameraTransform->GetTransform()); //TODO inversing the transform like this isnt too fast, consider only allowing camera on root level entity so we can just -pos and -rot
            uint32_t vpOffset = 0;

            CameraStruct cs = {vp, cameraTransform->pos};
            m_ubAllocators["camera" + std::to_string(imageIndex)]->UpdateBuffer(0, &cs);

            auto prepassBegin = m_depthPipeline->m_renderPass->GetBeginInfo(imageIndex);
            vkCmdBeginRenderPass(cb, &prepassBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_pipeline);

            // This should be fine since all our shaders use set 0 for the camera so the descriptor doesn't get unbound by pipeline switches
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_layout, 0, 1, &m_cameraDescSets[imageIndex], 1, &vpOffset);
            {
                PROFILE_SCOPE("Prepass draw call loop");
                for(auto* renderable : cm->GetComponents<Renderable>())
                {
                    auto transform  = cm->GetComponent<Transform>(renderable->GetOwner());
                    glm::mat4 model =  transform->GetTransform();
                    {
                        PROFILE_SCOPE("Bind buffers");
                        renderable->vertexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
                        renderable->indexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
                    }
                    uint32_t offset;
                    uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(transform->ub_id, offset);

                    m_ubAllocators["transforms" + std::to_string(imageIndex)]->UpdateBuffer(transform->ub_id, &model);

                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthPipeline->m_layout, 2, 1, &m_transformDescSets[imageIndex][bufferIndex], 1, &offset);
                    vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)
                }
            }
            vkCmdEndRenderPass(cb);
            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPools[m_currentFrame], 1);


            {
                glm::mat4 inverseVP = cameraTransform->GetTransform() * glm::inverse(camera.GetProjection()); // TODO implement a function for inverse projection matrix instead of using generic inverse
                auto shadowBegin = m_shadowRenderPass.GetBeginInfo(0);
                PROFILE_SCOPE("Shadow pass draw call loop");
                for(auto* light : cm->GetComponents<DirectionalLight>())
                {
                    Light internalLight = m_lightMap[light->GetComponentID()];
                    auto [cascadesVP, cascadesV, zPlanes] = GetCascadeMatricesOrtho(inverseVP, internalLight.direction, camera.zNear, MAX_SHADOW_DEPTH);
                    std::cout << "VP: " << glm::to_string(cascadesVP[0] * glm::vec4(cameraTransform->pos, 1.0)) << std::endl;
                    std::cout << "V:  " << glm::to_string(cascadesV[0] * glm::vec4(cameraTransform->pos, 1.0)) << std::endl;

                    internalLight.lightSpaceMatrices = cascadesVP;
                    internalLight.lightViewMatrices = cascadesV;
                    internalLight.zPlanes = zPlanes;
                    m_lightsBuffers[imageIndex]->UpdateBuffer(light->_slot, &internalLight);

                    for (int i = 0; i < NUM_CASCADES; ++i)
                    {

                        VkRenderPassAttachmentBeginInfo attachmentBegin = {};
                        attachmentBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO;
                        attachmentBegin.attachmentCount = 1;
                        VkImageView view = m_shadowmaps[light->_shadowSlot][i]->GetImageView();
                        attachmentBegin.pAttachments = &view;
                        shadowBegin.pNext = &attachmentBegin;

                        vkCmdBeginRenderPass(cb, &shadowBegin, VK_SUBPASS_CONTENTS_INLINE);
                        if(i == 0) // only bind these once, they will stay the same among the cascades
                        {
                            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_pipeline);
                            uint32_t offset = 0;
                            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_layout, 0, 1, &m_shadowDesc[imageIndex], 1, &offset);
                        }
                        struct {
                            uint32_t slot;
                            uint32_t cascadeIndex;
                        } pc;
                        pc.slot = light->_slot;
                        pc.cascadeIndex = i;
                        vkCmdPushConstants(cb, m_shadowPipeline->m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);


                        for(auto* renderable : cm->GetComponents<Renderable>())
                        {
                            auto transform  = cm->GetComponent<Transform>(renderable->GetOwner());
                            {
                                PROFILE_SCOPE("Bind buffers");
                                renderable->vertexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
                                renderable->indexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
                            }
                            uint32_t offset;
                            uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(transform->ub_id, offset);


                            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->m_layout, 2, 1, &m_transformDescSets[imageIndex][bufferIndex], 1, &offset);
                            vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)
                        }
                        vkCmdEndRenderPass(cb);
                    }
                }
            }
            //TODO timestamp



            std::vector<VkBufferMemoryBarrier> barriersBefore(2);
            barriersBefore[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriersBefore[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriersBefore[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriersBefore[0].buffer = m_lightsBuffers[imageIndex]->GetBuffer(0);
            barriersBefore[0].size = m_lightsBuffers[imageIndex]->GetSize();
            barriersBefore[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriersBefore[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriersBefore[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriersBefore[1].buffer = m_visibleLightsBuffers[imageIndex]->GetBuffer(0);
            barriersBefore[1].size = m_visibleLightsBuffers[imageIndex]->GetSize();

            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriersBefore.size(), barriersBefore.data(), 0, nullptr);
            uint32_t offset = 0;
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_pipeline);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute->m_layout, 0, 1, &m_computeDesc[imageIndex], 1, &offset);


            m_computePushConstants.debugMode = 1;
            vkCmdPushConstants(cb, m_compute->m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &m_computePushConstants);
            vkCmdDispatch(cb, m_computePushConstants.tileNums.x, m_computePushConstants.tileNums.y, 1);

            std::vector<VkBufferMemoryBarrier> barriersAfter(2);
            barriersAfter[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriersAfter[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriersAfter[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriersAfter[0].buffer = m_lightsBuffers[imageIndex]->GetBuffer(0);
            barriersAfter[0].size = m_lightsBuffers[imageIndex]->GetSize();
            barriersAfter[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriersAfter[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriersAfter[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriersAfter[1].buffer = m_visibleLightsBuffers[imageIndex]->GetBuffer(0);
            barriersAfter[1].size = m_visibleLightsBuffers[imageIndex]->GetSize();
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, barriersAfter.size(), barriersAfter.data(), 0, nullptr);

            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_queryPools[m_currentFrame], 2);

            for(auto& pipeline : m_pipelines)
            {
                //if(pipeline.m_name == "depth") continue;
                auto tempIt = materialIterator;// to be able to restore the iterator to its previous place after global pipeline is finished
                if(pipeline.m_isGlobal)
                    materialIterator = cm->begin<Material>();

                if(pipeline.m_renderPass != lastRenderPass)
                {
                    if(lastRenderPass)
                        vkCmdEndRenderPass(cb);

                    lastRenderPass = pipeline.m_renderPass;
                    auto beginInfo = lastRenderPass->GetBeginInfo(imageIndex);
                    vkCmdBeginRenderPass(cb, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

                    //vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 0, 1, m_descriptorSets[pipeline.m_name + "0"][imageIndex], __, __); TODO use a global descriptor

                }


                //vkCmdPushConstants(m_mainCommandBuffers[imageIndex].GetCommandBuffer(), pipeline.m_layout,

                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_pipeline);
                vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 0, 1, &m_tempDesc[imageIndex], 1, &offset);
                vkCmdPushConstants(cb, pipeline.m_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ComputePushConstants), &m_computePushConstants);


                PROFILE_SCOPE("Draw call loop");
                while(materialIterator != end && (pipeline.m_isGlobal || materialIterator->shaderName == pipeline.m_name))
                {
                    Transform* transform = cm->GetComponent<Transform>(currentEntity);
                    Renderable* renderable = cm->GetComponent<Renderable>(currentEntity);
                    if(renderable == nullptr)
                        continue;

                    // We don't need to update the transforms here, because they have already been updated in the depth prepass draw loop

                    //transform->rot = glm::rotate(transform->rot,  i * 0.001f * glm::radians(90.0f), glm::vec3(0,0,1));
                    //glm::mat4 model =  transform->GetTransform();

                    renderable->vertexBuffer.Bind(m_mainCommandBuffers[imageIndex]);
                    renderable->indexBuffer.Bind(m_mainCommandBuffers[imageIndex]);

                    uint32_t offset;
                    uint32_t bufferIndex = m_ubAllocators["transforms" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(transform->ub_id, offset);

                    //m_ubAllocators["transforms" + std::to_string(imageIndex)]->UpdateBuffer(transform->ub_id, &model);
                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 2, 1, &m_transformDescSets[imageIndex][bufferIndex], 1, &offset);


                    if(!pipeline.m_isGlobal)
                    {
                        m_ubAllocators[pipeline.m_name + "material" + std::to_string(imageIndex)]->GetBufferIndexAndOffset(materialIterator->_ubSlot, offset);

                        glm::vec4 ubs[] = {{materialIterator->_textureSlot, 0, 0, 0}, {0.5f, 0,0,1.0f}, };

                        m_ubAllocators[pipeline.m_name + "material" + std::to_string(imageIndex)]->UpdateBuffer(materialIterator->_ubSlot, ubs);

                    }
                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_layout, 1, 1, &m_descriptorSets[pipeline.m_name + std::to_string(1)][bufferIndex + imageIndex], 1, &offset);
                    vkCmdDrawIndexed(cb, static_cast<uint32_t>(renderable->indexBuffer.GetSize() / sizeof(uint32_t)) , 1, 0, 0, 0); // TODO: make the size calculation better (not hardcoded for uint32_t)


                    materialIterator++;
                    if(materialIterator != end)
                        currentEntity = materialIterator->GetOwner();
                }
                if(pipeline.m_isGlobal && tempIt != end)
                {
                    materialIterator = tempIt;
                    currentEntity = materialIterator->GetOwner();
                }
            }



            m_debugUI->Draw(&m_mainCommandBuffers[imageIndex]);


            vkCmdEndRenderPass(cb);
            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPools[m_currentFrame], 3);
            m_mainCommandBuffers[imageIndex].End();


        }
    */
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


void Renderer::OnMeshComponentAdded(const ComponentAdded<Mesh>* e)
{
    // TODO don't keep the vertex and index vectors after sending them to gpu
    Mesh* mesh               = e->component;
    VkDeviceSize vBufferSize = sizeof(mesh->vertices[0]) * mesh->vertices.size();
    Renderable* comp         = m_ecs->componentManager->AddComponent<Renderable>(e->entity);


    // create the vertex and index buffers on the gpu
    bool didVBResize    = false;
    uint64_t vertexSlot = m_vertexBuffer.Allocate(mesh->vertices.size(), didVBResize, (void*)&comp);
    m_vertexBuffer.UploadData(vertexSlot, mesh->vertices.data());

    comp->vertexOffset = vertexSlot;
    comp->vertexCount  = mesh->vertices.size();

    bool didIBResize   = false;
    uint64_t indexSlot = m_indexBuffer.Allocate(mesh->indices.size(), didIBResize, &comp);
    m_indexBuffer.UploadData(indexSlot, mesh->indices.data());

    comp->indexOffset = indexSlot;
    comp->indexCount  = mesh->indices.size();

    if(didVBResize)
    {
        for(const auto& [slot, info] : m_vertexBuffer.GetAllocationInfos())
        {
            auto* renderable         = (Renderable*)info.pUserData;
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
    /*
    Buffer stagingVertexBuffer(vBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingVertexBuffer.Fill((void*)mesh->vertices.data(), vBufferSize);

    VkDeviceSize iBufferSize = sizeof(mesh->indices[0]) * mesh->indices.size();
    Buffer stagingIndexBuffer(iBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingIndexBuffer.Fill((void*)mesh->indices.data(), iBufferSize);

    Renderable* comp   = m_ecs->componentManager->AddComponent<Renderable>(e->entity);
    comp->vertexBuffer = Buffer(vBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    comp->indexBuffer  = Buffer(iBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    stagingVertexBuffer.Copy(&comp->vertexBuffer, vBufferSize);

    stagingIndexBuffer.Copy(&comp->indexBuffer, iBufferSize);

    stagingVertexBuffer.Free();
    stagingIndexBuffer.Free();
    */

    uint32_t slot = 0;
    for(auto& buffer : m_transformBuffers)
    {
        bool didResize = false;
        slot           = buffer.Allocate(1, didResize);
    }
    m_ecs->componentManager->GetComponent<Transform>(e->entity)->ub_id = slot;

    comp->objectID = slot;

    m_needDrawBufferReupload = true;
}

void Renderer::OnMeshComponentRemoved(const ComponentRemoved<Mesh>* e)
{
    m_ecs->componentManager->RemoveComponent<Renderable>(e->entity);

    /*
    for(uint32_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        uint32_t id = m_ecs->componentManager->GetComponent<Transform>(e->entity)->ub_id;
        m_ubAllocators["transforms" + std::to_string(i)]->Free(id);
    }
    */
}

void Renderer::OnMaterialComponentAdded(const ComponentAdded<Material>* e)
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

void Renderer::OnDirectionalLightAdded(const ComponentAdded<DirectionalLight>* e)
{
    uint32_t slot = 0;
    bool tmp      = false;
    for(auto& buffer : m_lightsBuffers)
        slot = buffer->Allocate(1, tmp);  // slot should be the same for all of these, since we allocate to every buffer every time

    e->component->_slot = slot;

    m_lightMap[e->component->GetComponentID()] = {0};  // 0 = DirectionalLight
    Light* light                               = &m_lightMap[e->component->GetComponentID()];

    uint32_t matricesSlot = 0;
    for(auto& buffer : m_shadowMatricesBuffers)
        matricesSlot = buffer.Allocate(1, tmp);  // slot should be the same for all of these, since we allocate to every buffer every time

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

    slot                      = m_shadowmaps.size();
    e->component->_shadowSlot = slot;


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
    uint32_t shadowSlot = m_shadowIndices.Allocate(1, tmp);
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

void Renderer::OnPointLightAdded(const ComponentAdded<PointLight>* e)
{
    uint32_t slot = 0;
    bool tmp      = false;
    for(auto& buffer : m_lightsBuffers)
        slot = buffer->Allocate(1, tmp);  // slot should be the same for all of these, since we allocate to every buffer every time


    e->component->_slot = slot;

    m_lightMap[e->component->GetComponentID()] = {1};  // 1 = PointLight
    Light* light                               = &m_lightMap[e->component->GetComponentID()];

    for(auto& dict : m_changedLights)
    {
        dict[slot] = light;
    }
}

void Renderer::OnSpotLightAdded(const ComponentAdded<SpotLight>* e)
{
    uint32_t slot = 0;
    bool tmp      = false;
    for(auto& buffer : m_lightsBuffers)
        slot = buffer->Allocate(1, tmp);  // slot should be the same for all of these, since we allocate to every buffer every time

    e->component->_slot = slot;

    m_lightMap[e->component->GetComponentID()] = {2};  // 2 = SpotLight
    Light* light                               = &m_lightMap[e->component->GetComponentID()];

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
