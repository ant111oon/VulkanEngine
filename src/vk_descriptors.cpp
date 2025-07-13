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


void DescriptorAllocatorGrowable::Init(VkDevice pDevice, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) noexcept
{
    m_ratios.clear();
    
    for (const PoolSizeRatio& ratio : poolRatios) {
        m_ratios.push_back(ratio);
    }
	
    VkDescriptorPool newPool = CreatePool(pDevice, maxSets, poolRatios);

    m_setsPerPool = maxSets * 1.5;

    m_readyPools.push_back(newPool);
}


void DescriptorAllocatorGrowable::ClearPools(VkDevice pDevice) noexcept
{
    for (VkDescriptorPool& pool : m_readyPools) {
        vkResetDescriptorPool(pDevice, pool, 0);
    }

    for (VkDescriptorPool& pool : m_fullPools) {
        vkResetDescriptorPool(pDevice, pool, 0);
        m_readyPools.push_back(pool);
    }

    m_fullPools.clear();
}


void DescriptorAllocatorGrowable::DestroyPools(VkDevice pDevice) noexcept
{
    for (VkDescriptorPool& pool : m_readyPools) {
		vkDestroyDescriptorPool(pDevice, pool, nullptr);
	}

    m_readyPools.clear();
    
	for (VkDescriptorPool& pool : m_fullPools) {
		vkDestroyDescriptorPool(pDevice, pool, nullptr);
    }

    m_fullPools.clear();
}


VkDescriptorSet DescriptorAllocatorGrowable::Allocate(VkDevice pDevice, VkDescriptorSetLayout pLayout, void* pNext) noexcept
{
    VkDescriptorPool pPoolToUse = GetPool(pDevice);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = pNext;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pPoolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &pLayout;

	VkDescriptorSet ds;
	VkResult result = vkAllocateDescriptorSets(pDevice, &allocInfo, &ds);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        m_fullPools.push_back(pPoolToUse);
    
        pPoolToUse = GetPool(pDevice);
        allocInfo.descriptorPool = pPoolToUse;

        ENG_VK_CHECK(vkAllocateDescriptorSets(pDevice, &allocInfo, &ds));
    }
  
    m_readyPools.push_back(pPoolToUse);

    return ds;
}


VkDescriptorPool DescriptorAllocatorGrowable::GetPool(VkDevice pDevice) noexcept
{
    VkDescriptorPool pNewPool;
    if (!m_readyPools.empty()) {
        pNewPool = m_readyPools.back();
        m_readyPools.pop_back();
    } else {
	    pNewPool = CreatePool(pDevice, m_setsPerPool, m_ratios);
	    m_setsPerPool *= 1.5;
	    m_setsPerPool = m_setsPerPool <= 4092 ? m_setsPerPool : 4092;
    }   

    return pNewPool;
}


VkDescriptorPool DescriptorAllocatorGrowable::CreatePool(VkDevice pDevice, uint32_t setsCount, std::span<PoolSizeRatio> poolRatios) noexcept
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(poolRatios.size());

	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize {
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * setsCount)
		});
	}

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = setsCount;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(pDevice, &poolInfo, nullptr, &newPool);
    return newPool;
}


void DescriptorWriter::WriteImage(uint32_t binding, VkImageView pImage, VkSampler pSampler, VkImageLayout pLayout, VkDescriptorType type) noexcept
{
    VkDescriptorImageInfo& info = m_imageInfos.emplace_back(VkDescriptorImageInfo {
		.sampler = pSampler,
		.imageView = pImage,
		.imageLayout = pLayout
	});

	VkWriteDescriptorSet writeInfo = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	writeInfo.dstBinding = binding;
	writeInfo.dstSet = VK_NULL_HANDLE;
	writeInfo.descriptorCount = 1;
	writeInfo.descriptorType = type;
	writeInfo.pImageInfo = &info;

	m_writes.push_back(writeInfo);
}


void DescriptorWriter::WriteBuffer(uint32_t binding, VkBuffer pBuffer, size_t size, size_t offset, VkDescriptorType type) noexcept
{
    VkDescriptorBufferInfo& info = m_bufferInfos.emplace_back(VkDescriptorBufferInfo {
		.buffer = pBuffer,
		.offset = offset,
		.range = size
	});

	VkWriteDescriptorSet writeInfo = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

	writeInfo.dstBinding = binding;
	writeInfo.dstSet = VK_NULL_HANDLE;
	writeInfo.descriptorCount = 1;
	writeInfo.descriptorType = type;
	writeInfo.pBufferInfo = &info;

	m_writes.push_back(writeInfo);    
}


void DescriptorWriter::Clear() noexcept
{
    m_imageInfos.clear();
    m_writes.clear();
    m_bufferInfos.clear();
}


void DescriptorWriter::UpdateSet(VkDevice device, VkDescriptorSet set) noexcept
{
    for (VkWriteDescriptorSet& write : m_writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, m_writes.size(), m_writes.data(), 0, nullptr);
}
