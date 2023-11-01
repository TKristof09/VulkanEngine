// bindings that are used in every shader
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
layout (set = 0, binding = 0) uniform sampler2D textures[];
//layout (set = 0, binding = 1) uniform image2D storageTextures[];
layout(buffer_reference, buffer_reference_align=4) readonly buffer Transforms {
    mat4 m[];
};
// TODO look at alignment
layout(buffer_reference, buffer_reference_align=4) readonly buffer ShaderData;
layout(buffer_reference, buffer_reference_align=4) readonly buffer MaterialData;
layout(push_constant) uniform PC
{
    mat4 viewProj;
    vec3 cameraPos;
    int debugMode;

    ShaderData shaderDataPtr; // we leave this as uint64_t because each shader declares their own ShaderData struct, and some shaders might not want to use any ShaderData
    Transforms transformsPtr; // accessed with objectId
    uint64_t drawIdToObjectIdPtr; // used to access transforms, converts glDrawId to objectId
    MaterialData materialsPtr; // accessed with glDrawId
};
