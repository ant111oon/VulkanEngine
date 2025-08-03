#include "pch.h"

#include "core.h"

#include "vk_engine.h"

#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_images.h"
#include "vk_loader.h"

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

static const std::filesystem::path ENG_GRADIENT_CS_PATH = "../shaders/bin/gradient.comp.spv";
static const std::filesystem::path ENG_SKY_CS_PATH = "../shaders/bin/sky.comp.spv";
static const std::filesystem::path ENG_COLORED_TRIANGLE_PS_PATH = "../shaders/bin/colored_mesh.frag.spv";
static const std::filesystem::path ENG_TEX_IMAGE_PS_PATH = "../shaders/bin/tex_image.frag.spv";
static const std::filesystem::path ENG_COLORED_TRIANGLE_MESH_VS_PATH = "../shaders/bin/colored_mesh.vert.spv";
static const std::filesystem::path ENG_MESH_VS_PATH = "../shaders/bin/mesh.vert.spv";
static const std::filesystem::path ENG_MESH_FS_PATH = "../shaders/bin/mesh.frag.spv";

static const std::filesystem::path ENG_BASIC_GLTF_MESH_PATH = "../assets/basicmesh.glb";
static const std::filesystem::path ENG_STRUCTURE_GLTF_MESH_PATH = "../assets/structure.glb";


#define ENG_RND_BACKGROUND_VERSION_CLEAR 0
#define ENG_RND_BACKGROUND_VERSION_COMPUTE_GRADIENT 1
#define ENG_RND_BACKGROUND_VERSION ENG_RND_BACKGROUND_VERSION_COMPUTE_GRADIENT


#define ENG_CHECK_SDL_ERROR(COND, ...)                      \
    if (!(COND)) {                                          \
        ENG_ASSERT_FAIL("[SDL ERROR]: {}", SDL_GetError()); \
        return __VA_ARGS__;                                 \
    }


static bool IsRendObjVisible(const RenderObject& obj, const glm::mat4& viewproj)
{
    static constexpr std::array corners = {
        glm::vec3 {  1,  1,  1 },
        glm::vec3 {  1,  1, -1 },
        glm::vec3 {  1, -1,  1 },
        glm::vec3 {  1, -1, -1 },
        glm::vec3 { -1,  1,  1 },
        glm::vec3 { -1,  1, -1 },
        glm::vec3 { -1, -1,  1 },
        glm::vec3 { -1, -1, -1 },
    };

    const glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min(1.5f);
    glm::vec3 max(-1.5f);

    for (size_t c = 0; c < corners.size(); ++c) {
        const glm::vec4 boundCornerLPos = glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);
        glm::vec4 v = matrix * boundCornerLPos;

        v = v / v.w;

        min = glm::min(glm::vec3(v.x, v.y, v.z), min);
        max = glm::max(glm::vec3(v.x, v.y, v.z), max);
    }

    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
        return false;
    } else {
        return true;
    }
}


