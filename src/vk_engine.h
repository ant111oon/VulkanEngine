#pragma once

#include "vk.h"


class VulkanEngine final
{
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

    bool InitVulkan() noexcept;

    bool InitSwapChain() noexcept;
    bool CreateSwapChain(uint32_t width, uint32_t height) noexcept;
    void DestroySwapChain() noexcept;

    bool InitCommands() noexcept;
    bool InitSyncStructures() noexcept;

private:
    struct SDL_Window* m_pWindow = nullptr;
	VkExtent2D m_windowExtent = { 900 , 640 };

    VkInstance m_pVkInstance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_pVkDgbMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_pVkPhysDevice = VK_NULL_HANDLE;
    VkDevice m_pVkDevice = VK_NULL_HANDLE;
    VkSurfaceKHR m_pVkSurface = VK_NULL_HANDLE;
    
    VkSwapchainKHR m_pVkSwapChain = VK_NULL_HANDLE;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;

    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;

	uint64_t m_frameNumber = 0;
    bool m_isInitialized = false;
	bool m_needRender = true;
};
