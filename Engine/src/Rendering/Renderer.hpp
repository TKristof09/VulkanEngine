#pragma once

#include "ECS/CoreEvents/ComponentEvents.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <set>

#include "Rendering/RenderGraph/RenderGraph.hpp"
#include "Utils/DebugUIElements.hpp"
#include "Window.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "Utils/DebugUI.hpp"
#include "ECS/CoreComponents/Mesh.hpp"
#include "ECS/CoreComponents/Material.hpp"
#include "Pipeline.hpp"
#include "ECS/ECS.hpp"
#include "ECS/CoreComponents/Lights.hpp"

#define SHADOWMAP_SIZE   2048
#define MAX_SHADOW_DEPTH 2000
#define NUM_CASCADES     4

class Renderer
{
public:
    Renderer(std::shared_ptr<Window> window, Scene* scene);
    ~Renderer();
    void Render(double dt);

    Pipeline* AddPipeline(const std::string& name, PipelineCreateInfo createInfo, uint32_t priority);

    void AddDebugUIWindow(DebugUIWindow* window) { m_debugUI->AddWindow(window); };

    void OnMeshComponentAdded(const ComponentAdded<Mesh>* e);
    void OnMeshComponentRemoved(const ComponentRemoved<Mesh>* e);
    void OnMaterialComponentAdded(const ComponentAdded<Material>* e);
    void OnDirectionalLightAdded(const ComponentAdded<DirectionalLight>* e);
    void OnPointLightAdded(const ComponentAdded<PointLight>* e);
    void OnSpotLightAdded(const ComponentAdded<SpotLight>* e);

    void AddTexture(Image* texture);
    void RemoveTexture(Image* texture);

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

        glm::vec3 filler;
        uint32_t shadowSlot;
        uint32_t matricesSlot;
    };

    struct ShadowMatrices
    {
        std::array<glm::mat4, NUM_CASCADES> lightSpaceMatrices;  // potentially put the shadowslot in the matrix
        std::array<glm::mat4, NUM_CASCADES> lightViewMatrices;   // potentially put the shadowslot in the matrix
        std::array<glm::vec2, NUM_CASCADES> zPlanes;
    };

    struct TileLights  // TODO
    {
        glm::uint count;
        glm::uint indices[1024];
    };

    struct PushConstants
    {
        glm::mat4 viewProj;
        glm::vec3 cameraPos;
        int32_t debugMode;
        uint64_t shaderDataPtr;
        uint64_t transformBufferPtr;
        uint64_t objectIdMapPtr;
        uint64_t materialDataPtr;
    };

    PushConstants m_pushConstants;

    std::unique_ptr<Pipeline> m_compute;
    std::vector<std::unique_ptr<DynamicBufferAllocator>> m_lightsBuffers;
    std::vector<std::unique_ptr<Buffer>> m_visibleLightsBuffers;
    VkBuffer m_visibleLightsBuffer;
    VkSampler m_computeSampler;  // TODO
    std::unordered_map<ComponentID, Light> m_lightMap;
    std::vector<std::unordered_map<uint32_t, Light*>> m_changedLights;
    void UpdateLights(uint32_t index);
    void UpdateLightMatrices(uint32_t index);

    std::shared_ptr<Image> m_lightCullDebugImage;

    std::vector<std::vector<std::unique_ptr<Image>>> m_shadowmaps;  // non point lights, store NUM_CASCADES images for each light
    std::unique_ptr<Pipeline> m_shadowPipeline;
    VkSampler m_shadowSampler;
    VkSampler m_shadowSamplerPCF;
    // std::vector<std::unique_ptr<Image>> m_pointLightShadowmaps; //cube maps


    std::unique_ptr<Pipeline> m_depthPipeline;

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

    void CreateColorResources();
    void CreateDepthResources();

    void CreateSampler();

    void RefreshDrawCommands();

    void RefreshShaderDataOffsets();

    void SetupDebugMessenger();

    ECSEngine* m_ecs;

    std::shared_ptr<Window> m_window;

    std::shared_ptr<DebugUI> m_debugUI;

    std::vector<VkQueryPool> m_queryPools;
    std::vector<uint64_t> m_queryResults;
    uint64_t m_timestampPeriod;


    VkInstance& m_instance;
    VkPhysicalDevice& m_gpu;
    VkDevice& m_device;

    VkQueue& m_graphicsQueue;
    VkQueue m_presentQueue;
    VkQueue m_computeQueue;

    VkSurfaceKHR m_surface;

    VkSwapchainKHR m_swapchain;
    std::vector<std::shared_ptr<Image>> m_swapchainImages;
    VkFormat m_swapchainImageFormat;
    VkExtent2D m_swapchainExtent;


    std::multiset<Pipeline> m_pipelines;
    std::unordered_map<std::string, Pipeline*> m_pipelinesRegistry;

    // std::unordered_map<std::string, std::unique_ptr<BufferAllocator>> m_ubAllocators;  // key: pipelineName + uboName(from shader) and "transforms" -> ub for storing the model matrices and "camera" -> VP matrix TODO: dont need one for the camera since its a global thing


    VkCommandPool& m_commandPool;
    std::vector<CommandBuffer> m_mainCommandBuffers;

    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;

    VkDescriptorPool m_descriptorPool;

    std::shared_ptr<Image> m_depthImage;

    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    std::shared_ptr<Image> m_colorImage;  // for MSAA

    size_t m_currentFrame = 0;


    std::unique_ptr<DebugUIWindow> m_rendererDebugWindow;

    RenderGraph m_renderGraph;

    DynamicBufferAllocator m_vertexBuffer;
    DynamicBufferAllocator m_indexBuffer;

    DynamicBufferAllocator m_shaderDataBuffer;
    std::array<std::unordered_map<std::string, uint64_t>, NUM_FRAMES_IN_FLIGHT> m_shaderDataOffsets;  // shader name -> offset in the shader data buffer

    std::list<int32_t> m_freeTextureSlots;  // i think having it sorted will be better for the gpu so the descriptor set doesnt get so fragmented

    std::vector<DynamicBufferAllocator> m_shadowMatricesBuffers;

    // TODO temp
    Buffer m_drawBuffer;
    uint32_t m_numDrawCommands;
    bool m_needDrawBufferReupload = false;
    std::vector<DynamicBufferAllocator> m_transformBuffers;

    RenderingTextureArrayResource* m_shadowMaps;
    DynamicBufferAllocator m_shadowIndices;
    uint32_t m_numShadowIndices = 0;

    // TODO remove
    Entity* m_frustrumEntity;
};
