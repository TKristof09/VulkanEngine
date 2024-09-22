#pragma once

#include "Rendering/Buffer.hpp"
#include "Rendering/Image.hpp"

struct DrawCommandBuffer
{
    DynamicBufferAllocator buffer;
    uint32_t count{};

    template<typename... Args,
             std::enable_if_t<std::is_constructible_v<DynamicBufferAllocator, Args...>, int> = 0>
    DrawCommandBuffer(Args&&... args) : buffer(std::forward<Args>(args)...)
    {
    }

    ~DrawCommandBuffer() = default;

    DrawCommandBuffer(const DrawCommandBuffer&)            = delete;
    DrawCommandBuffer& operator=(const DrawCommandBuffer&) = delete;

    DrawCommandBuffer(DrawCommandBuffer&&)            = default;
    DrawCommandBuffer& operator=(DrawCommandBuffer&&) = default;
};

struct BoundingBoxBuffer
{
    DynamicBufferAllocator buffer;
    uint32_t count{};

    template<typename... Args,
             std::enable_if_t<std::is_constructible_v<DynamicBufferAllocator, Args...>, int> = 0>
    BoundingBoxBuffer(Args&&... args) : buffer(std::forward<Args>(args)...)
    {
    }

    ~BoundingBoxBuffer() = default;

    BoundingBoxBuffer(const BoundingBoxBuffer&)            = delete;
    BoundingBoxBuffer& operator=(const BoundingBoxBuffer&) = delete;

    BoundingBoxBuffer(BoundingBoxBuffer&&)            = default;
    BoundingBoxBuffer& operator=(BoundingBoxBuffer&&) = default;
};

struct TransformBuffers
{
    std::vector<DynamicBufferAllocator> buffers;
    TransformBuffers()
    {
        buffers.reserve(NUM_FRAMES_IN_FLIGHT);
    }

    ~TransformBuffers() = default;

    TransformBuffers(const TransformBuffers&)            = delete;
    TransformBuffers& operator=(const TransformBuffers&) = delete;

    TransformBuffers(TransformBuffers&&)            = default;
    TransformBuffers& operator=(TransformBuffers&&) = default;
};

struct PBREnvironment
{
    Image irradianceMap;
    Image prefilteredEnvMap;
    Image BRDFLUT;

    PBREnvironment(Image&& irradianceMap, Image&& prefilteredEnvMap, Image&& BRDFLUT)
        : irradianceMap(std::move(irradianceMap)), prefilteredEnvMap(std::move(prefilteredEnvMap)), BRDFLUT(std::move(BRDFLUT))
    {
    }

    ~PBREnvironment() = default;

    PBREnvironment(const PBREnvironment&)            = delete;
    PBREnvironment& operator=(const PBREnvironment&) = delete;

    PBREnvironment(PBREnvironment&&)            = default;
    PBREnvironment& operator=(PBREnvironment&&) = default;
};

struct ShadowBuffers
{
    std::vector<DynamicBufferAllocator> matricesBuffers;
    std::vector<DynamicBufferAllocator> indicesBuffers;
    uint32_t numIndices = 0;

    ShadowBuffers()
    {
        matricesBuffers.reserve(NUM_FRAMES_IN_FLIGHT);
        indicesBuffers.reserve(NUM_FRAMES_IN_FLIGHT);
    }

    ~ShadowBuffers() = default;

    ShadowBuffers(const ShadowBuffers&)            = delete;
    ShadowBuffers& operator=(const ShadowBuffers&) = delete;

    ShadowBuffers(ShadowBuffers&&)            = default;
    ShadowBuffers& operator=(ShadowBuffers&&) = default;
};

struct LightBuffers
{
    std::vector<DynamicBufferAllocator> buffers;
    Buffer visibleLightsBuffer;  // this one is fixed max size
    uint32_t lightNum = 0;

    LightBuffers()
    {
        buffers.reserve(NUM_FRAMES_IN_FLIGHT);
    }

    ~LightBuffers() = default;

    LightBuffers(const LightBuffers&)            = delete;
    LightBuffers& operator=(const LightBuffers&) = delete;

    LightBuffers(LightBuffers&&)            = default;
    LightBuffers& operator=(LightBuffers&&) = default;
};
