#pragma once

#include "vk_types.h"


namespace vkutil
{
    void TransitImage(VkCommandBuffer pCmdBuf, VkImage pImage, VkImageLayout currentLayout, VkImageLayout newLayout) noexcept;
    void CopyImage(VkCommandBuffer pCmdBuf, VkImage pSrcImage, const VkExtent2D& srcExtent, VkImage pDstImage, const VkExtent2D& dstExtent, VkFilter filter = VK_FILTER_LINEAR) noexcept;

    void GenerateMipmaps(VkCommandBuffer pCmdBuf, VkImage pImage, VkExtent2D extent) noexcept;
}
