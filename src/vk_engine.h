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

private:
    struct SDL_Window* m_pWindow = nullptr;

	VkExtent2D m_windowExtent = { 900 , 640 };
	uint64_t m_frameNumber = 0;
    bool m_isInitialized = false;
	bool m_needRender = true;
};