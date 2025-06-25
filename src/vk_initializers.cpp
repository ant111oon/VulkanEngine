#include "pch.h"

#include "vk_initializers.h"


namespace vkinit
{
    VkCommandPoolCreateInfo CmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
    {
        VkCommandPoolCreateInfo info = {};
        
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.pNext = nullptr;
        info.queueFamilyIndex = queueFamilyIndex;
        info.flags = flags;
        
        return info;
    }
    
    
    VkCommandBufferAllocateInfo CmdBufferAllocateInfo(VkCommandPool pVkCmdPool, uint32_t count)
    {
        VkCommandBufferAllocateInfo info = {};
        
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.pNext = nullptr;
        info.commandPool = pVkCmdPool;
        info.commandBufferCount = count;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        
        return info;
    }
}
