#pragma once

#include "vk_types.h"

#include <vector>


class DescriptorLayoutBuilder final
{
public:
    DescriptorLayoutBuilder() = default;

    void AddBinding(uint32_t binding, VkDescriptorType type) noexcept;
    void Clear() noexcept;

    VkDescriptorSetLayout Build(VkDevice pDevice, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0) noexcept;

private:
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};


class DescriptorAllocator final
{
public:
    struct PoolSizeRatio final
    {
		VkDescriptorType type;
		float ratio;
    };

public:
    DescriptorAllocator() = default;

    void InitPool(VkDevice pDevice, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) noexcept;
    void ClearDescriptors(VkDevice pDevice) noexcept;
    void DestroyPool(VkDevice pDevice) noexcept;

    VkDescriptorSet Allocate(VkDevice pDevice, VkDescriptorSetLayout pLayout);

private:
    VkDescriptorPool m_pPool;
};
