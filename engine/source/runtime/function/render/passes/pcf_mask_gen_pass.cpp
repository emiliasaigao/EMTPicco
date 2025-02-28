#include "runtime/function/render/render_helper.h"
#include "runtime/function/render/render_mesh.h"
#include "runtime/function/render/rhi/vulkan/vulkan_rhi.h"
#include "runtime/function/render/rhi/vulkan/vulkan_util.h"

#include "runtime/function/render/passes/pcf_mask_gen_pass.h"

#include <pcf_mask_gen_vert.h>
#include <pcf_mask_gen_frag.h>

#include <stdexcept>

namespace Piccolo
{
    void PCFMaskGenPass::initialize(const RenderPassInitInfo* init_info)
    {
        RenderPass::initialize(nullptr);

        setupAttachments();
        setupRenderPass();
        setupFramebuffer();
        setupDescriptorSetLayout();
        setupPipelines();
        setupDescriptorSet();
    }

    void PCFMaskGenPass::preparePassData(std::shared_ptr<RenderResourceBase> render_resource)
    {
        const RenderResource* vulkan_resource = static_cast<const RenderResource*>(render_resource.get());
        if (vulkan_resource)
        {
            m_pcf_mask_gen_push_constants_object.inverse_proj_view_matrix =
                vulkan_resource->m_mesh_perframe_storage_buffer_object.proj_view_matrix.inverse();
            m_pcf_mask_gen_push_constants_object.light_proj_view_matrix = 
                vulkan_resource->m_mesh_perframe_storage_buffer_object.directional_light_proj_view;
        }
    }

    void PCFMaskGenPass::draw() { 
        
        VkRenderPassBeginInfo renderpass_begin_info {};
        renderpass_begin_info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_begin_info.renderPass        = m_framebuffer.render_pass;
        renderpass_begin_info.framebuffer       = m_framebuffer.framebuffer;
        renderpass_begin_info.renderArea.offset = {0, 0};
        renderpass_begin_info.renderArea.extent = {m_vulkan_rhi->m_swapchain_extent.width / 4,
                                                    m_vulkan_rhi->m_swapchain_extent.height / 4};

        VkClearValue clear_values[2];
        clear_values[0].color                 = {1.0f};
        clear_values[1].color                 = {1.0f};
        renderpass_begin_info.clearValueCount = (sizeof(clear_values) / sizeof(clear_values[0]));
        renderpass_begin_info.pClearValues    = clear_values;

        m_vulkan_rhi->m_vk_cmd_begin_render_pass(
            m_vulkan_rhi->m_current_command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            VkDebugUtilsLabelEXT label_info = {
                VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "PCF Mask", {1.0f, 1.0f, 1.0f, 1.0f}};
            m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_vulkan_rhi->m_current_command_buffer, &label_info);
        }
        
