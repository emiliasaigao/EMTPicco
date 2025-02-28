#include "runtime/function/render/rhi/vulkan/vulkan_rhi.h"
#include "runtime/function/render/rhi/vulkan/vulkan_util.h"
#include "runtime/function/render/render_helper.h"
#include "runtime/function/render/passes/ssao_generate_pass.h"

#include <ssao_generate_vert.h>
#include <hbao_generate_frag.h>
#include <ssao_generate_frag.h>

#include <stdexcept>

namespace Piccolo
{
    void SSAOGeneratePass::initialize(const RenderPassInitInfo* init_info)
    {
        RenderPass::initialize(nullptr);

        const SSAOGeneratePassInitInfo* _init_info = static_cast<const SSAOGeneratePassInitInfo*>(init_info);
        m_framebuffer.render_pass                  = _init_info->render_pass;

        setupDescriptorSetLayout();
        setupPipelines();
        setupDescriptorSet();
        m_normal_attachment = _init_info->normal_attachment;
        updateAfterFramebufferRecreate(_init_info->normal_attachment);
    }

    void SSAOGeneratePass::setupDescriptorSetLayout()
    {
        m_descriptor_infos.resize(1);

        VkDescriptorSetLayoutBinding ssao_generate_global_layout_bindings[4] = {};

        VkDescriptorSetLayoutBinding& ssao_generate_global_layout_perframe_storage_buffer_binding =
            ssao_generate_global_layout_bindings[0];
        ssao_generate_global_layout_perframe_storage_buffer_binding.binding = 0;
        ssao_generate_global_layout_perframe_storage_buffer_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        ssao_generate_global_layout_perframe_storage_buffer_binding.descriptorCount = 1;
        ssao_generate_global_layout_perframe_storage_buffer_binding.stageFlags =
            VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding& ssao_generate_global_layout_normal_attachment_binding =
            ssao_generate_global_layout_bindings[1];
        ssao_generate_global_layout_normal_attachment_binding.binding         = 1;
        ssao_generate_global_layout_normal_attachment_binding.descriptorType  = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        ssao_generate_global_layout_normal_attachment_binding.descriptorCount = 1;
        ssao_generate_global_layout_normal_attachment_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding& ssao_generate_global_layout_depth_attachment_binding =
            ssao_generate_global_layout_bindings[2];
        ssao_generate_global_layout_depth_attachment_binding.binding         = 2;
        ssao_generate_global_layout_depth_attachment_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssao_generate_global_layout_depth_attachment_binding.descriptorCount = 1;
        ssao_generate_global_layout_depth_attachment_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // noiseMap
        VkDescriptorSetLayoutBinding& ssao_generate_global_layout_noiseMap_binding = 
            ssao_generate_global_layout_bindings[3];
        ssao_generate_global_layout_noiseMap_binding.binding                       = 3;
        ssao_generate_global_layout_noiseMap_binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssao_generate_global_layout_noiseMap_binding.descriptorCount = 1;
        ssao_generate_global_layout_noiseMap_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ssao_generate_global_layout_create_info;
        ssao_generate_global_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ssao_generate_global_layout_create_info.pNext = NULL;
        ssao_generate_global_layout_create_info.flags = 0;
        ssao_generate_global_layout_create_info.bindingCount =
            sizeof(ssao_generate_global_layout_bindings) / sizeof(ssao_generate_global_layout_bindings[0]);
        ssao_generate_global_layout_create_info.pBindings = ssao_generate_global_layout_bindings;

        if (VK_SUCCESS !=
            vkCreateDescriptorSetLayout(
                m_vulkan_rhi->m_device, &ssao_generate_global_layout_create_info, NULL, &m_descriptor_infos[0].layout))
        {
            throw std::runtime_error("create post process global layout");
        }
    }

    void SSAOGeneratePass::preparePassData(std::shared_ptr<RenderResourceBase> render_resource)
    {
        const RenderResource* vulkan_resource = static_cast<const RenderResource*>(render_resource.get());
        if (vulkan_resource)
        {
            m_ssao_generate_perframe_storage_buffer_object =
                vulkan_resource->m_ssao_generate_perframe_storage_buffer_object;
        }

    }

