#include "pch.h"

#include "vk_initializers.h"


namespace vkinit
{
    VkCommandPoolCreateInfo CmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) noexcept
    {
        VkCommandPoolCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = flags,
            .queueFamilyIndex = queueFamilyIndex,
        };
        
        return info;
    }
    
    
    VkCommandBufferAllocateInfo CmdBufferAllocateInfo(VkCommandPool pVkCmdPool, uint32_t count) noexcept
    {
        VkCommandBufferAllocateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pVkCmdPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = count,
        };
        
        return info;
    }
    
    
    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags) noexcept
    {
        VkFenceCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = flags,
        };

        return info;
    }
    
    
    VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags) noexcept
    {
        VkSemaphoreCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .flags = flags,
        };

        return info;
    }
    
    
    VkCommandBufferBeginInfo CmdBufferBeginInfo(VkCommandBufferUsageFlags flags) noexcept
    {
        VkCommandBufferBeginInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = flags,
        };

        return info;
    }
    
    
    VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask) noexcept
    {
        VkImageSubresourceRange subImage = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };

        return subImage;
    }


    VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore pSemaphore) noexcept
    {
        VkSemaphoreSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = pSemaphore,
            .value = 1,
            .stageMask = stageMask,
            .deviceIndex = 0,
        };

        return submitInfo;
    }


    VkCommandBufferSubmitInfo CmdBufferSubmitInfo(VkCommandBuffer pCmdBuf) noexcept
    {
        VkCommandBufferSubmitInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = pCmdBuf,
            .deviceMask = 0,
        };

        return info;
    }


    VkSubmitInfo2 SubmitInfo2(VkCommandBufferSubmitInfo* pCmdBufSubmitInfo, VkSemaphoreSubmitInfo* pSignalSemaphoreInfo,
        VkSemaphoreSubmitInfo* pWaitSemaphoreInfo) noexcept
    {
        VkSubmitInfo2 info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = pWaitSemaphoreInfo == nullptr ? UINT32_C(0) : UINT32_C(1),
            .pWaitSemaphoreInfos = pWaitSemaphoreInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = pCmdBufSubmitInfo,
            .signalSemaphoreInfoCount = pSignalSemaphoreInfo == nullptr ? UINT32_C(0) : UINT32_C(1),
            .pSignalSemaphoreInfos = pSignalSemaphoreInfo,
        };

        return info;
    }
    
    
    VkImageCreateInfo ImageCreateInfo(const VkExtent3D& extent, VkFormat format, VkImageUsageFlags usageFlags) noexcept
    {
        VkImageCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usageFlags,
        };

        return info;
    }
    
    
    VkImageViewCreateInfo ImageViewCreateInfo(VkImage pImage, VkFormat format, VkImageAspectFlags aspectFlags) noexcept
    {
        VkImageViewCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = pImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
        };

        info.subresourceRange.aspectMask = aspectFlags;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.baseMipLevel = 0;

        return info;
    }


    VkRenderingAttachmentInfo RenderingAttachmentInfo(VkImageView pImageView, const std::optional<VkClearValue>& clearValue, VkImageLayout layout) noexcept
    {
        VkRenderingAttachmentInfo info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = pImageView,
            .imageLayout = layout,
            .loadOp = clearValue.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearValue.value_or({0}),
        };

        return info;
    }

    
    VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView pImageView, VkImageLayout layout) noexcept
    {
        VkRenderingAttachmentInfo depthAttachment = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depthAttachment.imageView = pImageView;
        depthAttachment.imageLayout = layout;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil.depth = 0.f;

        return depthAttachment;
    }


    VkRenderingInfo RenderingInfo(const VkExtent2D& extent, const VkRenderingAttachmentInfo* pColorAttachment, const VkRenderingAttachmentInfo* pDepthAttachment) noexcept
    {
        VkRenderingInfo info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = VkRect2D { VkOffset2D { 0, 0 }, extent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = pColorAttachment,
            .pDepthAttachment = pDepthAttachment,
        };

        return info;
    }


    VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule pShaderModule, const char* pEntry) noexcept
    {
        VkPipelineShaderStageCreateInfo info = {};
        
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = stage;
        info.module = pShaderModule;
        info.pName = pEntry;

        return info;
    }
    
    
    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo() noexcept
    {
        VkPipelineLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        
        return info;
    }
}
