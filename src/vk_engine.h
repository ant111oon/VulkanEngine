#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"

#include <array>
#include <deque>

#include <functional>

#include <cstdint>


class DeletionQueue final
{
public:
    DeletionQueue() = default;

	void PushDeletor(std::function<void()>&& deletor) noexcept
    {
		m_deletors.emplace_back(std::forward<std::function<void()>>(deletor));
	}

	void Flush() noexcept
    {
		for (auto it = m_deletors.rbegin(); it != m_deletors.rend(); ++it) {
			(*it)();
		}

		m_deletors.clear();
	}

private:
    std::deque<std::function<void()>> m_deletors;
};


class VulkanEngine final
{
private:
    struct FrameData
    {
        VkCommandPool pVkCmdPool;
        VkCommandBuffer pVkCmdBuffer;

        VkSemaphore pVkSwapChainSemaphore;
        VkSemaphore pVkRenderSemaphore;
	    VkFence pVkRenderFence;

        DeletionQueue deletionQueue;
    };

    static constexpr size_t FRAMES_DATA_INST_COUNT = UINTMAX_C(2);

    struct ComputePushConstants
    {
        glm::vec4 data[4];
    };

    struct ComputeEffect {
        std::string_view name;

        VkPipeline pipeline;
        VkPipelineLayout layout;

        ComputePushConstants data;
    };

public:
    static VulkanEngine& GetInstance() noexcept;

public:
    void Init() noexcept;
    void Terminate() noexcept;
    void Run() noexcept;

    bool IsInitialized() const noexcept { return m_isInitialized; }

private:
    VulkanEngine() = default;
    ~VulkanEngine();

    VulkanEngine(const VulkanEngine& other) = delete;
    VulkanEngine(VulkanEngine&& other) = delete;
    VulkanEngine& operator=(const VulkanEngine& other) = delete;
    VulkanEngine& operator=(VulkanEngine&& other) = delete;

    void Render() noexcept;
    void RenderBackground(VkCommandBuffer pCmdBuf) noexcept;
    void RenderGeometry(VkCommandBuffer pCmdBuf) noexcept;
    void RenderImGui(VkCommandBuffer pCmdBuf, VkImageView pTargetImageView) noexcept;

    bool InitVulkan() noexcept;

    bool InitSwapChain() noexcept;
    bool CreateSwapChain(uint32_t width, uint32_t height) noexcept;
    void DestroySwapChain() noexcept;

    bool InitCommands() noexcept;
    bool InitSyncStructures() noexcept;

    bool InitDescriptors() noexcept;

    bool InitPipelines() noexcept;
    bool InitBackgroundPipelines() noexcept;
    bool InitTrianglePipeline() noexcept;
    bool InitMeshPipeline() noexcept;

    void InitDefaultData() noexcept;

    bool InitImGui() noexcept;
    void ImmediateSubmit(std::function<void(VkCommandBuffer pCmdBuf)>&& function) const noexcept;

    BufferHandle AllocateBuffer(size_t size, VkBufferUsageFlags bufUsage, VmaMemoryUsage memUsage) const noexcept;
    void DestroyBuffer(BufferHandle& buffer) const noexcept;

    MeshGpuBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) const noexcept;

    FrameData& GetCurrentFrameData() noexcept { return m_framesData[m_frameNumber % FRAMES_DATA_INST_COUNT]; }

private:
    struct SDL_Window* m_pWindow = nullptr;
	VkExtent2D m_windowExtent = { 1000 , 720 };

    VkInstance m_pVkInstance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_pVkDgbMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_pVkPhysDevice = VK_NULL_HANDLE;
    VkDevice m_pVkDevice = VK_NULL_HANDLE;
    VkSurfaceKHR m_pVkSurface = VK_NULL_HANDLE;
    
    VkSwapchainKHR m_pVkSwapChain = VK_NULL_HANDLE;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;

    std::vector<VkImage> m_vkSwapChainImages;
    std::vector<VkImageView> m_vkSwapChainImageViews;

    std::array<FrameData, FRAMES_DATA_INST_COUNT> m_framesData;

    VkQueue m_pVkGraphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily;

    ImageHandle m_rndImage;
    VkExtent2D m_rndExtent;

    DescriptorAllocator m_globalDescriptorAllocator;

	VkDescriptorSet m_pRndImageDescriptors = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_pRndImageDescriptorLayout = VK_NULL_HANDLE;

    VkPipelineLayout m_pGradientPipelineLayout = VK_NULL_HANDLE;

    VkPipelineLayout m_pTrianglePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pTrianglePipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_pMeshPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pMeshPipeline = VK_NULL_HANDLE;

    MeshGpuBuffers m_rectangle;

    // immediate submit structures
    VkFence m_pImmFence;
    VkCommandBuffer m_pImmCommandBuffer;
    VkCommandPool m_pImmCommandPool;

    VmaAllocator m_pVMA = VK_NULL_HANDLE;
    DeletionQueue m_mainDeletionQueue;

    std::vector<ComputeEffect> m_backgroundEffects;
    int32_t m_currBackgroundEffect = 0;

	uint64_t m_frameNumber = 0;
    bool m_isInitialized = false;
	bool m_needRender = true;
};
