#include "pch.h"

#include "vk_pipelines.h"
#include "vk_initializers.h"


namespace vkutil
{
    PipelineBuilder::PipelineBuilder()
    {
        Clear();
    }

    
    VkPipeline PipelineBuilder::Build(VkDevice pDevice) noexcept
    {
        VkPipelineViewportStateCreateInfo viewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineColorBlendStateCreateInfo colorBlending = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &m_colorBlendAttachment,
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = { 
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
        };

        VkGraphicsPipelineCreateInfo pipelineInfo = { 
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &m_renderInfo,
            .stageCount = (uint32_t)m_shaderStages.size(),
            .pStages = m_shaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &m_inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &m_rasterizer,
            .pMultisampleState = &m_multisampling,
            .pDepthStencilState = &m_depthStencil,
            .pColorBlendState = &colorBlending,
            .layout = m_pipelineLayout,
        };

        const VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicInfo.pDynamicStates = dynStates;
        dynamicInfo.dynamicStateCount = std::size(dynStates);

        pipelineInfo.pDynamicState = &dynamicInfo;

        VkPipeline newPipeline = VK_NULL_HANDLE;
        if (vkCreateGraphicsPipelines(pDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
            ENG_ASSERT_FAIL("failed to create graphics pipeline");
            return VK_NULL_HANDLE;
        }

        return newPipeline;
    }


    void PipelineBuilder::Clear() noexcept
    {
        m_shaderStages.clear();
        
        m_inputAssembly         = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        m_rasterizer            = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        m_multisampling         = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        m_depthStencil          = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        m_renderInfo            = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        m_colorBlendAttachment  = {};
        m_pipelineLayout        = {};
    }

    
    void PipelineBuilder::SetShaders(VkShaderModule pVertexShader, VkShaderModule pPixelShader) noexcept
    {
        m_shaderStages.clear();

        m_shaderStages.emplace_back(vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, pVertexShader));
        m_shaderStages.emplace_back(vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, pPixelShader));
    }

    
    void PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology) noexcept
    {
        m_inputAssembly.topology = topology;
        m_inputAssembly.primitiveRestartEnable = VK_FALSE;
    }


    void PipelineBuilder::SetPolygonMode(VkPolygonMode mode) noexcept
    {
        m_rasterizer.polygonMode = mode;
        m_rasterizer.lineWidth = 1.f;;
    }

    
    void PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) noexcept
    {
        m_rasterizer.cullMode = cullMode;
        m_rasterizer.frontFace = frontFace;
    }

    
    void PipelineBuilder::SetDepthTest(bool depthWriteEnable, VkCompareOp cmpOp) noexcept
    {
        m_depthStencil.depthTestEnable = VK_TRUE;
        m_depthStencil.depthWriteEnable = depthWriteEnable;
        m_depthStencil.depthCompareOp = cmpOp;
        m_depthStencil.depthBoundsTestEnable = VK_FALSE;
        m_depthStencil.stencilTestEnable = VK_FALSE;
        m_depthStencil.front = {};
        m_depthStencil.back = {};
        m_depthStencil.minDepthBounds = 0.f;
        m_depthStencil.maxDepthBounds = 1.f;
    }


    void PipelineBuilder::SetAdditiveBlending() noexcept
    {
        m_colorBlendAttachment.colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        m_colorBlendAttachment.blendEnable         = VK_TRUE;
        m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        m_colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        m_colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    }


    void PipelineBuilder::SetAlphaBlending() noexcept
    {
        m_colorBlendAttachment.colorWriteMask 
            = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        m_colorBlendAttachment.blendEnable         = VK_TRUE;
        m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        m_colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        m_colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    }


    void PipelineBuilder::SetColorAttachmentFormat(VkFormat format) noexcept
    {
        m_colorAttachmentFormat = format;
        m_renderInfo.colorAttachmentCount = 1;
        m_renderInfo.pColorAttachmentFormats = &m_colorAttachmentFormat;
    }

    
    void PipelineBuilder::SetDepthAttachmentFormat(VkFormat format) noexcept
    {
        m_renderInfo.depthAttachmentFormat = format;
    }


    void PipelineBuilder::DisableMultisampling() noexcept
    {
        m_multisampling.sampleShadingEnable = VK_FALSE;
        m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        m_multisampling.minSampleShading = 1.f;
        m_multisampling.pSampleMask = nullptr;
        m_multisampling.alphaToCoverageEnable = VK_FALSE;
        m_multisampling.alphaToOneEnable = VK_FALSE;
    }

    
    void PipelineBuilder::DisableBlending() noexcept
    {
        m_colorBlendAttachment.colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT | 
            VK_COLOR_COMPONENT_G_BIT | 
            VK_COLOR_COMPONENT_B_BIT | 
            VK_COLOR_COMPONENT_A_BIT;
        
        m_colorBlendAttachment.blendEnable = VK_FALSE;
    }


    void PipelineBuilder::DisableDepthTest() noexcept
    {
        m_depthStencil.depthTestEnable = VK_FALSE;
        m_depthStencil.depthWriteEnable = VK_FALSE;
        m_depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
        m_depthStencil.depthBoundsTestEnable = VK_FALSE;
        m_depthStencil.stencilTestEnable = VK_FALSE;
        m_depthStencil.front = {};
        m_depthStencil.back = {};
        m_depthStencil.minDepthBounds = 0.f;
        m_depthStencil.maxDepthBounds = 1.f;
    }


    void PipelineBuilder::SetLayout(VkPipelineLayout pLayout) noexcept
    {
        m_pipelineLayout = pLayout;
    }


    bool LoadShaderModule(const std::filesystem::path& filepath, VkDevice pDevice, VkShaderModule& pOutModule) noexcept
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            return false;
        }

        const size_t fileSize = (size_t)file.tellg();

        std::vector<uint32_t> buffer((fileSize + sizeof(uint32_t) - 1) / sizeof(uint32_t));

        file.seekg(0);
        file.read((char*)buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = buffer.size() * sizeof(uint32_t),
            .pCode = buffer.data(),
        };

        VkShaderModule pShaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(pDevice, &createInfo, nullptr, &pShaderModule) != VK_SUCCESS) {
            return false;
        }

        pOutModule = pShaderModule;
        
        return true;
    }
}
