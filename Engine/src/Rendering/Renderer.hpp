#pragma once

#include "Core/Events/CoreEvents.hpp"
#include "ECS/CoreComponents/BoundingBox.hpp"
#include "ECS/CoreComponents/InternalTransform.hpp"
#include "ECS/CoreComponents/Renderable.hpp"
#include "ECS/CoreEvents/ComponentEvents.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <set>

#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Rendering/Sampler.hpp"
#include "Utils/DebugUIElements.hpp"
#include "Window.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "Utils/DebugUI.hpp"

#include "ECS/Core.hpp"
#include "ECS/CoreComponents/Lights.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/CoreComponents/Material.hpp"


#define SHADOWMAP_SIZE   2048
#define MAX_SHADOW_DEPTH 1000
#define NUM_CASCADES     4

class Pipeline;
struct PipelineCreateInfo;
struct TransformBuffers;

class DepthPass;
class DrawcullPass;
class LightCullPass;
class LightingPass;
class SkyboxPass;
class ShadowPass;
class GTAOPass;
class DenoisePass;

class Renderer
{
public:
    Renderer(std::shared_ptr<Window> window);
    ~Renderer();
    Renderer(const Renderer&)            = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&)      = delete;
    void InitilizeRenderGraph();
    void Render(double dt);

    void SetupEnvironmentMaps();

    Pipeline* AddPipeline(const std::string& name, PipelineCreateInfo createInfo, uint32_t priority);

    void AddDebugUIWindow(DebugUIWindow* window) { m_debugUI->AddWindow(window); };
    void AddDebugUIElement(const std::shared_ptr<DebugUIElement>& element) { m_rendererDebugWindow->AddElement(element); };
    void AddShaderButton(std::string_view name) { m_shaderButtons.push_back(name); };

    void AddDebugUIImage(const RenderingTextureResource& image)
    {
        m_uiImages.push_back(image);
    }

    void OnSceneSwitched(SceneSwitchedEvent e);

    void OnMeshComponentAdded(ComponentAdded<Mesh> e);
    void OnMeshComponentRemoved(ComponentRemoved<Mesh> e);
    void OnMaterialComponentAdded(ComponentAdded<Material> e);
    void OnDirectionalLightAdded(ComponentAdded<DirectionalLight> e);
    void OnPointLightAdded(ComponentAdded<PointLight> e);
    void OnSpotLightAdded(ComponentAdded<SpotLight> e);

    void AddTexture(Image* texture, SamplerConfig samplerConf = {});
    void RemoveTexture(Image* texture);
    void AddStorageImage(Image* img);
    void RemoveStorageImage(Image* img);

    DynamicBufferAllocator& GetShaderDataBuffer() { return *m_shaderDataBuffer; }

