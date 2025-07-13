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


class DescriptorAllocatorGrowable final
{
public:
    struct PoolSizeRatio final
    {
		VkDescriptorType type;
		float ratio;
    };

public:
    DescriptorAllocatorGrowable() = default;

    void Init(VkDevice pDevice, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) noexcept;
    void ClearPools(VkDevice pDevice) noexcept;
    void DestroyPools(VkDevice pDevice) noexcept;

    VkDescriptorSet Allocate(VkDevice pDevice, VkDescriptorSetLayout pLayout, void* pNext = nullptr) noexcept;

private:
    VkDescriptorPool GetPool(VkDevice pDevice) noexcept;
    VkDescriptorPool CreatePool(VkDevice pDevice, uint32_t setsCount, std::span<PoolSizeRatio> poolRatios) noexcept;

private:
    std::vector<PoolSizeRatio> m_ratios;

    std::vector<VkDescriptorPool> m_fullPools;
    std::vector<VkDescriptorPool> m_readyPools;
    
    uint32_t m_setsPerPool;
};


class DescriptorWriter final
{
public:
    void WriteImage(uint32_t binding, VkImageView pImage, VkSampler pSampler, VkImageLayout pLayout, VkDescriptorType type) noexcept;
    void WriteBuffer(uint32_t binding, VkBuffer pBuffer, size_t size, size_t offset, VkDescriptorType type) noexcept;

    void Clear() noexcept;
    void UpdateSet(VkDevice device, VkDescriptorSet set) noexcept;

private:
    std::deque<VkDescriptorImageInfo> m_imageInfos;
    std::deque<VkDescriptorBufferInfo> m_bufferInfos;
    std::vector<VkWriteDescriptorSet> m_writes;
};