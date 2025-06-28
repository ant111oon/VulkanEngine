#pragma once

#include "vk_types.h"

#include <filesystem>


namespace vkutil
{
    bool LoadShaderModule(const std::filesystem::path& filepath, VkDevice pDevice, VkShaderModule& pOutModule) noexcept;
}