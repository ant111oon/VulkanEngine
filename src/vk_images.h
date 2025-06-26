#pragma once

#include "vk.h"


namespace vkutil
{
    void TransitImage(VkCommandBuffer pCmdBug, VkImage pImage, VkImageLayout currentLayout, VkImageLayout newLayout) noexcept;
}
