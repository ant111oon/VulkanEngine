#pragma once

#include "vk_types.h"

#include <filesystem>
#include <vector>


namespace vkutil
{
    class PipelineBuilder final
    {
    public:
        PipelineBuilder();
        
        void Clear() noexcept;

        void SetShaders(VkShaderModule pVertexShader, VkShaderModule pPixelShader) noexcept;
        void SetInputTopology(VkPrimitiveTopology topology) noexcept;
        void SetPolygonMode(VkPolygonMode mode) noexcept;
        void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) noexcept;
        void SetDepthTest(bool depthWriteEnable, VkCompareOp cmpOp) noexcept;

        void SetColorAttachmentFormat(VkFormat format) noexcept;
        void SetDepthAttachmentFormat(VkFormat format) noexcept;

        void DisableMultisampling() noexcept;
        void DisableBlending() noexcept;
        void DisableDepthTest() noexcept;

        void SetLayout(VkPipelineLayout pLayout) noexcept;

        VkPipeline Build(VkDevice pDevice) noexcept;

    private:
        std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

        VkPipelineInputAssemblyStateCreateInfo m_inputAssembly;
        VkPipelineRasterizationStateCreateInfo m_rasterizer;
        VkPipelineColorBlendAttachmentState m_colorBlendAttachment;
        VkPipelineMultisampleStateCreateInfo m_multisampling;
        VkPipelineLayout m_pipelineLayout;
        VkPipelineDepthStencilStateCreateInfo m_depthStencil;
        VkPipelineRenderingCreateInfo m_renderInfo;
        VkFormat m_colorAttachmentFormat;
    };

    bool LoadShaderModule(const std::filesystem::path& filepath, VkDevice pDevice, VkShaderModule& pOutModule) noexcept;
}