private:
    struct Light
    {
        int type;
        float attenuation[3];

        glm::vec3 color;  // use vec3 to have the struct take up 128 bytes
        float intensity;

        glm::vec3 direction;  // only for directional or spot
        float cutoff;         // only for spot

        glm::vec3 position;
        float range;  // only for spot and point

        uint32_t shadowSlot;
        uint32_t matricesSlot;
        glm::vec2 filler;
    };

    struct ShadowMatrices
    {
        std::array<glm::mat4, NUM_CASCADES> lightSpaceMatrices;  // potentially put the shadowslot in the matrix
        std::array<glm::mat4, NUM_CASCADES> lightViewMatrices;   // potentially put the shadowslot in the matrix
        std::array<glm::vec2, NUM_CASCADES> zPlanes;
    };

    struct TileLights
    {
        glm::uint count;
        glm::uint indices[1024];
    };

    struct PushConstants
    {
        glm::mat4 viewProj;   // this is set by the renderer, not each pass (might change in the future if we start needing multiple cameras for things like reflections or idk)
        glm::vec3 cameraPos;  // this is set by the renderer, not each pass (might change in the future if we start needing multiple cameras for things like reflections or idk)
        int32_t debugMode;
        float data[4];
        uint64_t shaderDataPtr;
        uint64_t transformBufferPtr;
        uint64_t objectIdMapPtr;
        uint64_t materialDataPtr;
    };


    std::unique_ptr<Pipeline> m_compute;
    std::unordered_map<uint32_t, Light> m_lightMap;
    std::vector<std::unordered_map<uint32_t, Light*>> m_changedLights;
    void UpdateLights(uint32_t index);
    void UpdateLightMatrices(uint32_t index);

    std::shared_ptr<Image> m_lightCullDebugImage;

    std::vector<std::unique_ptr<Image>> m_shadowmaps;

    friend class MaterialSystem;

    // vulkan initialization stuff
    void CreateDebugUI();

    void CreateInstance();
    void CreateSurface();
    void CreateDevice();
    void CreateVmaAllocator();
    void CreateSwapchain();
    void CreatePipeline();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();

    void RecreateSwapchain();
    void CleanupSwapchain();


    void CreateDrawBuffers();

    void CreateUniformBuffers();

    void CreateDescriptorPool();
    void CreateDescriptorSet();
    static void CreatePushConstants();


    void RefreshDrawCommands();

    void RefreshShaderDataOffsets();

    void SetupDebugMessenger();

    void CreateEnvironmentMap();


    ECS* m_ecs;

    std::shared_ptr<Window> m_window;

    std::shared_ptr<DebugUI> m_debugUI;

    std::vector<VkQueryPool> m_queryPools;
    std::vector<uint64_t> m_queryResults;
    uint64_t m_timestampPeriod{UINT64_MAX};


    VkInstance& m_instance;
    VkPhysicalDevice& m_gpu;
    VkDevice& m_device;

    Queue& m_graphicsQueue;
    Queue m_presentQueue{};
    Queue& m_computeQueue;  // unused for now
    Queue& m_transferQueue;

    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    VkSurfaceKHR m_surface{VK_NULL_HANDLE};

    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    std::vector<std::shared_ptr<Image>> m_swapchainImages;
    VkFormat m_swapchainImageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapchainExtent{};


    std::multiset<Pipeline> m_pipelines;
    std::unordered_map<std::string, Pipeline*> m_pipelinesRegistry;


    VkCommandPool& m_graphicsCommandPool;
    VkCommandPool& m_transferCommandPool;
    std::vector<CommandBuffer> m_mainCommandBuffers;

    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;

    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};


    size_t m_currentFrame = 0;


    std::unique_ptr<DebugUIWindow> m_rendererDebugWindow;

    RenderGraph m_renderGraph;

    std::unordered_map<SamplerConfig, Sampler> m_samplers;

    std::unique_ptr<DynamicBufferAllocator> m_vertexBuffer;
    std::unique_ptr<DynamicBufferAllocator> m_indexBuffer;

    std::unique_ptr<DynamicBufferAllocator> m_shaderDataBuffer;

    // TODO temp
    bool m_needDrawBufferReupload = false;


    std::list<int32_t> m_freeTextureSlots;  // i think having it sorted will be better for the gpu so the descriptor set doesnt get so fragmented
    std::list<int32_t> m_freeStorageImageSlots;

    std::unique_ptr<Pipeline> m_equiToCubePipeline;
    std::unique_ptr<Pipeline> m_convoltionPipeline;
    std::unique_ptr<Pipeline> m_prefilterPipeline;
    std::unique_ptr<Pipeline> m_computeBRDFPipeline;


    // Renderpasses
    std::unique_ptr<DepthPass> m_depthPass;
    std::unique_ptr<DrawcullPass> m_drawCullPass;
    std::unique_ptr<ShadowPass> m_shadowPass;
    std::unique_ptr<LightCullPass> m_lightCullPass;
    std::unique_ptr<LightingPass> m_lightingPass;
    std::unique_ptr<SkyboxPass> m_skyboxPass;
    std::unique_ptr<GTAOPass> m_gtaoPass;
    std::unique_ptr<DenoisePass> m_denoisePass;

    std::vector<RenderingTextureResource> m_uiImages;
    std::vector<std::string_view> m_shaderButtons;


    RenderingTextureArrayResource* m_shadowMaps{nullptr};

    // ECS queries
    Query<const InternalTransform, const Renderable, TransformBuffers> m_transformsQuery;
    Query<const Renderable, const BoundingBox> m_renderablesQuery;

    Query<const DirectionalLight, const InternalTransform> m_directionalLightsQuery;
    Query<const PointLight, const InternalTransform> m_pointLightsQuery;
    Query<const SpotLight, const InternalTransform> m_spotLightsQuery;
};
