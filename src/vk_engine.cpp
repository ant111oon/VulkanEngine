#include "core.h"

#include "vk_engine.h"

#include "vk_initializers.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <VkBootstrap.h>


#define ENG_CHECK_SDL_ERROR(COND, ...)                      \
    if (!(COND)) {                                          \
        ENG_ASSERT_FAIL("[SDL ERROR]: {}", SDL_GetError()); \
        return __VA_ARGS__;                                 \
    }


#ifdef ENG_DEBUG
    constexpr bool cfg_UseValidationLayers = true;
#else
    constexpr bool cfg_UseValidationLayers = false;
#endif


VulkanEngine& VulkanEngine::GetInstance() noexcept
{
    static VulkanEngine engine;
    return engine;
}


void VulkanEngine::Init() noexcept
{
    if (m_isInitialized) {
        return;
    }

    ENG_CHECK_SDL_ERROR(SDL_Init(SDL_INIT_VIDEO) == 0);

    const SDL_WindowFlags windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN);

    m_pWindow = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        m_windowExtent.width, m_windowExtent.height, windowFlags);
    ENG_CHECK_SDL_ERROR(m_pWindow);

    if (!InitVulkan()) {
        ENG_ASSERT_FAIL("Failed to init Vulkan");
        return;
    }

    if (!InitSwapChain()) {
        ENG_ASSERT_FAIL("Failed to init swapchain");
        return;
    }

    if (!InitCommands()) {
        ENG_ASSERT_FAIL("Failed to init commands");
        return;
    }

    if (!InitSyncStructures()) {
        ENG_ASSERT_FAIL("Failed to init sync structures");
        return;
    }

    m_isInitialized = true;
}


void VulkanEngine::Terminate() noexcept
{
    if (!m_isInitialized) {
        return;
    }

    vkDeviceWaitIdle(m_pVkDevice);

    for (size_t i = 0; i < FRAMES_DATA_INST_COUNT; ++i) {
        vkDestroyCommandPool(m_pVkDevice, m_framesData[i].pVkCmdPool, nullptr);
    }

    DestroySwapChain();

    vkDestroySurfaceKHR(m_pVkInstance, m_pVkSurface, nullptr);
	vkDestroyDevice(m_pVkDevice, nullptr);
		
	vkb::destroy_debug_utils_messenger(m_pVkInstance, m_pVkDgbMessenger);
	vkDestroyInstance(m_pVkInstance, nullptr);

    SDL_DestroyWindow(m_pWindow);

    m_isInitialized = false;
}


void VulkanEngine::Run() noexcept
{
    ENG_ASSERT(IsInitialized());

    bool isQuit = false;
    
    SDL_Event event;
    while (!isQuit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    isQuit = true;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                        m_needRender = false;
                    } else if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
                        m_needRender = true;
                    }
                    break;

                default:
                    break;
            }
        }

        if (m_needRender) {
            Render();
        } else {
            // do not draw if window is minimized
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
    }
}


VulkanEngine::~VulkanEngine()
{
    Terminate();
}


void VulkanEngine::Render() noexcept
{

}


bool VulkanEngine::InitVulkan() noexcept
{
    vkb::InstanceBuilder instBuilder;

    vkb::Result<vkb::Instance> instBuildResult = instBuilder.set_app_name("Example Vulkan Application")
		.request_validation_layers(cfg_UseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

    if (!instBuildResult.has_value()) {
        ENG_ASSERT_FAIL("Failed to create Vulkan instance");
        return false;
    }

    vkb::Instance& vkbInst = instBuildResult.value();

    m_pVkInstance = vkbInst.instance;
    m_pVkDgbMessenger = vkbInst.debug_messenger;

    if (SDL_Vulkan_CreateSurface(m_pWindow, m_pVkInstance, &m_pVkSurface) == SDL_FALSE) {
        ENG_ASSERT_FAIL("SDL failed to create surface");
        return false;
    }

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    vkb::PhysicalDeviceSelector vkbPhysDeviceSelector(vkbInst);
    vkb::Result<vkb::PhysicalDevice> vkbPhysDeviceSelectionResult = vkbPhysDeviceSelector
        .set_minimum_version(1, 3)
        .set_required_features_12(features12)
        .set_required_features_13(features13)
        .set_surface(m_pVkSurface)
        .select();

    if (!vkbPhysDeviceSelectionResult.has_value()) {
        ENG_ASSERT_FAIL("Failed to select physical device");
        return false;
    }

    vkb::PhysicalDevice& vkbPhysDevice = vkbPhysDeviceSelectionResult.value();

    vkb::DeviceBuilder vkbDeviceBuilder(vkbPhysDevice);
    vkb::Result<vkb::Device> vkbDeviceBuildResult = vkbDeviceBuilder.build();

    if (!vkbDeviceBuildResult.has_value()) {
        ENG_ASSERT_FAIL("Failed to build device");
        return false;
    }

    vkb::Device& vkbDevice = vkbDeviceBuildResult.value();

    m_pVkDevice = vkbDevice.device;
    m_pVkPhysDevice = vkbDevice.physical_device;

    m_pVkGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    return true;
}


bool VulkanEngine::InitSwapChain() noexcept
{
    return CreateSwapChain(m_windowExtent.width, m_windowExtent.height);
}


bool VulkanEngine::CreateSwapChain(uint32_t width, uint32_t height) noexcept
{
    vkb::SwapchainBuilder vkbSwapChainBuilder(m_pVkPhysDevice, m_pVkDevice, m_pVkSurface);

	m_vkSwapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkSurfaceFormatKHR surfaceFormat = {};
    surfaceFormat.format = m_vkSwapChainImageFormat;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	vkb::Result<vkb::Swapchain> vkbSwapChainBuildResult = vkbSwapChainBuilder
		.set_desired_format(surfaceFormat)
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build();

    if (!vkbSwapChainBuildResult.has_value()) {
        ENG_ASSERT_FAIL("Failed to build swapchain");
        return false;
    }

    vkb::Swapchain& vkbSwapChain = vkbSwapChainBuildResult.value();

	m_vkSwapChainExtent = vkbSwapChain.extent;
    
	m_pVkSwapChain = vkbSwapChain.swapchain;
	m_vkSwapChainImages = vkbSwapChain.get_images().value();
	m_vkSwapChainImageViews = vkbSwapChain.get_image_views().value();

    return true;
}


void VulkanEngine::DestroySwapChain() noexcept
{
    vkDestroySwapchainKHR(m_pVkDevice, m_pVkSwapChain, nullptr);

	for (size_t i = 0; i < m_vkSwapChainImageViews.size(); ++i) {
		vkDestroyImageView(m_pVkDevice, m_vkSwapChainImageViews[i], nullptr);
	}
}


bool VulkanEngine::InitCommands() noexcept
{
    const VkCommandPoolCreateInfo cmdPoolCreateInfo = vkinit::CmdPoolCreateInfo(m_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (size_t i = 0; i < FRAMES_DATA_INST_COUNT; ++i) {
        ENG_VK_CHECK(vkCreateCommandPool(m_pVkDevice, &cmdPoolCreateInfo, nullptr, &m_framesData[i].pVkCmdPool));

        const VkCommandBufferAllocateInfo cmdBufferAllocateInfo = vkinit::CmdBufferAllocateInfo(m_framesData[i].pVkCmdPool, 1);

        ENG_VK_CHECK(vkAllocateCommandBuffers(m_pVkDevice, &cmdBufferAllocateInfo, &m_framesData[i].pVkCmdBuffer));
    }

    return true;
}


bool VulkanEngine::InitSyncStructures() noexcept
{
    return true;
}