        m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_render_pipelines[0].pipeline);
        vkCmdPushConstants(m_vulkan_rhi->m_current_command_buffer, 
                           m_render_pipelines[0].layout, 
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(PCFMaskGenPushConstantsObject),
                           &m_pcf_mask_gen_push_constants_object);

        VkViewport viewport {};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = m_vulkan_rhi->m_swapchain_extent.width/4;
        viewport.height   = m_vulkan_rhi->m_swapchain_extent.height/4;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = {m_vulkan_rhi->m_swapchain_extent.width / 4, m_vulkan_rhi->m_swapchain_extent.height/4};
        m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
        m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);


        m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        m_render_pipelines[0].layout,
                        0,
                        1,
                        &m_descriptor_infos[0].descriptor_set,
                        0,
                        NULL);

        vkCmdDraw(m_vulkan_rhi->m_current_command_buffer, 3, 1, 0, 0);

        
        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            m_vulkan_rhi->m_vk_cmd_end_debug_utils_label_ext(m_vulkan_rhi->m_current_command_buffer);
        }

        m_vulkan_rhi->m_vk_cmd_end_render_pass(m_vulkan_rhi->m_current_command_buffer);
        
     }
    void PCFMaskGenPass::setupAttachments()
    {
        m_framebuffer.attachments.resize(2);
        m_framebuffer.attachments[0].format = VK_FORMAT_R16_SFLOAT;
        VulkanUtil::createImage(m_vulkan_rhi->m_physical_device,
                                m_vulkan_rhi->m_device,
                                m_vulkan_rhi->m_swapchain_extent.width / 4,
                                m_vulkan_rhi->m_swapchain_extent.height / 4,
                                m_framebuffer.attachments[0].format,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                m_framebuffer.attachments[0].image,
                                m_framebuffer.attachments[0].mem,
                                0,
                                1,
                                1);
        m_framebuffer.attachments[0].view = VulkanUtil::createImageView(m_vulkan_rhi->m_device,
                                                    m_framebuffer.attachments[0].image,
                                                    m_framebuffer.attachments[0].format,
                                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                                    VK_IMAGE_VIEW_TYPE_2D,
                                                    1,
                                                    1);
    }
    void PCFMaskGenPass::setupRenderPass()
    {
        VkAttachmentDescription attachments_dscp[1] = {};

        VkAttachmentDescription& pcf_mask_attachment_description = attachments_dscp[0];
        pcf_mask_attachment_description.format  = m_framebuffer.attachments[0].format;
        pcf_mask_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        pcf_mask_attachment_description.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        pcf_mask_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pcf_mask_attachment_description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pcf_mask_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pcf_mask_attachment_description.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        pcf_mask_attachment_description.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkSubpassDescription subpasses[1] = {};

        VkAttachmentReference pcf_mask_gen_pass_color_attachments_reference {};
        pcf_mask_gen_pass_color_attachments_reference.attachment =
            &pcf_mask_attachment_description - attachments_dscp;
        pcf_mask_gen_pass_color_attachments_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription& pcf_mask_gen_pass   = subpasses[0];
        pcf_mask_gen_pass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        pcf_mask_gen_pass.inputAttachmentCount    = 0;
        pcf_mask_gen_pass.pInputAttachments       = NULL;
        pcf_mask_gen_pass.colorAttachmentCount    = 1;
        pcf_mask_gen_pass.pColorAttachments       = &pcf_mask_gen_pass_color_attachments_reference;
        pcf_mask_gen_pass.pDepthStencilAttachment = NULL;
        pcf_mask_gen_pass.preserveAttachmentCount = 0;
        pcf_mask_gen_pass.pPreserveAttachments    = NULL;


        VkSubpassDependency dependencies[2] = {};

        VkSubpassDependency& pcf_mask_gen_pass_depend_on_pre_depth_pass = dependencies[0];
        pcf_mask_gen_pass_depend_on_pre_depth_pass.srcSubpass           = VK_SUBPASS_EXTERNAL;
        pcf_mask_gen_pass_depend_on_pre_depth_pass.dstSubpass           = 0;
        pcf_mask_gen_pass_depend_on_pre_depth_pass.srcStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        pcf_mask_gen_pass_depend_on_pre_depth_pass.dstStageMask         = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        pcf_mask_gen_pass_depend_on_pre_depth_pass.srcAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // STORE_OP_STORE
        pcf_mask_gen_pass_depend_on_pre_depth_pass.dstAccessMask        = VK_ACCESS_SHADER_READ_BIT;
        pcf_mask_gen_pass_depend_on_pre_depth_pass.dependencyFlags      = 0; // NOT BY REGION

        VkSubpassDependency& pcf_mask_gen_pass_dependency = dependencies[1];
        pcf_mask_gen_pass_dependency.srcSubpass           = 0;
        pcf_mask_gen_pass_dependency.dstSubpass           = VK_SUBPASS_EXTERNAL;
        pcf_mask_gen_pass_dependency.srcStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        pcf_mask_gen_pass_dependency.dstStageMask         = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        pcf_mask_gen_pass_dependency.srcAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // STORE_OP_STORE
        pcf_mask_gen_pass_dependency.dstAccessMask        = 0;
        pcf_mask_gen_pass_dependency.dependencyFlags      = 0; // NOT BY REGION

        VkRenderPassCreateInfo renderpass_create_info {};
        renderpass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderpass_create_info.attachmentCount = (sizeof(attachments_dscp) / sizeof(attachments_dscp[0]));
        renderpass_create_info.pAttachments    = attachments_dscp;
        renderpass_create_info.subpassCount    = (sizeof(subpasses) / sizeof(subpasses[0]));
        renderpass_create_info.pSubpasses      = subpasses;
        renderpass_create_info.dependencyCount = (sizeof(dependencies) / sizeof(dependencies[0]));
        renderpass_create_info.pDependencies   = dependencies;

        if (vkCreateRenderPass(m_vulkan_rhi->m_device, &renderpass_create_info, nullptr, &m_framebuffer.render_pass) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("create pre depth render pass");
        }
    }
    void PCFMaskGenPass::setupFramebuffer()
    {
        VkImageView attachments[1] = {m_framebuffer.attachments[0].view};

        VkFramebufferCreateInfo framebuffer_create_info {};
        framebuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.flags           = 0U;
        framebuffer_create_info.renderPass      = m_framebuffer.render_pass;
        framebuffer_create_info.attachmentCount = (sizeof(attachments) / sizeof(attachments[0]));
        framebuffer_create_info.pAttachments    = attachments;
        framebuffer_create_info.width           = m_vulkan_rhi->m_swapchain_extent.width / 4;
        framebuffer_create_info.height          = m_vulkan_rhi->m_swapchain_extent.height / 4;
        framebuffer_create_info.layers          = 1;

        if (vkCreateFramebuffer(
                m_vulkan_rhi->m_device, &framebuffer_create_info, nullptr, &m_framebuffer.framebuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("create directional light shadow framebuffer");
        }
    }
    void PCFMaskGenPass::setupDescriptorSetLayout()
    {
        m_descriptor_infos.resize(1);

        // pcf_mask_gen
        {
            VkDescriptorSetLayoutBinding pcf_mask_layout_bindings[2];

            VkDescriptorSetLayoutBinding& pcf_mask_layout_depth_texture_binding = pcf_mask_layout_bindings[0];
            pcf_mask_layout_depth_texture_binding.binding                       = 0;
            pcf_mask_layout_depth_texture_binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pcf_mask_layout_depth_texture_binding.descriptorCount    = 1;
            pcf_mask_layout_depth_texture_binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            pcf_mask_layout_depth_texture_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& pcf_mask_layout_direction_light_shadowMap_texture_binding =
                pcf_mask_layout_bindings[1];
            pcf_mask_layout_direction_light_shadowMap_texture_binding.binding = 1;
            pcf_mask_layout_direction_light_shadowMap_texture_binding.descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pcf_mask_layout_direction_light_shadowMap_texture_binding.descriptorCount    = 1;
            pcf_mask_layout_direction_light_shadowMap_texture_binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            pcf_mask_layout_direction_light_shadowMap_texture_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutCreateInfo pcf_mask_layout_create_info {};
            pcf_mask_layout_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            pcf_mask_layout_create_info.bindingCount = 2;
            pcf_mask_layout_create_info.pBindings    = pcf_mask_layout_bindings;

            if (VK_SUCCESS !=
                vkCreateDescriptorSetLayout(
                    m_vulkan_rhi->m_device, &pcf_mask_layout_create_info, NULL, &m_descriptor_infos[0].layout))
            {
                throw std::runtime_error("create mesh directional light shadow global layout");
            }
        }
  
    }
    void PCFMaskGenPass::setupPipelines()
    {
        m_render_pipelines.resize(1);

        // pcf_mask_gen
        {
            VkPushConstantRange pushConstantRange {};
            pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            pushConstantRange.offset     = 0;
            pushConstantRange.size       = sizeof(PCFMaskGenPushConstantsObject);

            VkDescriptorSetLayout      descriptorset_layouts[] = {m_descriptor_infos[0].layout};
            VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
            pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipeline_layout_create_info.setLayoutCount =
                (sizeof(descriptorset_layouts) / sizeof(descriptorset_layouts[0]));
            pipeline_layout_create_info.pSetLayouts            = descriptorset_layouts;
            pipeline_layout_create_info.pushConstantRangeCount = 1;
            pipeline_layout_create_info.pPushConstantRanges    = &pushConstantRange;

            if (vkCreatePipelineLayout(
                    m_vulkan_rhi->m_device, &pipeline_layout_create_info, nullptr, &m_render_pipelines[0].layout) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create deferred lighting pipeline layout");
            }

            VkShaderModule vert_shader_module =
                VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, PCF_MASK_GEN_VERT);
            VkShaderModule frag_shader_module =
                VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, PCF_MASK_GEN_FRAG);

            VkPipelineShaderStageCreateInfo vert_pipeline_shader_stage_create_info {};
            vert_pipeline_shader_stage_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vert_pipeline_shader_stage_create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            vert_pipeline_shader_stage_create_info.module = vert_shader_module;
            vert_pipeline_shader_stage_create_info.pName  = "main";
            // vert_pipeline_shader_stage_create_info.pSpecializationInfo

            VkPipelineShaderStageCreateInfo frag_pipeline_shader_stage_create_info {};
            frag_pipeline_shader_stage_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            frag_pipeline_shader_stage_create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            frag_pipeline_shader_stage_create_info.module = frag_shader_module;
            frag_pipeline_shader_stage_create_info.pName  = "main";

            VkPipelineShaderStageCreateInfo shader_stages[] = {vert_pipeline_shader_stage_create_info,
                                                               frag_pipeline_shader_stage_create_info};

            auto                                 vertex_binding_descriptions   = MeshVertex::getBindingDescriptions();
            auto                                 vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount = 0;
            vertex_input_state_create_info.pVertexBindingDescriptions    = NULL;
            vertex_input_state_create_info.vertexBindingDescriptionCount = 0;
            vertex_input_state_create_info.pVertexAttributeDescriptions  = NULL;

            VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
            input_assembly_create_info.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo viewport_state_create_info {};
            viewport_state_create_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state_create_info.viewportCount = 1;
            viewport_state_create_info.pViewports    = &m_vulkan_rhi->m_viewport;
            viewport_state_create_info.scissorCount  = 1;
            viewport_state_create_info.pScissors     = &m_vulkan_rhi->m_scissor;

            VkPipelineRasterizationStateCreateInfo rasterization_state_create_info {};
            rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization_state_create_info.depthClampEnable        = VK_FALSE;
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
            multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample_state_create_info.sampleShadingEnable  = VK_FALSE;
            multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState color_blend_attachments[1] = {};
            color_blend_attachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            color_blend_attachments[0].blendEnable         = VK_FALSE;
            color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachments[0].colorBlendOp        = VK_BLEND_OP_ADD;
            color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachments[0].alphaBlendOp        = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
            color_blend_state_create_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_state_create_info.logicOpEnable = VK_FALSE;
            color_blend_state_create_info.logicOp       = VK_LOGIC_OP_COPY;
            color_blend_state_create_info.attachmentCount =
                sizeof(color_blend_attachments) / sizeof(color_blend_attachments[0]);
            color_blend_state_create_info.pAttachments      = &color_blend_attachments[0];
            color_blend_state_create_info.blendConstants[0] = 0.0f;
            color_blend_state_create_info.blendConstants[1] = 0.0f;
            color_blend_state_create_info.blendConstants[2] = 0.0f;
            color_blend_state_create_info.blendConstants[3] = 0.0f;

            VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info {};
            depth_stencil_create_info.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil_create_info.depthTestEnable  = VK_FALSE;
            depth_stencil_create_info.depthWriteEnable = VK_FALSE;
            depth_stencil_create_info.depthCompareOp   = VK_COMPARE_OP_ALWAYS;
            depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
            depth_stencil_create_info.stencilTestEnable     = VK_FALSE;

            VkDynamicState                   dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
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
            pipelineInfo.subpass             = 0;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(m_vulkan_rhi->m_device,
                                          VK_NULL_HANDLE,
                                          1,
                                          &pipelineInfo,
                                          nullptr,
                                          &m_render_pipelines[0].pipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("create deferred lighting graphics pipeline");
            }

            vkDestroyShaderModule(m_vulkan_rhi->m_device, vert_shader_module, nullptr);
            vkDestroyShaderModule(m_vulkan_rhi->m_device, frag_shader_module, nullptr);
        }

        
    }

    void PCFMaskGenPass::setupDescriptorSet()
    {
        // pcf_mask_gen
        {
            VkDescriptorSetAllocateInfo pcf_mask_descriptor_set_alloc_info;
            pcf_mask_descriptor_set_alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            pcf_mask_descriptor_set_alloc_info.pNext              = NULL;
            pcf_mask_descriptor_set_alloc_info.descriptorPool     = m_vulkan_rhi->m_descriptor_pool;
            pcf_mask_descriptor_set_alloc_info.descriptorSetCount = 1;
            pcf_mask_descriptor_set_alloc_info.pSetLayouts        = &m_descriptor_infos[0].layout;

            if (VK_SUCCESS != vkAllocateDescriptorSets(m_vulkan_rhi->m_device,
                                                       &pcf_mask_descriptor_set_alloc_info,
                                                       &m_descriptor_infos[0].descriptor_set))
            {
                throw std::runtime_error("allocate pcf_mask descriptor set");
            }

            VkDescriptorImageInfo depth_input_attachment_info = {};
            depth_input_attachment_info.sampler =
                VulkanUtil::getOrCreateNearestSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
            depth_input_attachment_info.imageView   = m_vulkan_rhi->m_depth_only_image_view;
            depth_input_attachment_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo directional_light_shadow_texture_image_info {};
            directional_light_shadow_texture_image_info.sampler =
                VulkanUtil::getOrCreateNearestSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
            directional_light_shadow_texture_image_info.imageView   = m_directional_light_shadow_color_image_view;
            directional_light_shadow_texture_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet pcf_mask_descriptor_writes_info[2];

            pcf_mask_descriptor_writes_info[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            pcf_mask_descriptor_writes_info[0].pNext           = NULL;
            pcf_mask_descriptor_writes_info[0].dstSet          = m_descriptor_infos[0].descriptor_set;
            pcf_mask_descriptor_writes_info[0].dstBinding      = 0;
            pcf_mask_descriptor_writes_info[0].dstArrayElement = 0;
            pcf_mask_descriptor_writes_info[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pcf_mask_descriptor_writes_info[0].descriptorCount = 1;
            pcf_mask_descriptor_writes_info[0].pImageInfo      = &depth_input_attachment_info;

            pcf_mask_descriptor_writes_info[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            pcf_mask_descriptor_writes_info[1].pNext           = NULL;
            pcf_mask_descriptor_writes_info[1].dstSet          = m_descriptor_infos[0].descriptor_set;
            pcf_mask_descriptor_writes_info[1].dstBinding      = 1;
            pcf_mask_descriptor_writes_info[1].dstArrayElement = 0;
            pcf_mask_descriptor_writes_info[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pcf_mask_descriptor_writes_info[1].descriptorCount = 1;
            pcf_mask_descriptor_writes_info[1].pImageInfo      = &directional_light_shadow_texture_image_info;

            vkUpdateDescriptorSets(m_vulkan_rhi->m_device, 2, pcf_mask_descriptor_writes_info, 0, NULL);
        }

    }

} // namespace Piccolo
