#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

#include "core.h"


struct ImageHandle
{
    VkImage pImage;
    VkImageView pImageView;
    VmaAllocation pAllocation;
    VkExtent3D extent;
    VkFormat format;
};


struct BufferHandle
{
    VkBuffer pBuffer;
    VmaAllocation pAllocation;
    VmaAllocationInfo allocationInfo;
};


struct Vertex
{
	glm::vec3 position;
	float uvX;
	glm::vec3 normal;
	float uvY;
	glm::vec4 color;
};


struct MeshGpuBuffers
{
    BufferHandle idxBuff;
    BufferHandle vertBuff;

    VkDeviceAddress vertBufferGpuAddress;
};


struct GPUDrawPushConstants
{
    glm::mat4 transform;
    VkDeviceAddress vertBufferGpuAddress;
};


enum class MaterialPass : uint8_t
{
    OPAQUE,
    TRANSPARENT,
    OTHER
};


struct MaterialPipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
};


struct SceneData
{
    glm::mat4 viewMat;
    glm::mat4 projMat;
    glm::mat4 viewProjMat;
    glm::vec4 ambientColor;
    glm::vec4 sunLightDirectionAndPower;
    glm::vec4 sunLightColor;
};


struct MaterialInstance
{
    MaterialPipeline* pPipeline;
    VkDescriptorSet descriptorSet;
    MaterialPass passType;
};


struct RenderContext;


class IRenderable {

    virtual void Render(const glm::mat4& topMatrix, RenderContext& ctx) = 0;
};

struct Node : public IRenderable
{
    void RefreshTransform(const glm::mat4& parentTrs)
    {
        worldTrs = parentTrs * localTrs;
        
        for (std::shared_ptr<Node>& pChild : childrens) {
            pChild->RefreshTransform(worldTrs);
        }
    }

    virtual void Render(const glm::mat4& topMatrix, RenderContext& ctx) override
    {
        for (std::shared_ptr<Node>& pChild : childrens) {
            pChild->Render(topMatrix, ctx);
        }
    }

    std::weak_ptr<Node> pParent;
    std::vector<std::shared_ptr<Node>> childrens;

    glm::mat4 localTrs;
    glm::mat4 worldTrs;
};


#ifdef ENG_DEBUG
    #define ENG_VK_CHECK(RESULT)                                                                        \
        do {                                                                                            \
            const VkResult _vkCheckRes = RESULT;                                                        \
            ENG_ASSERT_MSG(_vkCheckRes == VK_SUCCESS, "[VK ERROR]: {}", string_VkResult(_vkCheckRes));  \
        } while (0)
#else
    #define ENG_VK_CHECK(RESULT) (RESULT)
#endif