void GLTFMetallic_Roughness::BuildPipelines(VulkanEngine* pEngine)
{
    VkShaderModule meshFragShader;
	if (!vkutil::LoadShaderModule(ENG_MESH_FS_PATH, pEngine->m_pVkDevice, meshFragShader)) {
		ENG_ASSERT_FAIL("Failed to load shader module: {}", ENG_MESH_FS_PATH.string().c_str());
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::LoadShaderModule(ENG_MESH_VS_PATH, pEngine->m_pVkDevice, meshVertexShader)) {
		ENG_ASSERT_FAIL("Failed to load shader module: {}", ENG_MESH_VS_PATH.string().c_str());
	}

	VkPushConstantRange matrixRange = {};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    descSetLayout = layoutBuilder.Build(pEngine->m_pVkDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { pEngine->m_pSceneDataDescriptorLayout, descSetLayout };

	VkPipelineLayoutCreateInfo meshLayoutInfo = vkinit::PipelineLayoutCreateInfo();
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pSetLayouts = layouts;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	ENG_VK_CHECK(vkCreatePipelineLayout(pEngine->m_pVkDevice, &meshLayoutInfo, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

	vkutil::PipelineBuilder pipelineBuilder;
	pipelineBuilder.SetShaders(meshVertexShader, meshFragShader);
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.DisableMultisampling();
	pipelineBuilder.DisableBlending();
	pipelineBuilder.SetDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder.SetColorAttachmentFormat(pEngine->m_rndImage.format);
	pipelineBuilder.SetDepthAttachmentFormat(pEngine->m_depthImage.format);
	pipelineBuilder.m_pipelineLayout = newLayout;

    opaquePipeline.pipeline = pipelineBuilder.Build(pEngine->m_pVkDevice);

	pipelineBuilder.SetAdditiveBlending();

	pipelineBuilder.SetDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.Build(pEngine->m_pVkDevice);
	
	vkDestroyShaderModule(pEngine->m_pVkDevice, meshFragShader, nullptr);
	vkDestroyShaderModule(pEngine->m_pVkDevice, meshVertexShader, nullptr);
}


void GLTFMetallic_Roughness::ClearResources(VkDevice device)
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
	vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
}


MaterialInstance GLTFMetallic_Roughness::WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
    MaterialInstance matData = {};
	matData.passType = pass;
    matData.pPipeline = pass == MaterialPass::TRANSPARENT ? &transparentPipeline : &opaquePipeline;
	matData.descriptorSet = descriptorAllocator.Allocate(device, descSetLayout);

	descWriter.Clear();
	descWriter.WriteBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descWriter.WriteImage(1, resources.colorImage.pImageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descWriter.WriteImage(2, resources.metalRoughImage.pImageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	descWriter.UpdateSet(device, matData.descriptorSet);

	return matData;
}


void MeshNode::Render(const glm::mat4& topMatrix, RenderContext& ctx)
{
    glm::mat4 nodeMatrix = topMatrix * worldTrs;

	for (GeoSurface& surface : pMesh->surfaces) {
		RenderObject def = {};
		def.indexCount = surface.count;
		def.firstIndex = surface.startIndex;
		def.indexBuffer = pMesh->meshBuffers.idxBuff.pBuffer;
		def.pMaterial = &surface.material->data;
        def.bounds = surface.bounds;
		def.transform = nodeMatrix;
		def.vertexBufferAddress = pMesh->meshBuffers.vertBufferGpuAddress;
        
        if (def.pMaterial->passType == MaterialPass::OPAQUE) {
            ctx.opaqueSurfaces.push_back(def);
        } else {
            ctx.transparentSurfaces.push_back(def);
        }
	}

	Node::Render(topMatrix, ctx);
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

    const SDL_WindowFlags windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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

    InitDefaultData();

    m_mainCamera.velocity = glm::vec3(0.f);
	m_mainCamera.position = glm::vec3(30.f, 0.f, -85.f);
    m_mainCamera.pitch = 0.f;
    m_mainCamera.yaw = 0.f;

    auto structureFile = LoadGLTF(this, ENG_STRUCTURE_GLTF_MESH_PATH);
    ENG_ASSERT(structureFile.has_value());

    m_loadedScenes["structure"] = structureFile.value();

    m_isInitialized = true;
}


void VulkanEngine::Terminate() noexcept
{
    if (!m_isInitialized) {
        return;
    }

    vkDeviceWaitIdle(m_pVkDevice);

    m_loadedScenes.clear();

    for (size_t i = 0; i < FRAMES_DATA_INST_COUNT; ++i) {
        vkDestroyCommandPool(m_pVkDevice, m_framesData[i].pVkCmdPool, nullptr);

        vkDestroyFence(m_pVkDevice, m_framesData[i].pVkRenderFence, nullptr);
		vkDestroySemaphore(m_pVkDevice, m_framesData[i].pVkRenderSemaphore, nullptr);
		vkDestroySemaphore(m_pVkDevice ,m_framesData[i].pVkSwapChainSemaphore, nullptr);
    
        m_framesData[i].deletionQueue.Flush();
    }

    m_metalRoughMaterial.ClearResources(m_pVkDevice);

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
        auto startTime = std::chrono::system_clock::now();

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
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_F5) {
                        m_isFlyCameraMode = !m_isFlyCameraMode;
                        SDL_SetRelativeMouseMode(m_isFlyCameraMode ? SDL_TRUE : SDL_FALSE);
                    }
                    break;

                default:
                    break;
            }

            if (m_isFlyCameraMode) {
                m_mainCamera.ProcessSDLEvent(event);
            }
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        if (!m_needRender) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (m_needResizeSwapChain) {
            ResizeSwapChain();
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        
        ImGui::NewFrame();
        RenderDbgUI();
        ImGui::Render();

        Render();

        auto endTime = std::chrono::system_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        m_stats.frameTime = elapsedTime.count() / 1000.f;
    }
}


VulkanEngine::~VulkanEngine()
{
    Terminate();
}


void VulkanEngine::Render() noexcept
{
    UpdateScene();

    FrameData& currFrameData = GetCurrentFrameData();

    constexpr uint64_t waitRenderFenceTimeoutNs = 1'000'000'000;

    ENG_VK_CHECK(vkWaitForFences(m_pVkDevice, 1, &currFrameData.pVkRenderFence, true, waitRenderFenceTimeoutNs));
	
    currFrameData.deletionQueue.Flush();
    currFrameData.descriptorAllocator.ClearPools(m_pVkDevice);

    constexpr uint64_t acquireNextSwapChainImageTimeoutNs = 1'000'000'000;

    uint32_t swapChainImageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(m_pVkDevice, m_pVkSwapChain, acquireNextSwapChainImageTimeoutNs,
        currFrameData.pVkSwapChainSemaphore, VK_NULL_HANDLE, &swapChainImageIndex);
        
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_needResizeSwapChain = true;
        return;
    }

    VkCommandBuffer pCmdBuf = currFrameData.pVkCmdBuffer;
        
    ENG_VK_CHECK(vkResetFences(m_pVkDevice, 1, &currFrameData.pVkRenderFence));
    ENG_VK_CHECK(vkResetCommandBuffer(pCmdBuf, 0));

    m_rndExtent.width  = std::min(m_swapChainExtent.width, m_rndImage.extent.width) * m_dynResScale;
    m_rndExtent.height = std::min(m_swapChainExtent.height, m_rndImage.extent.height) * m_dynResScale;

    const VkCommandBufferBeginInfo cmdBuffBeginInfo = vkinit::CmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    ENG_VK_CHECK(vkBeginCommandBuffer(pCmdBuf, &cmdBuffBeginInfo));

    vkutil::TransitImage(pCmdBuf, m_rndImage.pImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    RenderBackground(pCmdBuf);

    vkutil::TransitImage(pCmdBuf, m_rndImage.pImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::TransitImage(pCmdBuf, m_depthImage.pImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    RenderGeometry(pCmdBuf);

    vkutil::TransitImage(pCmdBuf, m_rndImage.pImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    
    vkutil::TransitImage(pCmdBuf, m_vkSwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    vkutil::CopyImage(pCmdBuf, m_rndImage.pImage, m_rndExtent, m_vkSwapChainImages[swapChainImageIndex], m_swapChainExtent, m_dynResCopyFilter);

    vkutil::TransitImage(pCmdBuf, m_vkSwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    RenderImGui(pCmdBuf, m_vkSwapChainImageViews[swapChainImageIndex]);

    vkutil::TransitImage(pCmdBuf, m_vkSwapChainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    
	ENG_VK_CHECK(vkEndCommandBuffer(pCmdBuf));

    VkCommandBufferSubmitInfo cmdBufSubmitInfo = vkinit::CmdBufferSubmitInfo(pCmdBuf);	
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, currFrameData.pVkSwapChainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currFrameData.pVkRenderSemaphore);	
	
	VkSubmitInfo2 submitInfo2 = vkinit::SubmitInfo2(&cmdBufSubmitInfo, &signalInfo, &waitInfo);	

	ENG_VK_CHECK(vkQueueSubmit2(m_pVkGraphicsQueue, 1, &submitInfo2, currFrameData.pVkRenderFence));

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currFrameData.pVkRenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_pVkSwapChain,
        .pImageIndices = &swapChainImageIndex,
    };

    VkResult presentResult = vkQueuePresentKHR(m_pVkGraphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_needResizeSwapChain = true;
        return;
    } else {
        ENG_VK_CHECK(presentResult);
    }

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

    vkCmdBindDescriptorSets(pCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pComputeBackgroundPipelineLayout, 0, 1, &m_pComputeBackgroundDescriptors, 0, nullptr);

    vkCmdPushConstants(pCmdBuf, m_pComputeBackgroundPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    vkCmdDispatch(pCmdBuf, std::ceil(m_rndExtent.width / 16.f), std::ceil(m_rndExtent.height / 16.f), 1);
#else
    #error Invalid RenderBackground version
#endif
}


void VulkanEngine::RenderGeometry(VkCommandBuffer pCmdBuf) noexcept
{
    m_stats.drawCallCount = 0;
    m_stats.triangleCount = 0;
    
    auto start = std::chrono::system_clock::now();

    VkRenderingAttachmentInfo colorAttachment = vkinit::RenderingAttachmentInfo(m_rndImage.pImageView, std::nullopt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::DepthAttachmentInfo(m_depthImage.pImageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::RenderingInfo(m_rndExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(pCmdBuf, &renderInfo);

	BufferHandle gpuSceneDataBuffer = CreateBuffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	GetCurrentFrameData().deletionQueue.PushDeletor([=, this]() {
		BufferHandle buffer = gpuSceneDataBuffer;
        DestroyBuffer(buffer);
	});

	SceneData* sceneUniformData = (SceneData*)gpuSceneDataBuffer.pAllocation->GetMappedData();
	*sceneUniformData = m_sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = GetCurrentFrameData().descriptorAllocator.Allocate(m_pVkDevice, m_pSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.WriteBuffer(0, gpuSceneDataBuffer.pBuffer, sizeof(SceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.UpdateSet(m_pVkDevice, globalDescriptor);

    MaterialPipeline* pLastPipeline = nullptr;
    MaterialInstance* pLastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto Render = [&](const RenderObject& obj) {
        if (obj.pMaterial != pLastMaterial) {
            pLastMaterial = obj.pMaterial;
            
            if (obj.pMaterial->pPipeline != pLastPipeline) {
                pLastPipeline = obj.pMaterial->pPipeline;

                vkCmdBindPipeline(pCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.pMaterial->pPipeline->pipeline);
                vkCmdBindDescriptorSets(pCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.pMaterial->pPipeline->layout, 0, 1, 
                    &globalDescriptor, 0, nullptr);

                VkViewport viewport = {};
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = m_rndExtent.width;
                viewport.height = m_rndExtent.height;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;

                vkCmdSetViewport(pCmdBuf, 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = m_rndExtent.width;
                scissor.extent.height = m_rndExtent.height;

                vkCmdSetScissor(pCmdBuf, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(pCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.pMaterial->pPipeline->layout, 1, 1, 
                &obj.pMaterial->descriptorSet, 0, nullptr);
        }

        if (obj.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = obj.indexBuffer;
            vkCmdBindIndexBuffer(pCmdBuf, obj.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }

		GPUDrawPushConstants pushConstants;
		pushConstants.vertBufferGpuAddress = obj.vertexBufferAddress;
		pushConstants.transform = obj.transform;
		vkCmdPushConstants(pCmdBuf, obj.pMaterial->pPipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(pCmdBuf, obj.indexCount, 1, obj.firstIndex, 0, 0);

        m_stats.drawCallCount++;
        m_stats.triangleCount += obj.indexCount / 3;   
    };

    std::vector<uint32_t> opaqueDraws;
    opaqueDraws.reserve(m_mainDrawContext.opaqueSurfaces.size());

    for (uint32_t i = 0; i < m_mainDrawContext.opaqueSurfaces.size(); i++) {
        if (IsRendObjVisible(m_mainDrawContext.opaqueSurfaces[i], m_sceneData.viewProjMat)) {
            opaqueDraws.push_back(i);
        }
    }

    std::sort(opaqueDraws.begin(), opaqueDraws.end(), [&](uint32_t iA, uint32_t iB) {
        const RenderObject& a = m_mainDrawContext.opaqueSurfaces[iA];
        const RenderObject& b = m_mainDrawContext.opaqueSurfaces[iB];

        return a.pMaterial == b.pMaterial ? a.indexBuffer < b.indexBuffer : a.pMaterial < b.pMaterial;
    });

	for (uint32_t idx : opaqueDraws) {
		Render(m_mainDrawContext.opaqueSurfaces[idx]);
	}

    for (const RenderObject& obj : m_mainDrawContext.transparentSurfaces) {
		if (IsRendObjVisible(obj, m_sceneData.viewProjMat)) {
            Render(obj);
        }
	}

	vkCmdEndRendering(pCmdBuf);

    auto end = std::chrono::system_clock::now();
    
    m_stats.meshRenderTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;
}


void VulkanEngine::RenderDbgUI() noexcept
{
    if (ImGui::Begin("Debug info")) {            
		ComputeEffect& selected = m_backgroundEffects[m_currBackgroundEffect];
		
		ImGui::Text("Selected effect: %s", selected.name.data());
		ImGui::SliderInt("Effect Index", &m_currBackgroundEffect, 0, m_backgroundEffects.size() - 1);
		
        for (size_t i = 0; i < std::size(selected.data.data); ++i) {
            char label[64] = {};
            sprintf_s(label, "data %u", i);
            
            glm::vec4& data = selected.data.data[i];
            ImGui::ColorEdit4(label, &data.x);
        }

        ImGui::SliderFloat("Dynamic Resolution Scale", &m_dynResScale, 0.1f, 1.f);

        static const char* dynResCopyFileters[] = { "Linear", "Nearest" };
        static const char* pCurrDynResCopyFileter = dynResCopyFileters[0];
        if (ImGui::BeginCombo("Dynamic Resolution Copy Filter", pCurrDynResCopyFileter)) {
            for (size_t i = 0; i < IM_ARRAYSIZE(dynResCopyFileters); ++i) {
                const bool isSelected = (pCurrDynResCopyFileter == dynResCopyFileters[i]);
                    
                if (ImGui::Selectable(dynResCopyFileters[i], isSelected)) {
                    pCurrDynResCopyFileter = dynResCopyFileters[i];
                }
                    
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            m_dynResCopyFilter = pCurrDynResCopyFileter == dynResCopyFileters[0] ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

            ImGui::EndCombo();
        }

        ImGui::NewLine();
        ImGui::Text("Is Fly Camera (F5):");
        ImGui::SameLine();
        ImGui::TextColored(m_isFlyCameraMode ? ImVec4(0.f, 1.f, 0.f, 1.f) : ImVec4(1.f, 0.f, 0.f, 1.f), m_isFlyCameraMode ? "true" : "false");
        ImGui::SliderFloat("Camera Speed", &m_mainCamera.speed, 0.f, 10.f);

        ImGui::End();
	}

    if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Frametime %f ms", m_stats.frameTime);
        ImGui::Text("Draw time %f ms", m_stats.meshRenderTime);
        ImGui::Text("Update time %f ms", m_stats.sceneUpdateTime);
        ImGui::Text("Triangles %i", m_stats.triangleCount);
        ImGui::Text("Draws %i", m_stats.drawCallCount);
        ImGui::End();
    }
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

    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .bufferDeviceAddress = true,
    };

    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

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

	VkImageUsageFlags rndImageUsages{};
	rndImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	rndImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	rndImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	rndImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    m_rndImage = CreateImage(rndImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, rndImageUsages);
    m_depthImage = CreateImage(rndImageExtent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    m_mainDeletionQueue.PushDeletor([&]() {
        DestroyImage(m_rndImage);
        DestroyImage(m_depthImage);
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


void VulkanEngine::ResizeSwapChain() noexcept
{
    vkDeviceWaitIdle(m_pVkDevice);
    DestroySwapChain();

    int32_t w = 0, h = 0;
    SDL_GetWindowSize(m_pWindow, &w, &h);

    m_windowExtent.width = w;
    m_windowExtent.height = h;

    CreateSwapChain(m_windowExtent.width, m_windowExtent.height);

    m_needResizeSwapChain = false;
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
    std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 1> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f }
	};

	m_globalDescriptorAllocator.Init(m_pVkDevice, 10, sizes);

	DescriptorLayoutBuilder builder;
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	m_pComputeBackgroundDescriptorLayout = builder.Build(m_pVkDevice, VK_SHADER_STAGE_COMPUTE_BIT);
    m_pComputeBackgroundDescriptors = m_globalDescriptorAllocator.Allocate(m_pVkDevice, m_pComputeBackgroundDescriptorLayout);	

    builder.Clear();
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_pSceneDataDescriptorLayout = builder.Build(m_pVkDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    builder.Clear();
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	m_singleImageDescriptorLayout = builder.Build(m_pVkDevice, VK_SHADER_STAGE_FRAGMENT_BIT);

	DescriptorWriter writer;
    writer.WriteImage(0, m_rndImage.pImageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.UpdateSet(m_pVkDevice, m_pComputeBackgroundDescriptors);

    for (size_t i = 0; i < FRAMES_DATA_INST_COUNT; ++i) {
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = { 
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		m_framesData[i].descriptorAllocator = DescriptorAllocatorGrowable{};
		m_framesData[i].descriptorAllocator.Init(m_pVkDevice, 1000, frameSizes);
	}

	m_mainDeletionQueue.PushDeletor([&]() {
        for (size_t i = 0; i < FRAMES_DATA_INST_COUNT; ++i) {
            m_framesData[i].descriptorAllocator.DestroyPools(m_pVkDevice);
        }

		m_globalDescriptorAllocator.DestroyPools(m_pVkDevice);
        
        vkDestroyDescriptorSetLayout(m_pVkDevice, m_singleImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_pVkDevice, m_pSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_pVkDevice, m_pComputeBackgroundDescriptorLayout, nullptr);
	});

    return true;
}


bool VulkanEngine::InitPipelines() noexcept
{
    if (!InitBackgroundPipelines()) {
        return false;
    }

    m_metalRoughMaterial.BuildPipelines(this);

    return true;
}


bool VulkanEngine::InitBackgroundPipelines() noexcept
{
    VkPipelineLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.setLayoutCount = 1;
    layoutCreateInfo.pSetLayouts = &m_pComputeBackgroundDescriptorLayout;

    VkPushConstantRange pushConstRange = {};
    pushConstRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstRange.offset = 0;
    pushConstRange.size = sizeof(ComputePushConstants);

    layoutCreateInfo.pPushConstantRanges = &pushConstRange;
    layoutCreateInfo.pushConstantRangeCount = 1;

    ENG_VK_CHECK(vkCreatePipelineLayout(m_pVkDevice, &layoutCreateInfo, VK_NULL_HANDLE, &m_pComputeBackgroundPipelineLayout));

    VkShaderModule pGradientShaderModule = VK_NULL_HANDLE;
    if (!vkutil::LoadShaderModule(ENG_GRADIENT_CS_PATH, m_pVkDevice, pGradientShaderModule)) {
        ENG_ASSERT_FAIL("Failed to load shader module: {}", ENG_GRADIENT_CS_PATH.string().c_str());
        return false;
    }

    VkShaderModule pSkyShaderModule = VK_NULL_HANDLE;
    if (!vkutil::LoadShaderModule(ENG_SKY_CS_PATH, m_pVkDevice, pSkyShaderModule)) {
        ENG_ASSERT_FAIL("Failed to load shader module: {}", ENG_SKY_CS_PATH.string().c_str());
        return false;
    }

    VkPipelineShaderStageCreateInfo stageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = pGradientShaderModule,
        .pName = "main",
    };

    VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stageCreateInfo,
        .layout = m_pComputeBackgroundPipelineLayout,
    };

    ComputeEffect gradient = {};
    gradient.name = "gradient";
    gradient.layout = m_pComputeBackgroundPipelineLayout;
    gradient.data.data[0] = glm::vec4(1.f, 0.f, 0.f, 1.f);
    gradient.data.data[1] = glm::vec4(0.f, 0.f, 1.f, 1.f);

    ENG_VK_CHECK(vkCreateComputePipelines(m_pVkDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));
    
    computePipelineCreateInfo.stage.module = pSkyShaderModule;
    
    ComputeEffect sky = {};
    sky.name = "sky";
    sky.layout = m_pComputeBackgroundPipelineLayout;
    sky.data.data[0] = glm::vec4(0.1f, 0.2f, 0.4f, 0.97f);

    ENG_VK_CHECK(vkCreateComputePipelines(m_pVkDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

    m_backgroundEffects.emplace_back(gradient);
    m_backgroundEffects.emplace_back(sky);

    vkDestroyShaderModule(m_pVkDevice, pGradientShaderModule, nullptr);
    vkDestroyShaderModule(m_pVkDevice, pSkyShaderModule, nullptr);

	m_mainDeletionQueue.PushDeletor([&]() {
		vkDestroyPipelineLayout(m_pVkDevice, m_pComputeBackgroundPipelineLayout, nullptr);
		
        for (ComputeEffect& effect : m_backgroundEffects) {
            vkDestroyPipeline(m_pVkDevice, effect.pipeline, nullptr);
        }
	});

    return true;
}


void VulkanEngine::InitDefaultData() noexcept
{
    const uint32_t whiteColorU32 = glm::packUnorm4x8(glm::vec4(1.f));
	m_whiteImage = CreateImage(VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, (const void*)&whiteColorU32);

	const uint32_t greyColorU32 = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1.f));
	m_greyImage = CreateImage(VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, (const void*)&greyColorU32);

	const uint32_t blackColorU32 = glm::packUnorm4x8(glm::vec4(0.f, 0.f, 0.f, 1.f));
	m_blackImage = CreateImage(VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, (const void*)&blackColorU32);

	std::array<uint32_t, 16 * 16> pixels;
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? whiteColorU32 : blackColorU32;
		}
	}
	m_checkerboardImage = CreateImage(VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, pixels.data());

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(m_pVkDevice, &samplerInfo, nullptr, &m_nearestSampler);

	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(m_pVkDevice, &samplerInfo, nullptr, &m_linearSampler);

    GLTFMetallic_Roughness::MaterialResources materialResources;
	materialResources.colorImage = m_whiteImage;
	materialResources.colorSampler = m_linearSampler;
	materialResources.metalRoughImage = m_whiteImage;
	materialResources.metalRoughSampler = m_linearSampler;

	BufferHandle materialConstants = CreateBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	GLTFMetallic_Roughness::MaterialConstants* pSceneUniformData = (GLTFMetallic_Roughness::MaterialConstants*)materialConstants.pAllocation->GetMappedData();
	pSceneUniformData->colorFactors = glm::vec4{1,1,1,1};
	pSceneUniformData->metallicRoughnessFactors = glm::vec4{1,0.5,0,0};

	m_mainDeletionQueue.PushDeletor([=, this]() {
		BufferHandle buffer = materialConstants;
        DestroyBuffer(buffer);
	});

	materialResources.dataBuffer = materialConstants.pBuffer;
	materialResources.dataBufferOffset = 0;

	m_defaultData = m_metalRoughMaterial.WriteMaterial(m_pVkDevice, MaterialPass::OPAQUE, materialResources, m_globalDescriptorAllocator);

	m_mainDeletionQueue.PushDeletor([&]() {
        vkDestroySampler(m_pVkDevice, m_nearestSampler, nullptr);
        vkDestroySampler(m_pVkDevice, m_linearSampler, nullptr);

        DestroyImage(m_whiteImage);
        DestroyImage(m_blackImage);
        DestroyImage(m_greyImage);
        DestroyImage(m_checkerboardImage);
	});
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

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = (uint32_t)poolSizes.size(),
        .pPoolSizes = poolSizes.data(),
    };

	VkDescriptorPool pImGuiPool = VK_NULL_HANDLE;
	ENG_VK_CHECK(vkCreateDescriptorPool(m_pVkDevice, &poolInfo, nullptr, &pImGuiPool));

    ImGui::CreateContext();

    ImGui_ImplSDL2_InitForVulkan(m_pWindow);

    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance = m_pVkInstance,
        .PhysicalDevice = m_pVkPhysDevice,
        .Device = m_pVkDevice,
        .Queue = m_pVkGraphicsQueue,
        .DescriptorPool = pImGuiPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .UseDynamicRendering = true,
    };
    
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapChainImageFormat;

	ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();

	m_mainDeletionQueue.PushDeletor([this, pDescPool = pImGuiPool]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(m_pVkDevice, pDescPool, nullptr);
	});

    return true;
}


void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer pCmdBuf)>&& function) const noexcept
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


void VulkanEngine::UpdateScene()
{
    m_stats.sceneUpdateTime = 0;
    
    auto start = std::chrono::system_clock::now();

    m_mainCamera.Update();

    m_mainDrawContext.opaqueSurfaces.clear();
    m_mainDrawContext.transparentSurfaces.clear();

    m_loadedScenes["structure"]->Render(glm::identity<glm::mat4>(), m_mainDrawContext);

    const glm::mat4 viewMat = m_mainCamera.GetViewMatrix();
    glm::mat4 projMat = glm::perspective(glm::radians(70.f), (float)m_windowExtent.width / (float)m_windowExtent.height, 10000.f, 0.1f);

    projMat[1][1] *= -1;

	m_sceneData.viewMat = viewMat;
	m_sceneData.projMat = projMat;
	m_sceneData.viewProjMat = projMat * viewMat;

	m_sceneData.ambientColor = glm::vec4(0.1f);
	m_sceneData.sunLightColor = glm::vec4(1.f);
	m_sceneData.sunLightDirectionAndPower = glm::vec4(0.f, 1.f, 0.5f, 1.f);

    auto end = std::chrono::system_clock::now();
    
    m_stats.sceneUpdateTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;
}


BufferHandle VulkanEngine::CreateBuffer(size_t size, VkBufferUsageFlags bufUsage, VmaMemoryUsage memUsage) const noexcept
{
    VkBufferCreateInfo bufCreateInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufCreateInfo.size = size;
    bufCreateInfo.usage = bufUsage;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = memUsage;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    BufferHandle buffer = {};
    ENG_VK_CHECK(vmaCreateBuffer(m_pVMA, &bufCreateInfo, &allocCreateInfo, &buffer.pBuffer, &buffer.pAllocation, &buffer.allocationInfo));

    return buffer;
}


void VulkanEngine::DestroyBuffer(BufferHandle& buffer) const noexcept
{
    vmaDestroyBuffer(m_pVMA, buffer.pBuffer, buffer.pAllocation);
    
    buffer.pBuffer = VK_NULL_HANDLE;
    buffer.pAllocation = VK_NULL_HANDLE;
}


ImageHandle VulkanEngine::CreateImage(const VkExtent3D& extent, VkFormat format, VkImageUsageFlags usage, const void* pData, bool mipmapped)
{
    ImageHandle image = {};
    image.extent = extent;
    image.format = format;

    if (pData != nullptr) {
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(extent, format, usage);
    if (mipmapped) {
        imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ENG_VK_CHECK(vmaCreateImage(m_pVMA, &imageInfo, &allocInfo, &image.pImage, &image.pAllocation, nullptr));
    
    const VkImageAspectFlags aspectFlag = (format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(image.pImage, format, aspectFlag);
	viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;

	ENG_VK_CHECK(vkCreateImageView(m_pVkDevice, &viewInfo, nullptr, &image.pImageView));

    if (pData != nullptr) {
        const size_t dataSize = extent.width * extent.height * extent.depth * 4;
        BufferHandle stagingBuffer = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    
        memcpy(stagingBuffer.allocationInfo.pMappedData, pData, dataSize);
    
        ImmediateSubmit([&](VkCommandBuffer cmdBuffer){
            vkutil::TransitImage(cmdBuffer, image.pImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
    
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = extent;
    
            vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer.pBuffer, image.pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    
            if (mipmapped) {
                vkutil::GenerateMipmaps(cmdBuffer, image.pImage, VkExtent2D{image.extent.width, image.extent.height});
            } else {
                vkutil::TransitImage(cmdBuffer, image.pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });
    
        DestroyBuffer(stagingBuffer);
    }

    return image;
}


void VulkanEngine::DestroyImage(ImageHandle& image)
{
    vkDestroyImageView(m_pVkDevice, image.pImageView, nullptr);
	vmaDestroyImage(m_pVMA, image.pImage, image.pAllocation);

    image.pImage = VK_NULL_HANDLE;
    image.pImageView = VK_NULL_HANDLE;
    image.pAllocation = VK_NULL_HANDLE;
    image.extent = {};
    image.format = VK_FORMAT_UNDEFINED;
}


MeshGpuBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)  const noexcept
{
    
    MeshGpuBuffers mesh;
    
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	mesh.vertBuff = CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo deviceAdressInfo = { 
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = mesh.vertBuff.pBuffer
    };
	mesh.vertBufferGpuAddress = vkGetBufferDeviceAddress(m_pVkDevice, &deviceAdressInfo);

    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
	mesh.idxBuff = CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    BufferHandle stagingBuff = CreateBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = stagingBuff.pAllocation->GetMappedData();

	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy((uint8_t*)data + vertexBufferSize, indices.data(), indexBufferSize);

	ImmediateSubmit([&](VkCommandBuffer pCmd) {
		VkBufferCopy vertexCopy = { 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(pCmd, stagingBuff.pBuffer, mesh.vertBuff.pBuffer, 1, &vertexCopy);

		VkBufferCopy indexCopy = { 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(pCmd, stagingBuff.pBuffer, mesh.idxBuff.pBuffer, 1, &indexCopy);
	});

	DestroyBuffer(stagingBuff);

	return mesh;
}
