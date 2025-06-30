#include "pch.h"

#include "core.h"

#include "vk_engine.h"

#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_images.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>


#ifdef ENG_DEBUG
    constexpr bool cfg_UseValidationLayers = true;
#else
    constexpr bool cfg_UseValidationLayers = false;
#endif

static const std::filesystem::path ENG_GRADIENT_COMPUTE_SHADER_PATH = "../shaders/bin/gradient.comp.spv";
static const std::filesystem::path ENG_SKY_SHADER_PATH = "../shaders/bin/sky.comp.spv";


#define ENG_RND_BACKGROUND_VERSION_CLEAR 0
#define ENG_RND_BACKGROUND_VERSION_COMPUTE_GRADIENT 1
#define ENG_RND_BACKGROUND_VERSION ENG_RND_BACKGROUND_VERSION_COMPUTE_GRADIENT


#define ENG_CHECK_SDL_ERROR(COND, ...)                      \
    if (!(COND)) {                                          \
        ENG_ASSERT_FAIL("[SDL ERROR]: {}", SDL_GetError()); \
        return __VA_ARGS__;                                 \
    }


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

    if (!InitDescriptors()) {
        ENG_ASSERT_FAIL("Failed to init descriptors");
        return;
    }

    if (!InitPipelines()) {
        ENG_ASSERT_FAIL("Failed to init pipelines");
        return;
    }

    if (!InitImGui()) {
        ENG_ASSERT_FAIL("Failed to init ImGui");
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

        vkDestroyFence(m_pVkDevice, m_framesData[i].pVkRenderFence, nullptr);
		vkDestroySemaphore(m_pVkDevice, m_framesData[i].pVkRenderSemaphore, nullptr);
		vkDestroySemaphore(m_pVkDevice ,m_framesData[i].pVkSwapChainSemaphore, nullptr);
    
        m_framesData[i].deletionQueue.Flush();
    }

    m_mainDeletionQueue.Flush();

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

            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        if (!m_needRender) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Background")) {
			
			ComputeEffect& selected = m_backgroundEffects[m_currBackgroundEffect];
		
			ImGui::Text("Selected effect: %s", selected.name.data());
		
			ImGui::SliderInt("Effect Index", &m_currBackgroundEffect, 0, m_backgroundEffects.size() - 1);
		
            for (size_t i = 0; i < std::size(selected.data.data); ++i) {
                glm::vec4& data = selected.data.data[i];
                
                char label[64] = {};
                sprintf_s(label, "data %u", i);

                ImGui::InputFloat4(label,(float*)&data.x);
            }
		}
		ImGui::End();

        ImGui::Render();

        Render();
    }
}


VulkanEngine::~VulkanEngine()
{
    Terminate();
}


