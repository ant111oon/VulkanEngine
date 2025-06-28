#include "pch.h"

#include "vk_pipelines.h"

namespace vkutil
{
    bool LoadShaderModule(const std::filesystem::path& filepath, VkDevice pDevice, VkShaderModule& pOutModule) noexcept
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            return false;
        }

        const size_t fileSize = (size_t)file.tellg();

        std::vector<uint32_t> buffer((fileSize + sizeof(uint32_t) - 1) / sizeof(uint32_t));

        file.seekg(0);
        file.read((char*)buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo = {};
        
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.codeSize = buffer.size() * sizeof(uint32_t);
        createInfo.pCode = buffer.data();

        VkShaderModule pShaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(pDevice, &createInfo, nullptr, &pShaderModule) != VK_SUCCESS) {
            return false;
        }

        pOutModule = pShaderModule;
        
        return true;
    }
}

