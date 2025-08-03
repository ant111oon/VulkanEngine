#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"

#include "camera.h"

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


struct RenderObject
{
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;
    
    MaterialInstance* pMaterial;
    Bounds bounds;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};


struct RenderContext
{
	std::vector<RenderObject> opaqueSurfaces;
	std::vector<RenderObject> transparentSurfaces;
};


struct GLTFMetallic_Roughness
{
    struct MaterialResources;

    void BuildPipelines(VulkanEngine* pEngine);
	void ClearResources(VkDevice device);

	MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);

	struct MaterialConstants
    {
		glm::vec4 colorFactors;
		glm::vec4 metallicRoughnessFactors;
		glm::vec4 PADDING[14];
	};

	struct MaterialResources
    {
		ImageHandle colorImage;
		VkSampler colorSampler;
		ImageHandle metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

    MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout descSetLayout;

	DescriptorWriter descWriter;
};


struct MeshNode : public Node
{
	void Render(const glm::mat4& topMatrix, RenderContext& ctx) override;

	std::shared_ptr<MeshAsset> pMesh;
};


struct EngineStats
{
    float frameTime;
    float sceneUpdateTime;
    float meshRenderTime;
    int triangleCount;
    int drawCallCount;
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
        DescriptorAllocatorGrowable descriptorAllocator;
    };

    static constexpr size_t FRAMES_DATA_INST_COUNT = UINTMAX_C(2);

    struct ComputePushConstants
    {
        glm::vec4 data[4];
    };

    struct ComputeEffect
    {
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

    MeshGpuBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) const noexcept;

public:
    VulkanEngine() = default;
    ~VulkanEngine();

    VulkanEngine(const VulkanEngine& other) = delete;
    VulkanEngine(VulkanEngine&& other) = delete;
    VulkanEngine& operator=(const VulkanEngine& other) = delete;
    VulkanEngine& operator=(VulkanEngine&& other) = delete;

    void Render() noexcept;
    void RenderBackground(VkCommandBuffer pCmdBuf) noexcept;
    void RenderGeometry(VkCommandBuffer pCmdBuf) noexcept;
    void RenderDbgUI() noexcept;
    void RenderImGui(VkCommandBuffer pCmdBuf, VkImageView pTargetImageView) noexcept;

    bool InitVulkan() noexcept;

    bool InitSwapChain() noexcept;
    bool CreateSwapChain(uint32_t width, uint32_t height) noexcept;
    void ResizeSwapChain() noexcept;
    void DestroySwapChain() noexcept;

    bool InitCommands() noexcept;
    bool InitSyncStructures() noexcept;

    bool InitDescriptors() noexcept;

    bool InitPipelines() noexcept;
    bool InitBackgroundPipelines() noexcept;

    void InitDefaultData() noexcept;

    bool InitImGui() noexcept;
    void ImmediateSubmit(std::function<void(VkCommandBuffer pCmdBuf)>&& function) const noexcept;

    void UpdateScene();

    BufferHandle CreateBuffer(size_t size, VkBufferUsageFlags bufUsage, VmaMemoryUsage memUsage) const noexcept;
    void DestroyBuffer(BufferHandle& buffer) const noexcept;

    ImageHandle CreateImage(const VkExtent3D& extent, VkFormat format, VkImageUsageFlags usage, const void* pData = nullptr, bool mipmapped = false);
    void DestroyImage(ImageHandle& image);

    FrameData& GetCurrentFrameData() noexcept { return m_framesData[m_frameNumber % FRAMES_DATA_INST_COUNT]; }

public:
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
    ImageHandle m_depthImage;
    VkExtent2D m_rndExtent;
    float m_dynResScale = 1.f;
    VkFilter m_dynResCopyFilter = VK_FILTER_LINEAR;

    DescriptorAllocatorGrowable m_globalDescriptorAllocator;

	VkDescriptorSet m_pComputeBackgroundDescriptors = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_pComputeBackgroundDescriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pComputeBackgroundPipelineLayout = VK_NULL_HANDLE;

    // immediate submit structures
    VkFence m_pImmFence;
    VkCommandBuffer m_pImmCommandBuffer;
    VkCommandPool m_pImmCommandPool;

    VmaAllocator m_pVMA = VK_NULL_HANDLE;
    DeletionQueue m_mainDeletionQueue;

    std::vector<ComputeEffect> m_backgroundEffects;
    int32_t m_currBackgroundEffect = 0;

    SceneData m_sceneData;
    VkDescriptorSetLayout m_pSceneDataDescriptorLayout;

    RenderContext m_mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> m_loadedScenes;

    VkDescriptorSetLayout m_singleImageDescriptorLayout;

    MaterialInstance m_defaultData;
    GLTFMetallic_Roughness m_metalRoughMaterial;

    ImageHandle m_whiteImage;
	ImageHandle m_blackImage;
	ImageHandle m_greyImage;
	ImageHandle m_checkerboardImage;

	VkSampler m_nearestSampler;
    VkSampler m_linearSampler;

    Camera m_mainCamera;
    EngineStats m_stats;

	uint64_t m_frameNumber = 0;
    bool m_isInitialized = false;
    bool m_isFlyCameraMode = false;
	bool m_needRender = true;
	bool m_needResizeSwapChain = false;
};
