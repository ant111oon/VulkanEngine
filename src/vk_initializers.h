#pragma once

#include "vk_types.h"

#include <cstdint>


namespace vkinit
{
    VkCommandPoolCreateInfo CmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) noexcept;
    VkCommandBufferAllocateInfo CmdBufferAllocateInfo(VkCommandPool pVkCmdPool, uint32_t count = UINT32_C(0)) noexcept;

    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0) noexcept;
    VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0) noexcept;

    VkCommandBufferBeginInfo CmdBufferBeginInfo(VkCommandBufferUsageFlags flags = 0) noexcept;

    VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask) noexcept;

    VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore pSemaphore) noexcept;

    VkCommandBufferSubmitInfo CmdBufferSubmitInfo(VkCommandBuffer pCmdBuf) noexcept;

    VkSubmitInfo2 SubmitInfo2(VkCommandBufferSubmitInfo* pCmdBufSubmitInfo, VkSemaphoreSubmitInfo* pSignalSemaphoreInfo,
        VkSemaphoreSubmitInfo* pWaitSemaphoreInfo) noexcept;

    VkImageCreateInfo ImageCreateInfo(const VkExtent3D& extent, VkFormat format, VkImageUsageFlags usageFlags) noexcept;
    VkImageViewCreateInfo ImageViewCreateInfo(VkImage pImage, VkFormat format, VkImageAspectFlags aspectFlags) noexcept;

    VkRenderingAttachmentInfo RenderingAttachmentInfo(VkImageView pImageView, const std::optional<VkClearValue>& clearValue, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) noexcept;
    VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView pImageView, VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) noexcept;
    
    VkRenderingInfo RenderingInfo(const VkExtent2D& extent, const VkRenderingAttachmentInfo* pColorAttachment, const VkRenderingAttachmentInfo* pDepthAttachment) noexcept;

    VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule pShaderModule, const char* pEntry = "main") noexcept;

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo() noexcept;
}
