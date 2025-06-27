#include "pch.h"

#include "vk_initializers.h"


namespace vkinit
{
    VkCommandPoolCreateInfo CmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) noexcept
    {
        VkCommandPoolCreateInfo info = {};
        
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.pNext = nullptr;
        info.queueFamilyIndex = queueFamilyIndex;
        info.flags = flags;
        
        return info;
    }
    
    
    VkCommandBufferAllocateInfo CmdBufferAllocateInfo(VkCommandPool pVkCmdPool, uint32_t count) noexcept
    {
        VkCommandBufferAllocateInfo info = {};
        
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.pNext = nullptr;
        info.commandPool = pVkCmdPool;
        info.commandBufferCount = count;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        
        return info;
    }
    
    
    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags) noexcept
    {
        VkFenceCreateInfo info = {};
        
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = flags;

        return info;
    }
    
    
    VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags) noexcept
    {
        VkSemaphoreCreateInfo info = {};

        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = flags;

        return info;
    }
    
    
    VkCommandBufferBeginInfo CmdBufferBeginInfo(VkCommandBufferUsageFlags flags) noexcept
    {
        VkCommandBufferBeginInfo info = {};

        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.pNext = nullptr;
        info.pInheritanceInfo = nullptr;
        info.flags = flags;

        return info;
    }
    
    
    VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask) noexcept
    {
        VkImageSubresourceRange subImage = {};

        subImage.aspectMask = aspectMask;
        subImage.baseMipLevel = 0;
        subImage.levelCount = VK_REMAINING_MIP_LEVELS;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

        return subImage;
    }


    VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore pSemaphore) noexcept
    {
        VkSemaphoreSubmitInfo submitInfo = {};

        submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.semaphore = pSemaphore;
        submitInfo.stageMask = stageMask;
        submitInfo.deviceIndex = 0;
        submitInfo.value = 1;

        return submitInfo;
    }


    VkCommandBufferSubmitInfo CmdBufferSubmitInfo(VkCommandBuffer pCmdBuf) noexcept
    {
        VkCommandBufferSubmitInfo info = {};

        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        info.pNext = nullptr;
        info.commandBuffer = pCmdBuf;
        info.deviceMask = 0;

        return info;
    }


    VkSubmitInfo2 SubmitInfo2(VkCommandBufferSubmitInfo* pCmdBufSubmitInfo, VkSemaphoreSubmitInfo* pSignalSemaphoreInfo,
        VkSemaphoreSubmitInfo* pWaitSemaphoreInfo) noexcept
    {
        VkSubmitInfo2 info = {};

        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        info.pNext = nullptr;
        info.waitSemaphoreInfoCount = pWaitSemaphoreInfo == nullptr ? 0 : 1;
        info.pWaitSemaphoreInfos = pWaitSemaphoreInfo;
        info.signalSemaphoreInfoCount = pSignalSemaphoreInfo == nullptr ? 0 : 1;
        info.pSignalSemaphoreInfos = pSignalSemaphoreInfo;
        info.commandBufferInfoCount = 1;
        info.pCommandBufferInfos = pCmdBufSubmitInfo;

        return info;
    }
    
    
    VkImageCreateInfo ImageCreateInfo(const VkExtent3D& extent, VkFormat format, VkImageUsageFlags usageFlags) noexcept
    {
        VkImageCreateInfo info = {};

        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.pNext = nullptr;

        info.imageType = VK_IMAGE_TYPE_2D;

        info.extent = extent;
        info.format = format;
        info.usage = usageFlags;

        info.mipLevels = 1;
        info.arrayLayers = 1;

        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;

        return info;
    }
    
    
    VkImageViewCreateInfo ImageViewCreateInfo(VkImage pImage, VkFormat format, VkImageAspectFlags aspectFlags) noexcept
    {
        VkImageViewCreateInfo info = {};

        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = nullptr;

        info.viewType = VK_IMAGE_VIEW_TYPE_2D;

        info.image = pImage;
        info.format = format;

        info.subresourceRange.aspectMask = aspectFlags;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.baseMipLevel = 0;

        return info;
    }
}
