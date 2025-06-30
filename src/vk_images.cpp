#include "pch.h"

#include "vk_images.h"
#include "vk_initializers.h"


namespace vkutil
{
    void TransitImage(VkCommandBuffer pCmdBuf, VkImage pImage, VkImageLayout currentLayout, VkImageLayout newLayout) noexcept
    {
        VkImageMemoryBarrier2 imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange = vkinit::ImageSubresourceRange(aspectMask);
        imageBarrier.image = pImage;

        VkDependencyInfo depInfo = {};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(pCmdBuf, &depInfo);
    }
    
    
    void CopyImage(VkCommandBuffer pCmdBuf, VkImage pSrcImage, const VkExtent2D& srcExtent, VkImage pDstImage, const VkExtent2D& dstExtent) noexcept
    {
        VkImageBlit2 blitRegion = {};
        
        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        
        blitRegion.srcOffsets[1].x = srcExtent.width;
        blitRegion.srcOffsets[1].y = srcExtent.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = dstExtent.width;
        blitRegion.dstOffsets[1].y = dstExtent.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo = {};

        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        
        blitInfo.srcImage = pSrcImage;
        blitInfo.dstImage = pDstImage;

        blitInfo.pRegions = &blitRegion;
        blitInfo.regionCount = 1;

        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(pCmdBuf, &blitInfo);
    }
}
