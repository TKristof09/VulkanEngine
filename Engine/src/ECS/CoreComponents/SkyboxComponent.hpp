#pragma once

#include "Rendering/Image.hpp"
struct SkyboxComponent
{
    Image skybox;

    template<typename... Args,
             std::enable_if_t<std::is_constructible<Image, Args...>::value, int> = 0>
    SkyboxComponent(Args&&... args)
        : skybox(std::forward<Args>(args)...)
    {
    }

    SkyboxComponent(const SkyboxComponent&)            = delete;
    SkyboxComponent& operator=(const SkyboxComponent&) = delete;

    SkyboxComponent(SkyboxComponent&&)            = default;
    SkyboxComponent& operator=(SkyboxComponent&&) = default;
};
