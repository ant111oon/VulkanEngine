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


struct MeshDrawPushConstants
{
    glm::mat4 transform;
    VkDeviceAddress vertBufferGpuAddress;
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