void VulkanEngine::Render() noexcept
{
    FrameData& currFrameData = GetCurrentFrameData();

    constexpr uint64_t waitRenderFenceTimeoutNs = 1'000'000'000;

    ENG_VK_CHECK(vkWaitForFences(m_pVkDevice, 1, &currFrameData.pVkRenderFence, true, waitRenderFenceTimeoutNs));
	
    currFrameData.deletionQueue.Flush();

    constexpr uint64_t acquireNextSwapChainImageTimeoutNs = 1'000'000'000;

    uint32_t swapChainImageIndex;
    vkAcquireNextImageKHR(m_pVkDevice, m_pVkSwapChain, acquireNextSwapChainImageTimeoutNs,
        currFrameData.pVkSwapChainSemaphore, VK_NULL_HANDLE, &swapChainImageIndex);
        
    VkCommandBuffer pCmdBuf = currFrameData.pVkCmdBuffer;
        
    ENG_VK_CHECK(vkResetFences(m_pVkDevice, 1, &currFrameData.pVkRenderFence));
    ENG_VK_CHECK(vkResetCommandBuffer(pCmdBuf, 0));

    m_rndExtent.width = m_rndImage.extent.width;
	m_rndExtent.height = m_rndImage.extent.height;

    const VkCommandBufferBeginInfo cmdBuffBeginInfo = vkinit::CmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    ENG_VK_CHECK(vkBeginCommandBuffer(pCmdBuf, &cmdBuffBeginInfo));

    vkutil::TransitImage(pCmdBuf, m_rndImage.pImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    RenderBackground(pCmdBuf);

    vkutil::TransitImage(pCmdBuf, m_rndImage.pImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    
    vkutil::TransitImage(pCmdBuf, m_vkSwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    vkutil::CopyImage(pCmdBuf, m_rndImage.pImage, m_rndExtent, m_vkSwapChainImages[swapChainImageIndex], m_swapChainExtent);

    vkutil::TransitImage(pCmdBuf, m_vkSwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    RenderImGui(pCmdBuf, m_vkSwapChainImageViews[swapChainImageIndex]);

    vkutil::TransitImage(pCmdBuf, m_vkSwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    
	ENG_VK_CHECK(vkEndCommandBuffer(pCmdBuf));

    VkCommandBufferSubmitInfo cmdBufSubmitInfo = vkinit::CmdBufferSubmitInfo(pCmdBuf);	
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, currFrameData.pVkSwapChainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currFrameData.pVkRenderSemaphore);	
	
	VkSubmitInfo2 submitInfo2 = vkinit::SubmitInfo2(&cmdBufSubmitInfo, &signalInfo, &waitInfo);	

	ENG_VK_CHECK(vkQueueSubmit2(m_pVkGraphicsQueue, 1, &submitInfo2, currFrameData.pVkRenderFence));

    VkPresentInfoKHR presentInfo = {};

	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pSwapchains = &m_pVkSwapChain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &currFrameData.pVkRenderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapChainImageIndex;

	ENG_VK_CHECK(vkQueuePresentKHR(m_pVkGraphicsQueue, &presentInfo));

	++m_frameNumber;
}


void VulkanEngine::RenderBackground(VkCommandBuffer pCmdBuf) noexcept
{
#if ENG_RND_BACKGROUND_VERSION == ENG_RND_BACKGROUND_VERSION_CLEAR
    VkClearColorValue clearValue = {};
    clearValue.float32[0] = std::abs(std::cos(m_frameNumber / 30.f));
    clearValue.float32[1] = 0.f;
    clearValue.float32[2] = std::abs(std::sin(m_frameNumber / 60.f));
    clearValue.float32[3] = 1.f;

    VkImageSubresourceRange clearRange = vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(pCmdBuf, m_rndImage.pImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
#elif ENG_RND_BACKGROUND_VERSION == ENG_RND_BACKGROUND_VERSION_COMPUTE_GRADIENT
    ComputeEffect& effect = m_backgroundEffects[m_currBackgroundEffect];

    vkCmdBindPipeline(pCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    vkCmdBindDescriptorSets(pCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pGradientPipelineLayout, 0, 1, &m_pRndImageDescriptors, 0, nullptr);

    vkCmdPushConstants(pCmdBuf, m_pGradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    vkCmdDispatch(pCmdBuf, std::ceil(m_rndExtent.width / 16.f), std::ceil(m_rndExtent.height / 16.f), 1);
#else
    #error Invalid RenderBackground version
#endif
}


void VulkanEngine::RenderImGui(VkCommandBuffer pCmdBuf, VkImageView pTargetImageView) noexcept
{
    const VkRenderingAttachmentInfo colorAttachmentInfo = vkinit::RenderingAttachmentInfo(pTargetImageView, std::nullopt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderingInfo = vkinit::RenderingInfo(m_swapChainExtent, &colorAttachmentInfo, nullptr);

    vkCmdBeginRendering(pCmdBuf, &renderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), pCmdBuf);

	vkCmdEndRendering(pCmdBuf);
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

    VmaAllocatorCreateInfo vmaCreateInfo = {};
    vmaCreateInfo.physicalDevice = m_pVkPhysDevice;
    vmaCreateInfo.device = m_pVkDevice;
    vmaCreateInfo.instance = m_pVkInstance;
    vmaCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&vmaCreateInfo, &m_pVMA);

    m_mainDeletionQueue.PushDeletor([&]() {
        vmaDestroyAllocator(m_pVMA);
    });

    return true;
}


bool VulkanEngine::InitSwapChain() noexcept
{
    if (!CreateSwapChain(m_windowExtent.width, m_windowExtent.height)) {
        return false;
    }

    const VkExtent3D rndImageExtent = { m_windowExtent.width, m_windowExtent.height, 1 };

	m_rndImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_rndImage.extent = rndImageExtent;

	VkImageUsageFlags rndImageUsages{};
	rndImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	rndImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	rndImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	rndImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const VkImageCreateInfo rndImageCreateInfo = vkinit::ImageCreateInfo(m_rndImage.extent, m_rndImage.format, rndImageUsages);

	VmaAllocationCreateInfo rndImageAllocInfo = {};
	rndImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rndImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(m_pVMA, &rndImageCreateInfo, &rndImageAllocInfo, &m_rndImage.pImage, &m_rndImage.pAllocation, nullptr);

	const VkImageViewCreateInfo rndImageViewInfo = vkinit::ImageViewCreateInfo(m_rndImage.pImage, m_rndImage.format, VK_IMAGE_ASPECT_COLOR_BIT);

	ENG_VK_CHECK(vkCreateImageView(m_pVkDevice, &rndImageViewInfo, nullptr, &m_rndImage.pImageView));

    m_mainDeletionQueue.PushDeletor([&]() {
		vkDestroyImageView(m_pVkDevice, m_rndImage.pImageView, nullptr);
		vmaDestroyImage(m_pVMA, m_rndImage.pImage, m_rndImage.pAllocation);
	});

    return true;
}


bool VulkanEngine::CreateSwapChain(uint32_t width, uint32_t height) noexcept
{
    vkb::SwapchainBuilder vkbSwapChainBuilder(m_pVkPhysDevice, m_pVkDevice, m_pVkSurface);

	m_swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkSurfaceFormatKHR surfaceFormat = {};
    surfaceFormat.format = m_swapChainImageFormat;
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

	m_swapChainExtent = vkbSwapChain.extent;
    
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

    ENG_VK_CHECK(vkCreateCommandPool(m_pVkDevice, &cmdPoolCreateInfo, nullptr, &m_pImmCommandPool));

    const VkCommandBufferAllocateInfo immCmdBufferAllocateInfo = vkinit::CmdBufferAllocateInfo(m_pImmCommandPool, 1);

    ENG_VK_CHECK(vkAllocateCommandBuffers(m_pVkDevice, &immCmdBufferAllocateInfo, &m_pImmCommandBuffer));

    m_mainDeletionQueue.PushDeletor([&](){
        vkDestroyCommandPool(m_pVkDevice, m_pImmCommandPool, nullptr);
    });

    return true;
}


bool VulkanEngine::InitSyncStructures() noexcept
{
    const VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();

	for (size_t i = 0; i < FRAMES_DATA_INST_COUNT; ++i) {
		ENG_VK_CHECK(vkCreateFence(m_pVkDevice, &fenceCreateInfo, nullptr, &m_framesData[i].pVkRenderFence));

		ENG_VK_CHECK(vkCreateSemaphore(m_pVkDevice, &semaphoreCreateInfo, nullptr, &m_framesData[i].pVkSwapChainSemaphore));
		ENG_VK_CHECK(vkCreateSemaphore(m_pVkDevice, &semaphoreCreateInfo, nullptr, &m_framesData[i].pVkRenderSemaphore));
	}

    ENG_VK_CHECK(vkCreateFence(m_pVkDevice, &fenceCreateInfo, nullptr, &m_pImmFence));
    m_mainDeletionQueue.PushDeletor([&]() { vkDestroyFence(m_pVkDevice, m_pImmFence, nullptr); });

    return true;
}


bool VulkanEngine::InitDescriptors() noexcept
{
    std::array<DescriptorAllocator::PoolSizeRatio, 1> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f }
	};

	m_globalDescriptorAllocator.InitPool(m_pVkDevice, 10, sizes);

    static constexpr uint32_t IMAGE_BINDING_IDX = 0;
    static constexpr VkDescriptorType IMAGE_DESCR_TYPE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	DescriptorLayoutBuilder builder;
	builder.AddBinding(IMAGE_BINDING_IDX, IMAGE_DESCR_TYPE);
	m_pRndImageDescriptorLayout = builder.Build(m_pVkDevice, VK_SHADER_STAGE_COMPUTE_BIT);

    m_pRndImageDescriptors = m_globalDescriptorAllocator.Allocate(m_pVkDevice, m_pRndImageDescriptorLayout);	

	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo.imageView = m_rndImage.pImageView;
	
	VkWriteDescriptorSet rndImageWrite = {};
	
    rndImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;	
	rndImageWrite.dstBinding = IMAGE_BINDING_IDX;
	rndImageWrite.dstSet = m_pRndImageDescriptors;
	rndImageWrite.descriptorCount = 1;
	rndImageWrite.descriptorType = IMAGE_DESCR_TYPE;
	rndImageWrite.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(m_pVkDevice, 1, &rndImageWrite, 0, nullptr);

	m_mainDeletionQueue.PushDeletor([&]() {
		m_globalDescriptorAllocator.DestroyPool(m_pVkDevice);

		vkDestroyDescriptorSetLayout(m_pVkDevice, m_pRndImageDescriptorLayout, nullptr);
	});

    return true;
}


bool VulkanEngine::InitPipelines() noexcept
{
    return InitBackgroundPipelines();
}


bool VulkanEngine::InitBackgroundPipelines() noexcept
{
    VkPipelineLayoutCreateInfo layoutCreateInfo = {};

    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pSetLayouts = &m_pRndImageDescriptorLayout;
    layoutCreateInfo.setLayoutCount = 1;

    VkPushConstantRange pushConstRange = {};
    pushConstRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstRange.offset = 0;
    pushConstRange.size = sizeof(ComputePushConstants);

    layoutCreateInfo.pPushConstantRanges = &pushConstRange;
    layoutCreateInfo.pushConstantRangeCount = 1;

    ENG_VK_CHECK(vkCreatePipelineLayout(m_pVkDevice, &layoutCreateInfo, VK_NULL_HANDLE, &m_pGradientPipelineLayout));

    VkShaderModule pGradientShaderModule = VK_NULL_HANDLE;
    if (!vkutil::LoadShaderModule(ENG_GRADIENT_COMPUTE_SHADER_PATH, m_pVkDevice, pGradientShaderModule)) {
        ENG_ASSERT_FAIL("Failed to load shader module: {}", ENG_GRADIENT_COMPUTE_SHADER_PATH.string().c_str());
        return false;
    }

    VkShaderModule pSkyShaderModule = VK_NULL_HANDLE;
    if (!vkutil::LoadShaderModule(ENG_SKY_SHADER_PATH, m_pVkDevice, pSkyShaderModule)) {
        ENG_ASSERT_FAIL("Failed to load shader module: {}", ENG_SKY_SHADER_PATH.string().c_str());
        return false;
    }

    VkPipelineShaderStageCreateInfo stageCreateInfo = {};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.module = pGradientShaderModule;
    stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageCreateInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo = {};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = m_pGradientPipelineLayout;
    computePipelineCreateInfo.stage = stageCreateInfo;

    ComputeEffect gradient = {};
    gradient.name = "gradient";
    gradient.layout = m_pGradientPipelineLayout;
    gradient.data.data[0] = glm::vec4(1.f, 0.f, 0.f, 1.f);
    gradient.data.data[1] = glm::vec4(0.f, 0.f, 1.f, 1.f);

    ENG_VK_CHECK(vkCreateComputePipelines(m_pVkDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));
    
    
    computePipelineCreateInfo.stage.module = pSkyShaderModule;
    
    ComputeEffect sky = {};
    sky.name = "sky";
    sky.layout = m_pGradientPipelineLayout;
    sky.data.data[0] = glm::vec4(0.1f, 0.2f, 0.4f, 0.97f);

    ENG_VK_CHECK(vkCreateComputePipelines(m_pVkDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

    m_backgroundEffects.emplace_back(gradient);
    m_backgroundEffects.emplace_back(sky);

    vkDestroyShaderModule(m_pVkDevice, pGradientShaderModule, nullptr);
    vkDestroyShaderModule(m_pVkDevice, pSkyShaderModule, nullptr);

	m_mainDeletionQueue.PushDeletor([&]() {
		vkDestroyPipelineLayout(m_pVkDevice, m_pGradientPipelineLayout, nullptr);
		
        for (ComputeEffect& effect : m_backgroundEffects) {
            vkDestroyPipeline(m_pVkDevice, effect.pipeline, nullptr);
        }
	});

    return true;
}


bool VulkanEngine::InitImGui() noexcept
{
    const std::array poolSizes = { 
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool pImGuiPool = VK_NULL_HANDLE;
	ENG_VK_CHECK(vkCreateDescriptorPool(m_pVkDevice, &poolInfo, nullptr, &pImGuiPool));

    ImGui::CreateContext();

    ImGui_ImplSDL2_InitForVulkan(m_pWindow);

    ImGui_ImplVulkan_InitInfo initInfo = {};

    initInfo.Instance = m_pVkInstance;
	initInfo.PhysicalDevice = m_pVkPhysDevice;
	initInfo.Device = m_pVkDevice;
	initInfo.Queue = m_pVkGraphicsQueue;
	initInfo.DescriptorPool = pImGuiPool;
	initInfo.MinImageCount = 3;
	initInfo.ImageCount = 3;
	initInfo.UseDynamicRendering = true;
	initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapChainImageFormat;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();

	m_mainDeletionQueue.PushDeletor([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(m_pVkDevice, pImGuiPool, nullptr);
	});

    return true;
}


void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer pCmdBuf)>&& function) noexcept
{
    VkCommandBuffer pCmdBuf = m_pImmCommandBuffer;

    ENG_VK_CHECK(vkResetFences(m_pVkDevice, 1, &m_pImmFence));
    ENG_VK_CHECK(vkResetCommandBuffer(pCmdBuf, 0));

    VkCommandBufferBeginInfo cmdBufBeginInfo = vkinit::CmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    ENG_VK_CHECK(vkBeginCommandBuffer(pCmdBuf, &cmdBufBeginInfo));
    function(pCmdBuf);
    ENG_VK_CHECK(vkEndCommandBuffer(pCmdBuf));

    VkCommandBufferSubmitInfo cmdBufSubmitInfo = vkinit::CmdBufferSubmitInfo(pCmdBuf);
    VkSubmitInfo2 submitInfo2 = vkinit::SubmitInfo2(&cmdBufSubmitInfo, nullptr, nullptr);

    ENG_VK_CHECK(vkQueueSubmit2(m_pVkGraphicsQueue, 1, &submitInfo2, m_pImmFence));
    ENG_VK_CHECK(vkWaitForFences(m_pVkDevice, 1, &m_pImmFence, true, 9'999'999'999));
}
