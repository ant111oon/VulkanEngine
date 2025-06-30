#include "pch.h"

#include "vk_descriptors.h"


void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type) noexcept
{
    VkDescriptorSetLayoutBinding bind = {};
    
    bind.binding = binding;
    bind.descriptorCount = 1;
    bind.descriptorType = type;

    m_bindings.push_back(bind);
}


void DescriptorLayoutBuilder::Clear() noexcept
{
    m_bindings.clear();
}


VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice pDevice, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags) noexcept
{
    for (VkDescriptorSetLayoutBinding& binding : m_bindings) {
        binding.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = pNext,
        .flags = flags,
        .bindingCount = static_cast<uint32_t>(m_bindings.size()),
        .pBindings = m_bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    ENG_VK_CHECK(vkCreateDescriptorSetLayout(pDevice, &info, nullptr, &layout));

    return layout;
}


void DescriptorAllocator::InitPool(VkDevice pDevice, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) noexcept
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(poolRatios.size());

    for (const PoolSizeRatio& ratio : poolRatios) {
        VkDescriptorPoolSize poolSize = {};
        
        poolSize.type = ratio.type;
        poolSize.descriptorCount = static_cast<uint32_t>(ratio.ratio * maxSets);

        poolSizes.emplace_back(poolSize);
    }

	VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

	vkCreateDescriptorPool(pDevice, &poolInfo, VK_NULL_HANDLE, &m_pPool);
}


void DescriptorAllocator::ClearDescriptors(VkDevice pDevice) noexcept
{
    vkResetDescriptorPool(pDevice, m_pPool, 0);
}


void DescriptorAllocator::DestroyPool(VkDevice pDevice) noexcept
{
    vkDestroyDescriptorPool(pDevice, m_pPool, VK_NULL_HANDLE);
}


VkDescriptorSet DescriptorAllocator::Allocate(VkDevice pDevice, VkDescriptorSetLayout pLayout)
{
    VkDescriptorSetAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_pPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &pLayout
    };

    VkDescriptorSet pSet = VK_NULL_HANDLE;
    ENG_VK_CHECK(vkAllocateDescriptorSets(pDevice, &info, &pSet));

    return pSet;
}
