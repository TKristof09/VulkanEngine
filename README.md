# VulkanEngine

A basic renderer using Vulkan and GLSL. This project's main goal is for me to be able to learn and play around with implementing various graphics techniques.

## Implemented Features
- PBR using the Cook-Torrance BRDF
- IBL
- Soft PCF shadows for directional lights
- A basic render graph to make adding renderpasses easier and to automatically add the needed synchronization inspired by [FrameGraph in Frostbite](https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in) and [this](https://themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/) blog post by Maister.
- Bindless rendering
- ECS using Flecs
- GTAO based on the [presentation](https://research.activision.com/publications/archives/practical-real-time-strategies-for-accurate-indirect-occlusion) and [paper](https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf) by Activision Blizzard
- Shader recompiling and reloading during runtime using [shaderc](https://github.com/google/shaderc)
- Frustum culling using a compute shader and `DrawIndexedIndirectCount`

  ![Screenshot_2](https://github.com/TKristof09/VulkanEngine/assets/25688981/4a02c935-921f-430f-a8dc-2acdc15616e3)
  ![Screenshot_4](https://github.com/user-attachments/assets/e4af5a1a-09fc-49e9-a444-709815228042)
