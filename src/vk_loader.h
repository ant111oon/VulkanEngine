#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"

#include <unordered_map>
#include <filesystem>


struct GLTFMaterial
{
	MaterialInstance data;
};


struct GeoSurface
{
    std::shared_ptr<GLTFMaterial> material;
    uint32_t startIndex;
    uint32_t count;
};


struct MeshAsset
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    MeshGpuBuffers meshBuffers;
};


class VulkanEngine;


struct LoadedGLTF : public IRenderable
{
    ~LoadedGLTF() { ClearAll(); }

    void Render(const glm::mat4& topMatrix, RenderContext& ctx) override;

private:
    void ClearAll();

public:
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, ImageHandle> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;
    
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    BufferHandle materialDataBuffer;

    VulkanEngine* pCreator;
};


std::optional<std::shared_ptr<LoadedGLTF>> LoadGLTF(VulkanEngine* pEngine, const std::filesystem::path& filepath);