    void SSAOGeneratePass::setupPipelines()
    {
        m_render_pipelines.resize(1);

        VkDescriptorSetLayout      descriptorset_layouts[1] = {m_descriptor_infos[0].layout};
        VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
        pipeline_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts    = descriptorset_layouts;

        if (vkCreatePipelineLayout(
                m_vulkan_rhi->m_device, &pipeline_layout_create_info, nullptr, &m_render_pipelines[0].layout) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("create post process pipeline layout");
        }

        VkShaderModule vert_shader_module = VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, SSAO_GENERATE_VERT);
        VkShaderModule frag_shader_module = VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, HBAO_GENERATE_FRAG);

        VkPipelineShaderStageCreateInfo vert_pipeline_shader_stage_create_info {};
        vert_pipeline_shader_stage_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_pipeline_shader_stage_create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vert_pipeline_shader_stage_create_info.module = vert_shader_module;
        vert_pipeline_shader_stage_create_info.pName  = "main";

        VkPipelineShaderStageCreateInfo frag_pipeline_shader_stage_create_info {};
        frag_pipeline_shader_stage_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_pipeline_shader_stage_create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_pipeline_shader_stage_create_info.module = frag_shader_module;
        frag_pipeline_shader_stage_create_info.pName  = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {vert_pipeline_shader_stage_create_info,
                                                           frag_pipeline_shader_stage_create_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
        vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_create_info.vertexBindingDescriptionCount   = 0;
        vertex_input_state_create_info.pVertexBindingDescriptions      = NULL;
        vertex_input_state_create_info.vertexAttributeDescriptionCount = 0;
        vertex_input_state_create_info.pVertexAttributeDescriptions    = NULL;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
        input_assembly_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_create_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state_create_info {};
        viewport_state_create_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_create_info.viewportCount = 1;
        viewport_state_create_info.pViewports    = &m_vulkan_rhi->m_viewport;
        viewport_state_create_info.scissorCount  = 1;
        viewport_state_create_info.pScissors     = &m_vulkan_rhi->m_scissor;

        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info {};
        rasterization_state_create_info.sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state_create_info.depthClampEnable = VK_FALSE;
        rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
        rasterization_state_create_info.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterization_state_create_info.lineWidth               = 1.0f;
        rasterization_state_create_info.cullMode                = VK_CULL_MODE_BACK_BIT;
        rasterization_state_create_info.frontFace               = VK_FRONT_FACE_CLOCKWISE;
        rasterization_state_create_info.depthBiasEnable         = VK_FALSE;
        rasterization_state_create_info.depthBiasConstantFactor = 0.0f;
        rasterization_state_create_info.depthBiasClamp          = 0.0f;
        rasterization_state_create_info.depthBiasSlopeFactor    = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisample_state_create_info {};
        multisample_state_create_info.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_create_info.sampleShadingEnable  = VK_FALSE;
        multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attachment_state {};
        color_blend_attachment_state.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment_state.blendEnable         = VK_FALSE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info {};
        color_blend_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable     = VK_FALSE;
        color_blend_state_create_info.logicOp           = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount   = 1;
        color_blend_state_create_info.pAttachments      = &color_blend_attachment_state;
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info {};
        depth_stencil_create_info.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_create_info.depthTestEnable       = VK_TRUE;
        depth_stencil_create_info.depthWriteEnable      = VK_TRUE;
        depth_stencil_create_info.depthCompareOp        = VK_COMPARE_OP_LESS;
        depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_create_info.stencilTestEnable     = VK_FALSE;

        VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamic_state_create_info {};
        dynamic_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_create_info.dynamicStateCount = 2;
        dynamic_state_create_info.pDynamicStates    = dynamic_states;

        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = shader_stages;
        pipelineInfo.pVertexInputState   = &vertex_input_state_create_info;
        pipelineInfo.pInputAssemblyState = &input_assembly_create_info;
        pipelineInfo.pViewportState      = &viewport_state_create_info;
        pipelineInfo.pRasterizationState = &rasterization_state_create_info;
        pipelineInfo.pMultisampleState   = &multisample_state_create_info;
        pipelineInfo.pColorBlendState    = &color_blend_state_create_info;
        pipelineInfo.pDepthStencilState  = &depth_stencil_create_info;
        pipelineInfo.layout              = m_render_pipelines[0].layout;
        pipelineInfo.renderPass          = m_framebuffer.render_pass;
        pipelineInfo.subpass             = _main_camera_subpass_ssao_generate;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
        pipelineInfo.pDynamicState       = &dynamic_state_create_info;

        if (vkCreateGraphicsPipelines(
                m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_render_pipelines[0].pipeline) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("create post process graphics pipeline");
        }

        vkDestroyShaderModule(m_vulkan_rhi->m_device, vert_shader_module, nullptr);
        vkDestroyShaderModule(m_vulkan_rhi->m_device, frag_shader_module, nullptr);
    }

    void SSAOGeneratePass::setupDescriptorSet()
    {
        VkDescriptorSetAllocateInfo post_process_global_descriptor_set_alloc_info;
        post_process_global_descriptor_set_alloc_info.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        post_process_global_descriptor_set_alloc_info.pNext          = NULL;
        post_process_global_descriptor_set_alloc_info.descriptorPool = m_vulkan_rhi->m_descriptor_pool;
        post_process_global_descriptor_set_alloc_info.descriptorSetCount = 1;
        post_process_global_descriptor_set_alloc_info.pSetLayouts        = &m_descriptor_infos[0].layout;

        if (VK_SUCCESS != vkAllocateDescriptorSets(m_vulkan_rhi->m_device,
                                                   &post_process_global_descriptor_set_alloc_info,
                                                   &m_descriptor_infos[0].descriptor_set))
        {
            throw std::runtime_error("allocate post process global descriptor set");
        }
    }

    void SSAOGeneratePass::updateAfterFramebufferRecreate(VkImageView normal_attachment)
    {
        VkSampler                  nearest_sampler;
        VkPhysicalDeviceProperties physical_device_properties {};
        vkGetPhysicalDeviceProperties(m_vulkan_rhi->m_physical_device, &physical_device_properties);

        VkSamplerCreateInfo samplerInfo {};

        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = VK_FILTER_NEAREST;
        samplerInfo.minFilter               = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = physical_device_properties.limits.maxSamplerAnisotropy; // close :1.0f
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = VK_LOD_CLAMP_NONE;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        if (vkCreateSampler(m_vulkan_rhi->m_device, &samplerInfo, nullptr, &nearest_sampler) != VK_SUCCESS)
        {
            throw std::runtime_error("vk create sampler");
        }


        VkDescriptorImageInfo gbuffer_normal_input_attachment_info = {};
        gbuffer_normal_input_attachment_info.sampler               = nearest_sampler;
            //VulkanUtil::getOrCreateNearestSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
        gbuffer_normal_input_attachment_info.imageView   = normal_attachment;
        gbuffer_normal_input_attachment_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo depth_input_attachment_info = {};
        depth_input_attachment_info.sampler               = nearest_sampler;
            //VulkanUtil::getOrCreateNearestSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
        depth_input_attachment_info.imageView   = m_vulkan_rhi->m_depth_only_image_view;
        depth_input_attachment_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo ssao_noise_image_info = {};


        ssao_noise_image_info.sampler =
            VulkanUtil::getOrCreateNearestRepeatSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
        ssao_noise_image_info.imageView =
            m_global_render_resource->_ssao_noise_resource._ssao_noise_texture_image_view;
        ssao_noise_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // 1 UBO, 2 attachment, 1 noiseTexture
        VkWriteDescriptorSet ssao_generate_descriptor_writes_info[4];

        VkDescriptorBufferInfo ssao_generate_perframe_storage_buffer_info = {};
        // this offset plus dynamic_offset should not be greater than the size of the buffer
        ssao_generate_perframe_storage_buffer_info.offset = 0;
        // the range means the size actually used by the shader per draw call
        ssao_generate_perframe_storage_buffer_info.range =
            sizeof(SSAOGeneratePerframeStorageBufferObject);
        ssao_generate_perframe_storage_buffer_info.buffer =
            m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
        assert(ssao_generate_perframe_storage_buffer_info.range <
               m_global_render_resource->_storage_buffer._max_storage_buffer_range);

        VkWriteDescriptorSet& ssao_generate_perframe_storage_buffer_write_info  = ssao_generate_descriptor_writes_info[0];
        ssao_generate_perframe_storage_buffer_write_info.sType                  = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssao_generate_perframe_storage_buffer_write_info.pNext                  = NULL;
        ssao_generate_perframe_storage_buffer_write_info.dstSet                 = m_descriptor_infos[0].descriptor_set;
        ssao_generate_perframe_storage_buffer_write_info.dstBinding             = 0;
        ssao_generate_perframe_storage_buffer_write_info.dstArrayElement        = 0;
        ssao_generate_perframe_storage_buffer_write_info.descriptorType         = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        ssao_generate_perframe_storage_buffer_write_info.descriptorCount        = 1;
        ssao_generate_perframe_storage_buffer_write_info.pBufferInfo            = &ssao_generate_perframe_storage_buffer_info;

        VkWriteDescriptorSet& gbuffer_normal_descriptor_input_attachment_write_info = ssao_generate_descriptor_writes_info[1];
        gbuffer_normal_descriptor_input_attachment_write_info.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        gbuffer_normal_descriptor_input_attachment_write_info.pNext                 = NULL;
        gbuffer_normal_descriptor_input_attachment_write_info.dstSet                = m_descriptor_infos[0].descriptor_set;
        gbuffer_normal_descriptor_input_attachment_write_info.dstBinding      = 1;
        gbuffer_normal_descriptor_input_attachment_write_info.dstArrayElement = 0;
        gbuffer_normal_descriptor_input_attachment_write_info.descriptorType  = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        gbuffer_normal_descriptor_input_attachment_write_info.descriptorCount = 1;
        gbuffer_normal_descriptor_input_attachment_write_info.pImageInfo      = &gbuffer_normal_input_attachment_info;

        VkWriteDescriptorSet& depth_descriptor_input_attachment_write_info = ssao_generate_descriptor_writes_info[2];
        depth_descriptor_input_attachment_write_info.sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        depth_descriptor_input_attachment_write_info.pNext      = NULL;
        depth_descriptor_input_attachment_write_info.dstSet     = m_descriptor_infos[0].descriptor_set;
        depth_descriptor_input_attachment_write_info.dstBinding = 2;
        depth_descriptor_input_attachment_write_info.dstArrayElement = 0;
        depth_descriptor_input_attachment_write_info.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        depth_descriptor_input_attachment_write_info.descriptorCount = 1;
        depth_descriptor_input_attachment_write_info.pImageInfo      = &depth_input_attachment_info;

        VkWriteDescriptorSet& ssao_noise_texture_write_info = ssao_generate_descriptor_writes_info[3];
        ssao_noise_texture_write_info.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssao_noise_texture_write_info.pNext                 = NULL;
        ssao_noise_texture_write_info.dstSet                = m_descriptor_infos[0].descriptor_set;
        ssao_noise_texture_write_info.dstBinding            = 3;
        ssao_noise_texture_write_info.dstArrayElement       = 0;
        ssao_noise_texture_write_info.descriptorType        = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssao_noise_texture_write_info.descriptorCount       = 1;
        ssao_noise_texture_write_info.pImageInfo            = &ssao_noise_image_info;

        vkUpdateDescriptorSets(m_vulkan_rhi->m_device,
                            sizeof(ssao_generate_descriptor_writes_info) /
                            sizeof(ssao_generate_descriptor_writes_info[0]),
                            ssao_generate_descriptor_writes_info,
                            0,
                            NULL);
    }

    void SSAOGeneratePass::draw()
    {

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            VkDebugUtilsLabelEXT label_info = {
                VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "SSAO Generate", {1.0f, 1.0f, 1.0f, 1.0f}};
            m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_vulkan_rhi->m_current_command_buffer, &label_info);
        }

        // perframe storage buffer
        uint32_t perframe_dynamic_offset =
            roundUp(m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                    m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
        m_global_render_resource->_storage_buffer._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
            perframe_dynamic_offset + sizeof(SSAOGeneratePerframeStorageBufferObject);
        assert(m_global_render_resource->_storage_buffer
                   ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
               (m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

            (*reinterpret_cast<SSAOGeneratePerframeStorageBufferObject*>(
                reinterpret_cast<uintptr_t>(
                    m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                perframe_dynamic_offset)) = m_ssao_generate_perframe_storage_buffer_object;

        

        m_vulkan_rhi->m_vk_cmd_bind_pipeline(
            m_vulkan_rhi->m_current_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_render_pipelines[0].pipeline);

        VkViewport viewport {};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = m_vulkan_rhi->m_swapchain_extent.width;
        viewport.height   = m_vulkan_rhi->m_swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = {m_vulkan_rhi->m_swapchain_extent.width, m_vulkan_rhi->m_swapchain_extent.height};
        m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
        m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);
        m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                    m_render_pipelines[0].layout,
                                                    0,
                                                    1,
                                                    &m_descriptor_infos[0].descriptor_set,
                                                    1,
                                                    &perframe_dynamic_offset);

        vkCmdDraw(m_vulkan_rhi->m_current_command_buffer, 3, 1, 0, 0);

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            m_vulkan_rhi->m_vk_cmd_end_debug_utils_label_ext(m_vulkan_rhi->m_current_command_buffer);
        }

    }
} // namespace Piccolo
