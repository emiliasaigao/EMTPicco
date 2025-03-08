#include "runtime/function/render/rhi/vulkan/vulkan_rhi.h"
#include "runtime/function/render/rhi/vulkan/vulkan_util.h"
#include "runtime/function/render/render_helper.h"
#include "runtime/function/render/render_mesh.h"
#include "runtime/function/render/passes/nbr_pass.h"

#include <nijigen_core_frag.h>
#include <nijigen_core_vert.h>
#include <nijigen_outline_vert.h>
#include <nijigen_outline_frag.h>

#include <stdexcept>

namespace Piccolo
{
    void NBRPass::initialize(const RenderPassInitInfo* init_info)
    {
        RenderPass::initialize(nullptr);

        const NBRPassInitInfo* _init_info = static_cast<const NBRPassInitInfo*>(init_info);
        m_framebuffer.render_pass                  = _init_info->render_pass;

        setupDescriptorSetLayout();
        setupPipelines();
        setupDescriptorSet();
    }

    void NBRPass::setupDescriptorSetLayout()
    {
        m_descriptor_infos.resize(4);

        {
            VkDescriptorSetLayoutBinding mesh_global_layout_bindings[5];

            VkDescriptorSetLayoutBinding& mesh_global_layout_perframe_storage_buffer_binding =
                mesh_global_layout_bindings[0];
            mesh_global_layout_perframe_storage_buffer_binding.binding = 0;
            mesh_global_layout_perframe_storage_buffer_binding.descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_global_layout_perframe_storage_buffer_binding.descriptorCount = 1;
            mesh_global_layout_perframe_storage_buffer_binding.stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            mesh_global_layout_perframe_storage_buffer_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& mesh_global_layout_perdrawcall_storage_buffer_binding =
                mesh_global_layout_bindings[1];
            mesh_global_layout_perdrawcall_storage_buffer_binding.binding = 1;
            mesh_global_layout_perdrawcall_storage_buffer_binding.descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_global_layout_perdrawcall_storage_buffer_binding.descriptorCount    = 1;
            mesh_global_layout_perdrawcall_storage_buffer_binding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
            mesh_global_layout_perdrawcall_storage_buffer_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding =
                mesh_global_layout_bindings[2];
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.binding = 2;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.descriptorCount = 1;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& mesh_global_layout_irradiance_texture_binding = mesh_global_layout_bindings[3];
            mesh_global_layout_irradiance_texture_binding.binding                       = 3;
            mesh_global_layout_irradiance_texture_binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            mesh_global_layout_irradiance_texture_binding.descriptorCount    = 1;
            mesh_global_layout_irradiance_texture_binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            mesh_global_layout_irradiance_texture_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& mesh_global_layout_depth_texture_binding =
                mesh_global_layout_bindings[4];
            mesh_global_layout_depth_texture_binding              = mesh_global_layout_irradiance_texture_binding;
            mesh_global_layout_depth_texture_binding.binding = 4;


            VkDescriptorSetLayoutCreateInfo mesh_global_layout_create_info;
            mesh_global_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            mesh_global_layout_create_info.pNext = NULL;
            mesh_global_layout_create_info.flags = 0;
            mesh_global_layout_create_info.bindingCount =
                (sizeof(mesh_global_layout_bindings) / sizeof(mesh_global_layout_bindings[0]));
            mesh_global_layout_create_info.pBindings = mesh_global_layout_bindings;

            if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_vulkan_rhi->m_device,
                                                            &mesh_global_layout_create_info,
                                                            NULL,
                                                            &m_descriptor_infos[0].layout))
            {
                throw std::runtime_error("create mesh global layout");
            }
        }

        {
            VkDescriptorSetLayoutBinding mesh_mesh_layout_bindings[1];

            VkDescriptorSetLayoutBinding& mesh_mesh_layout_uniform_buffer_binding = mesh_mesh_layout_bindings[0];
            mesh_mesh_layout_uniform_buffer_binding.binding                       = 0;
            mesh_mesh_layout_uniform_buffer_binding.descriptorType                = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            mesh_mesh_layout_uniform_buffer_binding.descriptorCount               = 1;
            mesh_mesh_layout_uniform_buffer_binding.stageFlags                    = VK_SHADER_STAGE_VERTEX_BIT;
            mesh_mesh_layout_uniform_buffer_binding.pImmutableSamplers            = NULL;

            VkDescriptorSetLayoutCreateInfo mesh_mesh_layout_create_info {};
            mesh_mesh_layout_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            mesh_mesh_layout_create_info.bindingCount = 1;
            mesh_mesh_layout_create_info.pBindings    = mesh_mesh_layout_bindings;

            if (vkCreateDescriptorSetLayout(m_vulkan_rhi->m_device,
                                            &mesh_mesh_layout_create_info,
                                            NULL,
                                            &m_descriptor_infos[1].layout) != VK_SUCCESS)
            {
                throw std::runtime_error("create mesh mesh layout");
            }
        }

        {
            VkDescriptorSetLayoutBinding mesh_material_layout_bindings[6];

            // (set = 2, binding = 0 in fragment shader)
            VkDescriptorSetLayoutBinding& mesh_material_layout_uniform_buffer_binding =
                mesh_material_layout_bindings[0];
            mesh_material_layout_uniform_buffer_binding.binding            = 0;
            mesh_material_layout_uniform_buffer_binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            mesh_material_layout_uniform_buffer_binding.descriptorCount    = 1;
            mesh_material_layout_uniform_buffer_binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            mesh_material_layout_uniform_buffer_binding.pImmutableSamplers = nullptr;

            // (set = 2, binding = 1 in fragment shader)
            VkDescriptorSetLayoutBinding& mesh_material_layout_base_color_texture_binding =
                mesh_material_layout_bindings[1];
            mesh_material_layout_base_color_texture_binding.binding         = 1;
            mesh_material_layout_base_color_texture_binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            mesh_material_layout_base_color_texture_binding.descriptorCount = 1;
            mesh_material_layout_base_color_texture_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
            mesh_material_layout_base_color_texture_binding.pImmutableSamplers = nullptr;

            // (set = 2, binding = 2 in fragment shader)
            VkDescriptorSetLayoutBinding& mesh_material_layout_light_map_texture_binding =
                mesh_material_layout_bindings[2];
            mesh_material_layout_light_map_texture_binding         = mesh_material_layout_base_color_texture_binding;
            mesh_material_layout_light_map_texture_binding.binding = 2;

            // (set = 2, binding = 3 in fragment shader)
            VkDescriptorSetLayoutBinding& mesh_material_layout_ramp_warm_texture_binding =
                mesh_material_layout_bindings[3];
            mesh_material_layout_ramp_warm_texture_binding         = mesh_material_layout_base_color_texture_binding;
            mesh_material_layout_ramp_warm_texture_binding.binding = 3;

            // (set = 2, binding = 4 in fragment shader)
            VkDescriptorSetLayoutBinding& mesh_material_layout_ramp_cool_texture_binding =
                mesh_material_layout_bindings[4];
            mesh_material_layout_ramp_cool_texture_binding         = mesh_material_layout_base_color_texture_binding;
            mesh_material_layout_ramp_cool_texture_binding.binding = 4;

            // (set = 2, binding = 5 in fragment shader)
            VkDescriptorSetLayoutBinding& mesh_material_layout_face_map_texture_binding =
                mesh_material_layout_bindings[5];
            mesh_material_layout_face_map_texture_binding         = mesh_material_layout_base_color_texture_binding;
            mesh_material_layout_face_map_texture_binding.binding = 5;

            VkDescriptorSetLayoutCreateInfo mesh_material_layout_create_info;
            mesh_material_layout_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            mesh_material_layout_create_info.pNext        = NULL;
            mesh_material_layout_create_info.flags        = 0;
            mesh_material_layout_create_info.bindingCount = 6;
            mesh_material_layout_create_info.pBindings    = mesh_material_layout_bindings;

            if (vkCreateDescriptorSetLayout(m_vulkan_rhi->m_device,
                                            &mesh_material_layout_create_info,
                                            nullptr,
                                            &m_descriptor_infos[2].layout) != VK_SUCCESS)

            {
                throw std::runtime_error("create nbr mesh material layout");
            }
        }

        // outline_descriptors
        {
            VkDescriptorSetLayoutBinding mesh_global_layout_bindings[3];

            VkDescriptorSetLayoutBinding& mesh_global_layout_perframe_storage_buffer_binding =
                mesh_global_layout_bindings[0];
            mesh_global_layout_perframe_storage_buffer_binding.binding = 0;
            mesh_global_layout_perframe_storage_buffer_binding.descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_global_layout_perframe_storage_buffer_binding.descriptorCount    = 1;
            mesh_global_layout_perframe_storage_buffer_binding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
            mesh_global_layout_perframe_storage_buffer_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& mesh_global_layout_perdrawcall_storage_buffer_binding =
                mesh_global_layout_bindings[1];
            mesh_global_layout_perdrawcall_storage_buffer_binding.binding = 1;
            mesh_global_layout_perdrawcall_storage_buffer_binding.descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_global_layout_perdrawcall_storage_buffer_binding.descriptorCount    = 1;
            mesh_global_layout_perdrawcall_storage_buffer_binding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
            mesh_global_layout_perdrawcall_storage_buffer_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding =
                mesh_global_layout_bindings[2];
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.binding = 2;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.descriptorCount = 1;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT;
            mesh_global_layout_per_drawcall_vertex_blending_storage_buffer_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutCreateInfo mesh_global_layout_create_info;
            mesh_global_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            mesh_global_layout_create_info.pNext = NULL;
            mesh_global_layout_create_info.flags = 0;
            mesh_global_layout_create_info.bindingCount =
                (sizeof(mesh_global_layout_bindings) / sizeof(mesh_global_layout_bindings[0]));
            mesh_global_layout_create_info.pBindings = mesh_global_layout_bindings;

            if (vkCreateDescriptorSetLayout(m_vulkan_rhi->m_device,
                                            &mesh_global_layout_create_info,
                                            nullptr,
                                            &m_descriptor_infos[3].layout) != VK_SUCCESS)

            {
                throw std::runtime_error("create nbr mesh material layout");
            }
        }
    }

    void NBRPass::preparePassData(std::shared_ptr<RenderResourceBase> render_resource)
    {
        const RenderResource* vulkan_resource = static_cast<const RenderResource*>(render_resource.get());
        if (vulkan_resource)
        {
            m_nbr_mesh_perframe_storage_buffer_object =
                vulkan_resource->m_nbr_mesh_perframe_storage_buffer_object;
            m_nbr_outline_mesh_perframe_storage_buffer_object =
                vulkan_resource->m_nbr_outline_mesh_perframe_storage_buffer_object;
        }

    }

    void NBRPass::setupPipelines()
    {
        m_render_pipelines.resize(_nbr_pipeline_type_count);

        VkDescriptorSetLayout      descriptorset_layouts[3] = {m_descriptor_infos[0].layout,
                                                               m_descriptor_infos[1].layout,
                                                               m_descriptor_infos[2].layout,};
        VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
        pipeline_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 3;
        pipeline_layout_create_info.pSetLayouts    = descriptorset_layouts;

        if (vkCreatePipelineLayout(
                m_vulkan_rhi->m_device, &pipeline_layout_create_info, nullptr, &m_render_pipelines[0].layout) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("create nbr pipeline layout");
        }

        VkShaderModule vert_shader_module = VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, NIJIGEN_CORE_VERT);
        VkShaderModule frag_shader_module = VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, NIJIGEN_CORE_FRAG);
        
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
        
        // _nbr_pipeline_type_eyes_and_eyebrows
        {
            auto                                 vertex_binding_descriptions   = MeshVertex::getBindingDescriptions();
            auto                                 vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_binding_descriptions.size();
            vertex_input_state_create_info.pVertexBindingDescriptions      = &vertex_binding_descriptions[0];
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state_create_info.pVertexAttributeDescriptions    = &vertex_attribute_descriptions[0];

            VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
            input_assembly_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_create_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
            rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
            depth_stencil_create_info.stencilTestEnable     = VK_TRUE;
            depth_stencil_create_info.front.failOp          = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.depthFailOp     = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.passOp          = VK_STENCIL_OP_REPLACE;
            depth_stencil_create_info.front.compareOp       = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.front.compareMask     = 0xFF;
            depth_stencil_create_info.front.writeMask       = 0xFF;
            depth_stencil_create_info.front.reference       = 2;
            depth_stencil_create_info.back.failOp           = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.passOp           = VK_STENCIL_OP_REPLACE;
            depth_stencil_create_info.back.compareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.back.compareMask      = 0xFF;
            depth_stencil_create_info.back.writeMask        = 0xFF;
            depth_stencil_create_info.back.reference        = 2;

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
            pipelineInfo.subpass             = _main_camera_subpass_forward_lighting;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(
                    m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_render_pipelines[_nbr_pipeline_type_eyes_and_eyebrows].pipeline) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr graphics pipeline");
            }
        }

        // _nbr_pipeline_type_face_and_mouth
        {
            auto                                 vertex_binding_descriptions   = MeshVertex::getBindingDescriptions();
            auto                                 vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_binding_descriptions.size();
            vertex_input_state_create_info.pVertexBindingDescriptions      = &vertex_binding_descriptions[0];
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state_create_info.pVertexAttributeDescriptions    = &vertex_attribute_descriptions[0];

            VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
            input_assembly_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_create_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
            rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
            depth_stencil_create_info.stencilTestEnable     = VK_TRUE;
            depth_stencil_create_info.front.failOp          = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.depthFailOp     = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.passOp          = VK_STENCIL_OP_ZERO;
            depth_stencil_create_info.front.compareOp       = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.front.compareMask     = 0xFF;
            depth_stencil_create_info.front.writeMask       = 0xFF;
            depth_stencil_create_info.front.reference       = 6;
            depth_stencil_create_info.back.failOp           = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.passOp           = VK_STENCIL_OP_ZERO;
            depth_stencil_create_info.back.compareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.back.compareMask      = 0xFF;
            depth_stencil_create_info.back.writeMask        = 0xFF;
            depth_stencil_create_info.back.reference        = 6;

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
            pipelineInfo.subpass             = _main_camera_subpass_forward_lighting;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(
                    m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_render_pipelines[_nbr_pipeline_type_face_and_mouth].pipeline) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr graphics pipeline");
            }
        }

        // _nbr_pipeline_type_body
        {
            auto                                 vertex_binding_descriptions   = MeshVertex::getBindingDescriptions();
            auto                                 vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_binding_descriptions.size();
            vertex_input_state_create_info.pVertexBindingDescriptions      = &vertex_binding_descriptions[0];
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state_create_info.pVertexAttributeDescriptions    = &vertex_attribute_descriptions[0];

            VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
            input_assembly_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_create_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
            rasterization_state_create_info.cullMode                = VK_CULL_MODE_NONE;
            rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
            depth_stencil_create_info.stencilTestEnable     = VK_TRUE;
            depth_stencil_create_info.front.failOp          = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.depthFailOp     = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.passOp          = VK_STENCIL_OP_ZERO;
            depth_stencil_create_info.front.compareOp       = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.front.compareMask     = 0xFF;
            depth_stencil_create_info.front.writeMask       = 0xFF;
            depth_stencil_create_info.front.reference       = 6;
            depth_stencil_create_info.back.failOp           = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.passOp           = VK_STENCIL_OP_ZERO;
            depth_stencil_create_info.back.compareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.back.compareMask      = 0xFF;
            depth_stencil_create_info.back.writeMask        = 0xFF;
            depth_stencil_create_info.back.reference        = 6;

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
            pipelineInfo.subpass             = _main_camera_subpass_forward_lighting;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(
                    m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_render_pipelines[_nbr_pipeline_type_body].pipeline) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr graphics pipeline");
            }
        }
        
        // _nbr_pipeline_type_hair
        {
            auto                                 vertex_binding_descriptions   = MeshVertex::getBindingDescriptions();
            auto                                 vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_binding_descriptions.size();
            vertex_input_state_create_info.pVertexBindingDescriptions      = &vertex_binding_descriptions[0];
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state_create_info.pVertexAttributeDescriptions    = &vertex_attribute_descriptions[0];

            VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
            input_assembly_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_create_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
            rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
            depth_stencil_create_info.stencilTestEnable     = VK_TRUE;
            depth_stencil_create_info.front.failOp          = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.depthFailOp     = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.passOp          = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.compareOp       = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.front.compareMask     = 0xFF;
            depth_stencil_create_info.front.writeMask       = 0xFF;
            depth_stencil_create_info.front.reference       = 1;
            depth_stencil_create_info.back.failOp           = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.passOp           = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.compareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL;
            depth_stencil_create_info.back.compareMask      = 0xFF;
            depth_stencil_create_info.back.writeMask        = 0xFF;
            depth_stencil_create_info.back.reference        = 1;

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
            pipelineInfo.subpass             = _main_camera_subpass_forward_lighting;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(
                    m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_render_pipelines[_nbr_pipeline_type_hair].pipeline) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr graphics pipeline");
            }
        }

        // _nbr_pipeline_type_hair_alpha
        {
            auto                                 vertex_binding_descriptions   = MeshVertex::getBindingDescriptions();
            auto                                 vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_binding_descriptions.size();
            vertex_input_state_create_info.pVertexBindingDescriptions      = &vertex_binding_descriptions[0];
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state_create_info.pVertexAttributeDescriptions    = &vertex_attribute_descriptions[0];

            VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
            input_assembly_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_create_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
            rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
            color_blend_attachment_state.blendEnable         = VK_TRUE;
            color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
            depth_stencil_create_info.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
            depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
            depth_stencil_create_info.stencilTestEnable     = VK_TRUE;
            depth_stencil_create_info.front.failOp          = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.depthFailOp     = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.passOp          = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.front.compareOp       = VK_COMPARE_OP_EQUAL;
            depth_stencil_create_info.front.compareMask     = 0xFF;
            depth_stencil_create_info.front.writeMask       = 0xFF;
            depth_stencil_create_info.front.reference       = 2;
            depth_stencil_create_info.back.failOp           = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.depthFailOp      = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.passOp           = VK_STENCIL_OP_KEEP;
            depth_stencil_create_info.back.compareOp        = VK_COMPARE_OP_EQUAL;
            depth_stencil_create_info.back.compareMask      = 0xFF;
            depth_stencil_create_info.back.writeMask        = 0xFF;
            depth_stencil_create_info.back.reference        = 2;

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
            pipelineInfo.subpass             = _main_camera_subpass_forward_lighting;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(
                    m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_render_pipelines[_nbr_pipeline_type_hair_alpha].pipeline) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr graphics pipeline");
            }
        }

        // _nbr_pipeline_type_eye_black
        {
            auto                                 vertex_binding_descriptions   = MeshVertex::getBindingDescriptions();
            auto                                 vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_binding_descriptions.size();
            vertex_input_state_create_info.pVertexBindingDescriptions      = &vertex_binding_descriptions[0];
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state_create_info.pVertexAttributeDescriptions    = &vertex_attribute_descriptions[0];

            VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info {};
            input_assembly_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_create_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
            rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
            color_blend_attachment_state.blendEnable         = VK_TRUE;
            color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
            depth_stencil_create_info.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
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
            pipelineInfo.subpass             = _main_camera_subpass_forward_lighting;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(
                    m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_render_pipelines[_nbr_pipeline_type_eye_black].pipeline) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr graphics pipeline");
            }
        }


        vkDestroyShaderModule(m_vulkan_rhi->m_device, vert_shader_module, nullptr);
        vkDestroyShaderModule(m_vulkan_rhi->m_device, frag_shader_module, nullptr);
        
        //outline_pipeline
        {
            VkDescriptorSetLayout descriptorset_layouts[3] = {
                m_descriptor_infos[3].layout,
                m_descriptor_infos[1].layout,
                m_descriptor_infos[2].layout,
            };

            VkPushConstantRange pushConstantRange {};
            pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
            pushConstantRange.offset     = 0;
            pushConstantRange.size       = sizeof(NBROutlinePushConstantObject);

            VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
            pipeline_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipeline_layout_create_info.setLayoutCount = 3;
            pipeline_layout_create_info.pSetLayouts    = descriptorset_layouts;
            pipeline_layout_create_info.pushConstantRangeCount = 1;
            pipeline_layout_create_info.pPushConstantRanges    = &pushConstantRange;

            if (vkCreatePipelineLayout(
                    m_vulkan_rhi->m_device, &pipeline_layout_create_info, nullptr, &m_render_pipelines[_nbr_pipeline_type_outline].layout) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr pipeline layout");
            }

            VkShaderModule vert_shader_module =
                VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, NIJIGEN_OUTLINE_VERT);
            VkShaderModule frag_shader_module =
                VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, NIJIGEN_OUTLINE_FRAG);

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
            
            auto vertex_binding_descriptions   = MeshVertex::getBindingDescriptionsWithVertexColor();
            auto vertex_attribute_descriptions = MeshVertex::getAttributeDescriptionsWithVertexColor();
            VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {};
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_binding_descriptions.size();
            vertex_input_state_create_info.pVertexBindingDescriptions      = &vertex_binding_descriptions[0];
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
            vertex_input_state_create_info.pVertexAttributeDescriptions    = &vertex_attribute_descriptions[0];

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
            rasterization_state_create_info.cullMode                = VK_CULL_MODE_FRONT_BIT;
            rasterization_state_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterization_state_create_info.depthBiasEnable         = VK_FALSE;
            rasterization_state_create_info.depthBiasConstantFactor = 0.0f;
            rasterization_state_create_info.depthBiasClamp          = 0.0f;
            rasterization_state_create_info.depthBiasSlopeFactor    = 0.0f;

            VkPipelineMultisampleStateCreateInfo multisample_state_create_info {};
            multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample_state_create_info.sampleShadingEnable  = VK_FALSE;
            multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState color_blend_attachment_state {};
            color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            color_blend_attachment_state.blendEnable         = VK_FALSE;
            color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
            color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo color_blend_state_create_info {};
            color_blend_state_create_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_state_create_info.logicOpEnable = VK_FALSE;
            color_blend_state_create_info.logicOp       = VK_LOGIC_OP_COPY;
            color_blend_state_create_info.attachmentCount   = 1;
            color_blend_state_create_info.pAttachments      = &color_blend_attachment_state;
            color_blend_state_create_info.blendConstants[0] = 0.0f;
            color_blend_state_create_info.blendConstants[1] = 0.0f;
            color_blend_state_create_info.blendConstants[2] = 0.0f;
            color_blend_state_create_info.blendConstants[3] = 0.0f;

            VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info {};
            depth_stencil_create_info.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil_create_info.depthTestEnable  = VK_TRUE;
            depth_stencil_create_info.depthWriteEnable = VK_TRUE;
            depth_stencil_create_info.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
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
            pipelineInfo.layout              = m_render_pipelines[_nbr_pipeline_type_outline].layout;
            pipelineInfo.renderPass          = m_framebuffer.render_pass;
            pipelineInfo.subpass             = _main_camera_subpass_forward_lighting;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(m_vulkan_rhi->m_device,
                                            VK_NULL_HANDLE,
                                            1,
                                            &pipelineInfo,
                                            nullptr,
                                            &m_render_pipelines[_nbr_pipeline_type_outline].pipeline) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create nbr graphics pipeline");
            }
            vkDestroyShaderModule(m_vulkan_rhi->m_device, vert_shader_module, nullptr);
            vkDestroyShaderModule(m_vulkan_rhi->m_device, frag_shader_module, nullptr);
        }
    }

    void NBRPass::setupDescriptorSet()
    {
        {
            VkDescriptorSetAllocateInfo mesh_global_descriptor_set_alloc_info;
            mesh_global_descriptor_set_alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            mesh_global_descriptor_set_alloc_info.pNext              = NULL;
            mesh_global_descriptor_set_alloc_info.descriptorPool     = m_vulkan_rhi->m_descriptor_pool;
            mesh_global_descriptor_set_alloc_info.descriptorSetCount = 1;
            mesh_global_descriptor_set_alloc_info.pSetLayouts        = &m_descriptor_infos[0].layout;
    
            if (VK_SUCCESS != vkAllocateDescriptorSets(m_vulkan_rhi->m_device,
                                                       &mesh_global_descriptor_set_alloc_info,
                                                       &m_descriptor_infos[0].descriptor_set))
            {
                throw std::runtime_error("allocate mesh global descriptor set");
            }
    
            VkDescriptorBufferInfo mesh_perframe_storage_buffer_info = {};
            // this offset plus dynamic_offset should not be greater than the size of the buffer
            mesh_perframe_storage_buffer_info.offset = 0;
            // the range means the size actually used by the shader per draw call
            mesh_perframe_storage_buffer_info.range  = sizeof(NBRMeshPerframeStorageBufferObject);
            mesh_perframe_storage_buffer_info.buffer = m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
            assert(mesh_perframe_storage_buffer_info.range <
                   m_global_render_resource->_storage_buffer._max_storage_buffer_range);
    
            VkDescriptorBufferInfo mesh_perdrawcall_storage_buffer_info = {};
            mesh_perdrawcall_storage_buffer_info.offset                 = 0;
            mesh_perdrawcall_storage_buffer_info.range                  = sizeof(MeshPerdrawcallStorageBufferObject);
            mesh_perdrawcall_storage_buffer_info.buffer =
                m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
            assert(mesh_perdrawcall_storage_buffer_info.range <
                   m_global_render_resource->_storage_buffer._max_storage_buffer_range);
    
            VkDescriptorBufferInfo mesh_per_drawcall_vertex_blending_storage_buffer_info = {};
            mesh_per_drawcall_vertex_blending_storage_buffer_info.offset                 = 0;
            mesh_per_drawcall_vertex_blending_storage_buffer_info.range =
                sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
            mesh_per_drawcall_vertex_blending_storage_buffer_info.buffer =
                m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
            assert(mesh_per_drawcall_vertex_blending_storage_buffer_info.range <
                   m_global_render_resource->_storage_buffer._max_storage_buffer_range);
    
            VkDescriptorImageInfo irradiance_texture_image_info = {};
            irradiance_texture_image_info.sampler = m_global_render_resource->_ibl_resource._irradiance_texture_sampler;
            irradiance_texture_image_info.imageView =
                m_global_render_resource->_ibl_resource._irradiance_texture_image_view;
            irradiance_texture_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    
            VkDescriptorImageInfo depth_texture_image_info {};
            depth_texture_image_info.sampler =
                VulkanUtil::getOrCreateNearestSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
            depth_texture_image_info.imageView   = m_depth_attachment;
            depth_texture_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
            VkWriteDescriptorSet mesh_descriptor_writes_info[5];
    
            mesh_descriptor_writes_info[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mesh_descriptor_writes_info[0].pNext           = NULL;
            mesh_descriptor_writes_info[0].dstSet          = m_descriptor_infos[0].descriptor_set;
            mesh_descriptor_writes_info[0].dstBinding      = 0;
            mesh_descriptor_writes_info[0].dstArrayElement = 0;
            mesh_descriptor_writes_info[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_descriptor_writes_info[0].descriptorCount = 1;
            mesh_descriptor_writes_info[0].pBufferInfo     = &mesh_perframe_storage_buffer_info;
    
            mesh_descriptor_writes_info[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mesh_descriptor_writes_info[1].pNext           = NULL;
            mesh_descriptor_writes_info[1].dstSet          = m_descriptor_infos[0].descriptor_set;
            mesh_descriptor_writes_info[1].dstBinding      = 1;
            mesh_descriptor_writes_info[1].dstArrayElement = 0;
            mesh_descriptor_writes_info[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_descriptor_writes_info[1].descriptorCount = 1;
            mesh_descriptor_writes_info[1].pBufferInfo     = &mesh_perdrawcall_storage_buffer_info;
    
            mesh_descriptor_writes_info[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mesh_descriptor_writes_info[2].pNext           = NULL;
            mesh_descriptor_writes_info[2].dstSet          = m_descriptor_infos[0].descriptor_set;
            mesh_descriptor_writes_info[2].dstBinding      = 2;
            mesh_descriptor_writes_info[2].dstArrayElement = 0;
            mesh_descriptor_writes_info[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_descriptor_writes_info[2].descriptorCount = 1;
            mesh_descriptor_writes_info[2].pBufferInfo     = &mesh_per_drawcall_vertex_blending_storage_buffer_info;
    
            mesh_descriptor_writes_info[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mesh_descriptor_writes_info[3].pNext           = NULL;
            mesh_descriptor_writes_info[3].dstSet          = m_descriptor_infos[0].descriptor_set;
            mesh_descriptor_writes_info[3].dstBinding      = 3;
            mesh_descriptor_writes_info[3].dstArrayElement = 0;
            mesh_descriptor_writes_info[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            mesh_descriptor_writes_info[3].descriptorCount = 1;
            mesh_descriptor_writes_info[3].pImageInfo      = &irradiance_texture_image_info;
    
            mesh_descriptor_writes_info[4]            = mesh_descriptor_writes_info[3];
            mesh_descriptor_writes_info[4].dstBinding = 4;
            mesh_descriptor_writes_info[4].pImageInfo = &depth_texture_image_info;
    
            vkUpdateDescriptorSets(m_vulkan_rhi->m_device,
                                   sizeof(mesh_descriptor_writes_info) / sizeof(mesh_descriptor_writes_info[0]),
                                   mesh_descriptor_writes_info,
                                   0,
                                   NULL);
        }

        {
            VkDescriptorSetAllocateInfo mesh_global_descriptor_set_alloc_info;
            mesh_global_descriptor_set_alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            mesh_global_descriptor_set_alloc_info.pNext              = NULL;
            mesh_global_descriptor_set_alloc_info.descriptorPool     = m_vulkan_rhi->m_descriptor_pool;
            mesh_global_descriptor_set_alloc_info.descriptorSetCount = 1;
            mesh_global_descriptor_set_alloc_info.pSetLayouts        = &m_descriptor_infos[3].layout;

            if (VK_SUCCESS != vkAllocateDescriptorSets(m_vulkan_rhi->m_device,
                                                       &mesh_global_descriptor_set_alloc_info,
                                                       &m_descriptor_infos[3].descriptor_set))
            {
                throw std::runtime_error("allocate mesh global descriptor set");
            }

            VkDescriptorBufferInfo mesh_perframe_storage_buffer_info = {};
            // this offset plus dynamic_offset should not be greater than the size of the buffer
            mesh_perframe_storage_buffer_info.offset = 0;
            // the range means the size actually used by the shader per draw call
            mesh_perframe_storage_buffer_info.range = sizeof(NBROutlineMeshPerframeStorageBufferObject);
            mesh_perframe_storage_buffer_info.buffer =
                m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
            assert(mesh_perframe_storage_buffer_info.range <
                   m_global_render_resource->_storage_buffer._max_storage_buffer_range);

            VkDescriptorBufferInfo mesh_perdrawcall_storage_buffer_info = {};
            mesh_perdrawcall_storage_buffer_info.offset                 = 0;
            mesh_perdrawcall_storage_buffer_info.range                  = sizeof(MeshPerdrawcallStorageBufferObject);
            mesh_perdrawcall_storage_buffer_info.buffer =
                m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
            assert(mesh_perdrawcall_storage_buffer_info.range <
                   m_global_render_resource->_storage_buffer._max_storage_buffer_range);

            VkDescriptorBufferInfo mesh_per_drawcall_vertex_blending_storage_buffer_info = {};
            mesh_per_drawcall_vertex_blending_storage_buffer_info.offset                 = 0;
            mesh_per_drawcall_vertex_blending_storage_buffer_info.range =
                sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
            mesh_per_drawcall_vertex_blending_storage_buffer_info.buffer =
                m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
            assert(mesh_per_drawcall_vertex_blending_storage_buffer_info.range <
                   m_global_render_resource->_storage_buffer._max_storage_buffer_range);

            VkWriteDescriptorSet mesh_descriptor_writes_info[3];

            mesh_descriptor_writes_info[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mesh_descriptor_writes_info[0].pNext           = NULL;
            mesh_descriptor_writes_info[0].dstSet          = m_descriptor_infos[3].descriptor_set;
            mesh_descriptor_writes_info[0].dstBinding      = 0;
            mesh_descriptor_writes_info[0].dstArrayElement = 0;
            mesh_descriptor_writes_info[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_descriptor_writes_info[0].descriptorCount = 1;
            mesh_descriptor_writes_info[0].pBufferInfo     = &mesh_perframe_storage_buffer_info;

            mesh_descriptor_writes_info[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mesh_descriptor_writes_info[1].pNext           = NULL;
            mesh_descriptor_writes_info[1].dstSet          = m_descriptor_infos[3].descriptor_set;
            mesh_descriptor_writes_info[1].dstBinding      = 1;
            mesh_descriptor_writes_info[1].dstArrayElement = 0;
            mesh_descriptor_writes_info[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_descriptor_writes_info[1].descriptorCount = 1;
            mesh_descriptor_writes_info[1].pBufferInfo     = &mesh_perdrawcall_storage_buffer_info;

            mesh_descriptor_writes_info[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mesh_descriptor_writes_info[2].pNext           = NULL;
            mesh_descriptor_writes_info[2].dstSet          = m_descriptor_infos[3].descriptor_set;
            mesh_descriptor_writes_info[2].dstBinding      = 2;
            mesh_descriptor_writes_info[2].dstArrayElement = 0;
            mesh_descriptor_writes_info[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            mesh_descriptor_writes_info[2].descriptorCount = 1;
            mesh_descriptor_writes_info[2].pBufferInfo     = &mesh_per_drawcall_vertex_blending_storage_buffer_info;

            vkUpdateDescriptorSets(m_vulkan_rhi->m_device,
                                   sizeof(mesh_descriptor_writes_info) / sizeof(mesh_descriptor_writes_info[0]),
                                   mesh_descriptor_writes_info,
                                   0,
                                   NULL);
        }
    }


    void NBRPass::draw()
    {

        struct MeshNode
        {
            VulkanNBRMaterial* material;
            VulkanMesh*        mesh;
            const Matrix4x4* model_matrix {nullptr};
            const Matrix4x4* joint_matrices {nullptr};
            uint32_t         joint_count {0};
        };

        std::vector<MeshNode> nbr_mesh_nodes(_nbr_mesh_count);
        // reorganize mesh
        for (RenderMeshNode& node : *(m_visiable_nodes.p_main_camera_visible_mesh_nodes))
        {
            if (!node.is_NBR_material)
                continue;
            assert(node.nbr_mesh_id != uint32_t(10000));

            MeshNode& temp    = nbr_mesh_nodes[node.nbr_mesh_id];
            temp.material     = node.ref_material_nbr;
            temp.mesh         = node.ref_mesh;
            temp.model_matrix = node.model_matrix;
            if (node.enable_vertex_blending)
            {
                temp.joint_matrices = node.joint_matrices;
                temp.joint_count    = node.joint_count;
            }
        }

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            VkDebugUtilsLabelEXT label_info = {
                VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "NBR Mesh", {1.0f, 1.0f, 1.0f, 1.0f}};
            m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_vulkan_rhi->m_current_command_buffer, &label_info);
        }

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

        // perframe storage buffer
        uint32_t perframe_dynamic_offset =
            roundUp(m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                    m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);

        m_global_render_resource->_storage_buffer._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
            perframe_dynamic_offset + sizeof(NBRMeshPerframeStorageBufferObject);
        assert(m_global_render_resource->_storage_buffer
                   ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
               (m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

        (*reinterpret_cast<NBRMeshPerframeStorageBufferObject*>(
            reinterpret_cast<uintptr_t>(
                m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
            perframe_dynamic_offset)) = m_nbr_mesh_perframe_storage_buffer_object;

        if (nbr_mesh_nodes[_nbr_mesh_eyes].material || nbr_mesh_nodes[_nbr_mesh_eyebrows].material)
        {
            m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 m_render_pipelines[_nbr_pipeline_type_eyes_and_eyebrows].pipeline);
            m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
            m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);

            if (nbr_mesh_nodes[_nbr_mesh_eyes].material)
            {
                VulkanNBRMaterial& eyes_material = *(nbr_mesh_nodes[_nbr_mesh_eyes].material);

                VulkanMesh& eyes_mesh      = *(nbr_mesh_nodes[_nbr_mesh_eyes].mesh);
                auto&       eyes_mesh_node = nbr_mesh_nodes[_nbr_mesh_eyes];

                uint32_t total_instance_count = 1;

                // bind per material
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            2,
                                                            1,
                                                            &eyes_material.material_descriptor_set,
                                                            0,
                                                            NULL);

                // bind per mesh
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            1,
                                                            1,
                                                            &eyes_mesh.mesh_vertex_blending_descriptor_set,
                                                            0,
                                                            NULL);

                VkBuffer     vertex_buffers[] = {eyes_mesh.mesh_vertex_position_buffer,
                                                 eyes_mesh.mesh_vertex_varying_enable_blending_buffer,
                                                 eyes_mesh.mesh_vertex_varying_buffer};
                VkDeviceSize offsets[]        = {0, 0, 0};
                m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                           0,
                                                           (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                           vertex_buffers,
                                                           offsets);
                m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                    m_vulkan_rhi->m_current_command_buffer, eyes_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

                uint32_t current_instance_count = 1;

                // per drawcall storage buffer
                uint32_t perdrawcall_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        perdrawcall_dynamic_offset));

                perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *eyes_mesh_node.model_matrix;
                perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                    eyes_mesh_node.joint_matrices ? 1.0 : -1.0;

                // per drawcall vertex blending storage buffer
                uint32_t per_drawcall_vertex_blending_dynamic_offset;

                if (eyes_mesh_node.joint_matrices)
                {
                    per_drawcall_vertex_blending_dynamic_offset =
                        roundUp(m_global_render_resource->_storage_buffer
                                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                                m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                        per_drawcall_vertex_blending_dynamic_offset +
                        sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                    assert(m_global_render_resource->_storage_buffer
                               ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                           (m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                            m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                    MeshPerdrawcallVertexBlendingStorageBufferObject&
                        per_drawcall_vertex_blending_storage_buffer_object =
                            (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                                reinterpret_cast<uintptr_t>(m_global_render_resource->_storage_buffer
                                                                ._global_upload_ringbuffer_memory_pointer) +
                                per_drawcall_vertex_blending_dynamic_offset));

                    for (uint32_t j = 0; j < eyes_mesh_node.joint_count; ++j)
                    {
                        per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                            eyes_mesh_node.joint_matrices[j];
                    }
                }
                else
                {
                    per_drawcall_vertex_blending_dynamic_offset = 0;
                }

                // bind perdrawcall
                uint32_t dynamic_offsets[3] = {
                    perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            0,
                                                            1,
                                                            &m_descriptor_infos[0].descriptor_set,
                                                            3,
                                                            dynamic_offsets);

                m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                    eyes_mesh.mesh_index_count,
                                                    current_instance_count,
                                                    0,
                                                    0,
                                                    0);
            }
            
            
            if (nbr_mesh_nodes[_nbr_mesh_eyebrows].material)
            {
                VulkanNBRMaterial& eyebrows_material = *(nbr_mesh_nodes[_nbr_mesh_eyebrows].material);

                VulkanMesh& eyebrows_mesh      = *(nbr_mesh_nodes[_nbr_mesh_eyebrows].mesh);
                auto&       eyebrows_mesh_node = nbr_mesh_nodes[_nbr_mesh_eyebrows];

                uint32_t total_instance_count = 1;

                // bind per material
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            2,
                                                            1,
                                                            &eyebrows_material.material_descriptor_set,
                                                            0,
                                                            NULL);

                // bind per mesh
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            1,
                                                            1,
                                                            &eyebrows_mesh.mesh_vertex_blending_descriptor_set,
                                                            0,
                                                            NULL);

                VkBuffer     vertex_buffers[] = {eyebrows_mesh.mesh_vertex_position_buffer,
                                                 eyebrows_mesh.mesh_vertex_varying_enable_blending_buffer,
                                                 eyebrows_mesh.mesh_vertex_varying_buffer};
                VkDeviceSize offsets[]        = {0, 0, 0};
                m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                           0,
                                                           (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                           vertex_buffers,
                                                           offsets);
                m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                    m_vulkan_rhi->m_current_command_buffer, eyebrows_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

                uint32_t current_instance_count = 1;

                // per drawcall storage buffer
                uint32_t perdrawcall_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        perdrawcall_dynamic_offset));

                perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *eyebrows_mesh_node.model_matrix;
                perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                    eyebrows_mesh_node.joint_matrices ? 1.0 : -1.0;

                // per drawcall vertex blending storage buffer
                uint32_t per_drawcall_vertex_blending_dynamic_offset;

                if (eyebrows_mesh_node.joint_matrices)
                {
                    per_drawcall_vertex_blending_dynamic_offset =
                        roundUp(m_global_render_resource->_storage_buffer
                                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                                m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                        per_drawcall_vertex_blending_dynamic_offset +
                        sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                    assert(m_global_render_resource->_storage_buffer
                               ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                           (m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                            m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                    MeshPerdrawcallVertexBlendingStorageBufferObject&
                        per_drawcall_vertex_blending_storage_buffer_object =
                            (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                                reinterpret_cast<uintptr_t>(m_global_render_resource->_storage_buffer
                                                                ._global_upload_ringbuffer_memory_pointer) +
                                per_drawcall_vertex_blending_dynamic_offset));

                    for (uint32_t j = 0; j < eyebrows_mesh_node.joint_count; ++j)
                    {
                        per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                            eyebrows_mesh_node.joint_matrices[j];
                    }
                }
                else
                {
                    per_drawcall_vertex_blending_dynamic_offset = 0;
                }

                // bind perdrawcall
                uint32_t dynamic_offsets[3] = {
                    perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            0,
                                                            1,
                                                            &m_descriptor_infos[0].descriptor_set,
                                                            3,
                                                            dynamic_offsets);

                m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                    eyebrows_mesh.mesh_index_count,
                                                    current_instance_count,
                                                    0,
                                                    0,
                                                    0);
            }
                    
        }

        if (nbr_mesh_nodes[_nbr_mesh_face].material || nbr_mesh_nodes[_nbr_mesh_mouth].material)
        {
            m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 m_render_pipelines[_nbr_pipeline_type_face_and_mouth].pipeline);
            m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
            m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);

            if (nbr_mesh_nodes[_nbr_mesh_face].material)
            {
                VulkanNBRMaterial& face_material = *(nbr_mesh_nodes[_nbr_mesh_face].material);

                VulkanMesh& face_mesh      = *(nbr_mesh_nodes[_nbr_mesh_face].mesh);
                auto&       face_mesh_node = nbr_mesh_nodes[_nbr_mesh_face];

                uint32_t total_instance_count = 1;

                // bind per material
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            2,
                                                            1,
                                                            &face_material.material_descriptor_set,
                                                            0,
                                                            NULL);

                // bind per mesh
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            1,
                                                            1,
                                                            &face_mesh.mesh_vertex_blending_descriptor_set,
                                                            0,
                                                            NULL);

                VkBuffer     vertex_buffers[] = {face_mesh.mesh_vertex_position_buffer,
                                                 face_mesh.mesh_vertex_varying_enable_blending_buffer,
                                                 face_mesh.mesh_vertex_varying_buffer};
                VkDeviceSize offsets[]        = {0, 0, 0};
                m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                           0,
                                                           (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                           vertex_buffers,
                                                           offsets);
                m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                    m_vulkan_rhi->m_current_command_buffer, face_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

                uint32_t current_instance_count = 1;

                // per drawcall storage buffer
                uint32_t perdrawcall_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        perdrawcall_dynamic_offset));

                perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *face_mesh_node.model_matrix;
                perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                    face_mesh_node.joint_matrices ? 1.0 : -1.0;

                // per drawcall vertex blending storage buffer
                uint32_t per_drawcall_vertex_blending_dynamic_offset;

                if (face_mesh_node.joint_matrices)
                {
                    per_drawcall_vertex_blending_dynamic_offset =
                        roundUp(m_global_render_resource->_storage_buffer
                                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                                m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                        per_drawcall_vertex_blending_dynamic_offset +
                        sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                    assert(m_global_render_resource->_storage_buffer
                               ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                           (m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                            m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                    MeshPerdrawcallVertexBlendingStorageBufferObject&
                        per_drawcall_vertex_blending_storage_buffer_object =
                            (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                                reinterpret_cast<uintptr_t>(m_global_render_resource->_storage_buffer
                                                                ._global_upload_ringbuffer_memory_pointer) +
                                per_drawcall_vertex_blending_dynamic_offset));

                    for (uint32_t j = 0; j < face_mesh_node.joint_count; ++j)
                    {
                        per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                            face_mesh_node.joint_matrices[j];
                    }
                }
                else
                {
                    per_drawcall_vertex_blending_dynamic_offset = 0;
                }

                // bind perdrawcall
                uint32_t dynamic_offsets[3] = {
                    perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            0,
                                                            1,
                                                            &m_descriptor_infos[0].descriptor_set,
                                                            3,
                                                            dynamic_offsets);

                m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                    face_mesh.mesh_index_count,
                                                    current_instance_count,
                                                    0,
                                                    0,
                                                    0);
            }

            if (nbr_mesh_nodes[_nbr_mesh_mouth].material)
            {
                VulkanNBRMaterial& mouth_material = *(nbr_mesh_nodes[_nbr_mesh_mouth].material);

                VulkanMesh& mouth_mesh      = *(nbr_mesh_nodes[_nbr_mesh_mouth].mesh);
                auto&       mouth_mesh_node = nbr_mesh_nodes[_nbr_mesh_mouth];

                uint32_t total_instance_count = 1;

                // bind per material
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            2,
                                                            1,
                                                            &mouth_material.material_descriptor_set,
                                                            0,
                                                            NULL);

                // bind per mesh
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            1,
                                                            1,
                                                            &mouth_mesh.mesh_vertex_blending_descriptor_set,
                                                            0,
                                                            NULL);

                VkBuffer     vertex_buffers[] = {mouth_mesh.mesh_vertex_position_buffer,
                                                 mouth_mesh.mesh_vertex_varying_enable_blending_buffer,
                                                 mouth_mesh.mesh_vertex_varying_buffer};
                VkDeviceSize offsets[]        = {0, 0, 0};
                m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                           0,
                                                           (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                           vertex_buffers,
                                                           offsets);
                m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                    m_vulkan_rhi->m_current_command_buffer, mouth_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

                uint32_t current_instance_count = 1;

                // per drawcall storage buffer
                uint32_t perdrawcall_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        perdrawcall_dynamic_offset));

                perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *mouth_mesh_node.model_matrix;
                perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                    mouth_mesh_node.joint_matrices ? 1.0 : -1.0;

                // per drawcall vertex blending storage buffer
                uint32_t per_drawcall_vertex_blending_dynamic_offset;

                if (mouth_mesh_node.joint_matrices)
                {
                    per_drawcall_vertex_blending_dynamic_offset =
                        roundUp(m_global_render_resource->_storage_buffer
                                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                                m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                        per_drawcall_vertex_blending_dynamic_offset +
                        sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                    assert(m_global_render_resource->_storage_buffer
                               ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                           (m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                            m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                    MeshPerdrawcallVertexBlendingStorageBufferObject&
                        per_drawcall_vertex_blending_storage_buffer_object =
                            (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                                reinterpret_cast<uintptr_t>(m_global_render_resource->_storage_buffer
                                                                ._global_upload_ringbuffer_memory_pointer) +
                                per_drawcall_vertex_blending_dynamic_offset));

                    for (uint32_t j = 0; j < mouth_mesh_node.joint_count; ++j)
                    {
                        per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                            mouth_mesh_node.joint_matrices[j];
                    }
                }
                else
                {
                    per_drawcall_vertex_blending_dynamic_offset = 0;
                }

                // bind perdrawcall
                uint32_t dynamic_offsets[3] = {
                    perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[0].layout,
                                                            0,
                                                            1,
                                                            &m_descriptor_infos[0].descriptor_set,
                                                            3,
                                                            dynamic_offsets);

                m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                    mouth_mesh.mesh_index_count,
                                                    current_instance_count,
                                                    0,
                                                    0,
                                                    0);
            }

        }

        if (nbr_mesh_nodes[_nbr_mesh_body].material)
        {
            m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 m_render_pipelines[_nbr_pipeline_type_body].pipeline);
            m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
            m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);

            VulkanNBRMaterial& body_material = *(nbr_mesh_nodes[_nbr_mesh_body].material);

            VulkanMesh& body_mesh      = *(nbr_mesh_nodes[_nbr_mesh_body].mesh);
            auto&       body_mesh_node = nbr_mesh_nodes[_nbr_mesh_body];

            uint32_t total_instance_count = 1;

            // bind per material
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        2,
                                                        1,
                                                        &body_material.material_descriptor_set,
                                                        0,
                                                        NULL);

            // bind per mesh
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        1,
                                                        1,
                                                        &body_mesh.mesh_vertex_blending_descriptor_set,
                                                        0,
                                                        NULL);

            VkBuffer     vertex_buffers[] = {body_mesh.mesh_vertex_position_buffer,
                                             body_mesh.mesh_vertex_varying_enable_blending_buffer,
                                             body_mesh.mesh_vertex_varying_buffer};
            VkDeviceSize offsets[]        = {0, 0, 0};
            m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                       0,
                                                       (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                       vertex_buffers,
                                                       offsets);
            m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                m_vulkan_rhi->m_current_command_buffer, body_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

            uint32_t current_instance_count = 1;

            // per drawcall storage buffer
            uint32_t perdrawcall_dynamic_offset =
                roundUp(m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                        m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
            m_global_render_resource->_storage_buffer
                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
            assert(m_global_render_resource->_storage_buffer
                       ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                   (m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

            MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                    reinterpret_cast<uintptr_t>(
                        m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                    perdrawcall_dynamic_offset));

            perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *body_mesh_node.model_matrix;
            perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                body_mesh_node.joint_matrices ? 1.0 : -1.0;

            // per drawcall vertex blending storage buffer
            uint32_t per_drawcall_vertex_blending_dynamic_offset;

            if (body_mesh_node.joint_matrices)
            {
                per_drawcall_vertex_blending_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    per_drawcall_vertex_blending_dynamic_offset +
                    sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallVertexBlendingStorageBufferObject& per_drawcall_vertex_blending_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        per_drawcall_vertex_blending_dynamic_offset));

                for (uint32_t j = 0; j < body_mesh_node.joint_count; ++j)
                {
                    per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                        body_mesh_node.joint_matrices[j];
                }
            }
            else
            {
                per_drawcall_vertex_blending_dynamic_offset = 0;
            }

            // bind perdrawcall
            uint32_t dynamic_offsets[3] = {
                perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        0,
                                                        1,
                                                        &m_descriptor_infos[0].descriptor_set,
                                                        3,
                                                        dynamic_offsets);

            m_vulkan_rhi->m_vk_cmd_draw_indexed(
                m_vulkan_rhi->m_current_command_buffer, body_mesh.mesh_index_count, current_instance_count, 0, 0, 0);
        }

        if (nbr_mesh_nodes[_nbr_mesh_hair].material)
        {
            m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 m_render_pipelines[_nbr_pipeline_type_hair].pipeline);
            m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
            m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);

            VulkanNBRMaterial& hair_material = *(nbr_mesh_nodes[_nbr_mesh_hair].material);

            VulkanMesh& hair_mesh      = *(nbr_mesh_nodes[_nbr_mesh_hair].mesh);
            auto&       hair_mesh_node = nbr_mesh_nodes[_nbr_mesh_hair];

            uint32_t total_instance_count = 1;

            // bind per material
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        2,
                                                        1,
                                                        &hair_material.material_descriptor_set,
                                                        0,
                                                        NULL);

            // bind per mesh
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        1,
                                                        1,
                                                        &hair_mesh.mesh_vertex_blending_descriptor_set,
                                                        0,
                                                        NULL);

            VkBuffer     vertex_buffers[] = {hair_mesh.mesh_vertex_position_buffer,
                                             hair_mesh.mesh_vertex_varying_enable_blending_buffer,
                                             hair_mesh.mesh_vertex_varying_buffer};
            VkDeviceSize offsets[]        = {0, 0, 0};
            m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                       0,
                                                       (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                       vertex_buffers,
                                                       offsets);
            m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                m_vulkan_rhi->m_current_command_buffer, hair_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

            uint32_t current_instance_count = 1;

            // per drawcall storage buffer
            uint32_t perdrawcall_dynamic_offset =
                roundUp(m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                        m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
            m_global_render_resource->_storage_buffer
                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
            assert(m_global_render_resource->_storage_buffer
                       ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                   (m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

            MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                    reinterpret_cast<uintptr_t>(
                        m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                    perdrawcall_dynamic_offset));

            perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *hair_mesh_node.model_matrix;
            perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                hair_mesh_node.joint_matrices ? 1.0 : -1.0;

            // per drawcall vertex blending storage buffer
            uint32_t per_drawcall_vertex_blending_dynamic_offset;

            if (hair_mesh_node.joint_matrices)
            {
                per_drawcall_vertex_blending_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    per_drawcall_vertex_blending_dynamic_offset +
                    sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallVertexBlendingStorageBufferObject& per_drawcall_vertex_blending_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        per_drawcall_vertex_blending_dynamic_offset));

                for (uint32_t j = 0; j < hair_mesh_node.joint_count; ++j)
                {
                    per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                        hair_mesh_node.joint_matrices[j];
                }
            }
            else
            {
                per_drawcall_vertex_blending_dynamic_offset = 0;
            }

            // bind perdrawcall
            uint32_t dynamic_offsets[3] = {
                perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        0,
                                                        1,
                                                        &m_descriptor_infos[0].descriptor_set,
                                                        3,
                                                        dynamic_offsets);

            m_vulkan_rhi->m_vk_cmd_draw_indexed(
                m_vulkan_rhi->m_current_command_buffer, hair_mesh.mesh_index_count, current_instance_count, 0, 0, 0);
        }

        if (nbr_mesh_nodes[_nbr_mesh_hair].material)
        {
            m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 m_render_pipelines[_nbr_pipeline_type_hair_alpha].pipeline);
            m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
            m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);

            VulkanNBRMaterial& hair_material = *(nbr_mesh_nodes[_nbr_mesh_hair].material);

            VulkanMesh& hair_mesh      = *(nbr_mesh_nodes[_nbr_mesh_hair].mesh);
            auto&       hair_mesh_node = nbr_mesh_nodes[_nbr_mesh_hair];

            uint32_t total_instance_count = 1;

            // bind per material
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        2,
                                                        1,
                                                        &hair_material.material_descriptor_set,
                                                        0,
                                                        NULL);

            // bind per mesh
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        1,
                                                        1,
                                                        &hair_mesh.mesh_vertex_blending_descriptor_set,
                                                        0,
                                                        NULL);

            VkBuffer     vertex_buffers[] = {hair_mesh.mesh_vertex_position_buffer,
                                             hair_mesh.mesh_vertex_varying_enable_blending_buffer,
                                             hair_mesh.mesh_vertex_varying_buffer};
            VkDeviceSize offsets[]        = {0, 0, 0};
            m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                       0,
                                                       (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                       vertex_buffers,
                                                       offsets);
            m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                m_vulkan_rhi->m_current_command_buffer, hair_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

            uint32_t current_instance_count = 1;

            // per drawcall storage buffer
            uint32_t perdrawcall_dynamic_offset =
                roundUp(m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                        m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
            m_global_render_resource->_storage_buffer
                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
            assert(m_global_render_resource->_storage_buffer
                       ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                   (m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

            MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                    reinterpret_cast<uintptr_t>(
                        m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                    perdrawcall_dynamic_offset));

            perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *hair_mesh_node.model_matrix;
            perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                hair_mesh_node.joint_matrices ? 1.0 : -1.0;

            // per drawcall vertex blending storage buffer
            uint32_t per_drawcall_vertex_blending_dynamic_offset;

            if (hair_mesh_node.joint_matrices)
            {
                per_drawcall_vertex_blending_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    per_drawcall_vertex_blending_dynamic_offset +
                    sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallVertexBlendingStorageBufferObject& per_drawcall_vertex_blending_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        per_drawcall_vertex_blending_dynamic_offset));

                for (uint32_t j = 0; j < hair_mesh_node.joint_count; ++j)
                {
                    per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                        hair_mesh_node.joint_matrices[j];
                }
            }
            else
            {
                per_drawcall_vertex_blending_dynamic_offset = 0;
            }

            // bind perdrawcall
            uint32_t dynamic_offsets[3] = {
                perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        0,
                                                        1,
                                                        &m_descriptor_infos[0].descriptor_set,
                                                        3,
                                                        dynamic_offsets);

            m_vulkan_rhi->m_vk_cmd_draw_indexed(
                m_vulkan_rhi->m_current_command_buffer, hair_mesh.mesh_index_count, current_instance_count, 0, 0, 0);
        }

        if (nbr_mesh_nodes[_nbr_mesh_eye_black].material)
        {
            m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 m_render_pipelines[_nbr_pipeline_type_eye_black].pipeline);
            m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
            m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);

            VulkanNBRMaterial& eye_black_material = *(nbr_mesh_nodes[_nbr_mesh_eye_black].material);

            VulkanMesh& eye_black_mesh      = *(nbr_mesh_nodes[_nbr_mesh_eye_black].mesh);
            auto&       eye_black_mesh_node = nbr_mesh_nodes[_nbr_mesh_eye_black];

            uint32_t total_instance_count = 1;

            // bind per material
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        2,
                                                        1,
                                                        &eye_black_material.material_descriptor_set,
                                                        0,
                                                        NULL);

            // bind per mesh
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        1,
                                                        1,
                                                        &eye_black_mesh.mesh_vertex_blending_descriptor_set,
                                                        0,
                                                        NULL);

            VkBuffer     vertex_buffers[] = {eye_black_mesh.mesh_vertex_position_buffer,
                                             eye_black_mesh.mesh_vertex_varying_enable_blending_buffer,
                                             eye_black_mesh.mesh_vertex_varying_buffer};
            VkDeviceSize offsets[]        = {0, 0, 0};
            m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                       0,
                                                       (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                       vertex_buffers,
                                                       offsets);
            m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                m_vulkan_rhi->m_current_command_buffer, eye_black_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

            uint32_t current_instance_count = 1;

            // per drawcall storage buffer
            uint32_t perdrawcall_dynamic_offset =
                roundUp(m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                        m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
            m_global_render_resource->_storage_buffer
                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
            assert(m_global_render_resource->_storage_buffer
                       ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                   (m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

            MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                    reinterpret_cast<uintptr_t>(
                        m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                    perdrawcall_dynamic_offset));

            perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *eye_black_mesh_node.model_matrix;
            perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                eye_black_mesh_node.joint_matrices ? 1.0 : -1.0;

            // per drawcall vertex blending storage buffer
            uint32_t per_drawcall_vertex_blending_dynamic_offset;

            if (eye_black_mesh_node.joint_matrices)
            {
                per_drawcall_vertex_blending_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    per_drawcall_vertex_blending_dynamic_offset +
                    sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallVertexBlendingStorageBufferObject& per_drawcall_vertex_blending_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        per_drawcall_vertex_blending_dynamic_offset));

                for (uint32_t j = 0; j < eye_black_mesh_node.joint_count; ++j)
                {
                    per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                        eye_black_mesh_node.joint_matrices[j];
                }
            }
            else
            {
                per_drawcall_vertex_blending_dynamic_offset = 0;
            }

            // bind perdrawcall
            uint32_t dynamic_offsets[3] = {
                perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
            m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                        m_render_pipelines[0].layout,
                                                        0,
                                                        1,
                                                        &m_descriptor_infos[0].descriptor_set,
                                                        3,
                                                        dynamic_offsets);

            m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                eye_black_mesh.mesh_index_count,
                                                current_instance_count,
                                                0,
                                                0,
                                                0);
        }

        // draw outline
        if (nbr_mesh_nodes[_nbr_mesh_face].material || nbr_mesh_nodes[_nbr_mesh_body].material ||
            nbr_mesh_nodes[_nbr_mesh_hair].material)
        {
            m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_command_buffer,
                                                 VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 m_render_pipelines[_nbr_pipeline_type_outline].pipeline);
            m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_command_buffer, 0, 1, &viewport);
            m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_command_buffer, 0, 1, &scissor);

            // perframe storage buffer
            perframe_dynamic_offset =
                roundUp(m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                        m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);

            m_global_render_resource->_storage_buffer
                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                perframe_dynamic_offset + sizeof(NBROutlineMeshPerframeStorageBufferObject);
            assert(m_global_render_resource->_storage_buffer
                       ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                   (m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

            (*reinterpret_cast<NBROutlineMeshPerframeStorageBufferObject*>(
                reinterpret_cast<uintptr_t>(
                    m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                perframe_dynamic_offset)) = m_nbr_outline_mesh_perframe_storage_buffer_object;

            if (nbr_mesh_nodes[_nbr_mesh_face].material)
            {
                VulkanNBRMaterial& face_material = *(nbr_mesh_nodes[_nbr_mesh_face].material);

                VulkanMesh& face_mesh      = *(nbr_mesh_nodes[_nbr_mesh_face].mesh);
                auto&       face_mesh_node = nbr_mesh_nodes[_nbr_mesh_face];

                uint32_t total_instance_count = 1;

                m_nbr_outline_push_constant_object.outline_width = 0.8;
                m_nbr_outline_push_constant_object.outline_z_offset = 0.0;
                m_nbr_outline_push_constant_object.outline_gamma = 180.0;

                vkCmdPushConstants(m_vulkan_rhi->m_current_command_buffer,
                                   m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0,
                                   sizeof(NBROutlinePushConstantObject),
                                   &m_nbr_outline_push_constant_object);

                // bind per material
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                                            2,
                                                            1,
                                                            &face_material.material_descriptor_set,
                                                            0,
                                                            NULL);

                // bind per mesh
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                                            1,
                                                            1,
                                                            &face_mesh.mesh_vertex_blending_descriptor_set,
                                                            0,
                                                            NULL);

                VkBuffer     vertex_buffers[] = {face_mesh.mesh_vertex_position_buffer,
                                                 face_mesh.mesh_vertex_varying_enable_blending_buffer,
                                                 face_mesh.mesh_vertex_varying_buffer,
                                                 face_mesh.mesh_vertex_color_buffer};
                VkDeviceSize offsets[]        = {0, 0, 0, 0};
                m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                           0,
                                                           (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                           vertex_buffers,
                                                           offsets);
                m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                    m_vulkan_rhi->m_current_command_buffer, face_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

                uint32_t current_instance_count = 1;

                // per drawcall storage buffer
                uint32_t perdrawcall_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        perdrawcall_dynamic_offset));

                perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *face_mesh_node.model_matrix;
                perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                    face_mesh_node.joint_matrices ? 1.0 : -1.0;

                // per drawcall vertex blending storage buffer
                uint32_t per_drawcall_vertex_blending_dynamic_offset;

                if (face_mesh_node.joint_matrices)
                {
                    per_drawcall_vertex_blending_dynamic_offset =
                        roundUp(m_global_render_resource->_storage_buffer
                                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                                m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                        per_drawcall_vertex_blending_dynamic_offset +
                        sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                    assert(m_global_render_resource->_storage_buffer
                               ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                           (m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                            m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                    MeshPerdrawcallVertexBlendingStorageBufferObject&
                        per_drawcall_vertex_blending_storage_buffer_object =
                            (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                                reinterpret_cast<uintptr_t>(m_global_render_resource->_storage_buffer
                                                                ._global_upload_ringbuffer_memory_pointer) +
                                per_drawcall_vertex_blending_dynamic_offset));

                    for (uint32_t j = 0; j < face_mesh_node.joint_count; ++j)
                    {
                        per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                            face_mesh_node.joint_matrices[j];
                    }
                }
                else
                {
                    per_drawcall_vertex_blending_dynamic_offset = 0;
                }

                // bind perdrawcall
                uint32_t dynamic_offsets[3] = {
                    perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                                            0,
                                                            1,
                                                            &m_descriptor_infos[3].descriptor_set,
                                                            3,
                                                            dynamic_offsets);

                m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                    face_mesh.mesh_index_count,
                                                    current_instance_count,
                                                    0,
                                                    0,
                                                    0);
            }

            if (nbr_mesh_nodes[_nbr_mesh_body].material)
            {
                VulkanNBRMaterial& body_material = *(nbr_mesh_nodes[_nbr_mesh_body].material);

                VulkanMesh& body_mesh      = *(nbr_mesh_nodes[_nbr_mesh_body].mesh);
                auto&       body_mesh_node = nbr_mesh_nodes[_nbr_mesh_body];

                uint32_t total_instance_count = 1;

                m_nbr_outline_push_constant_object.outline_width    = 0.9;
                m_nbr_outline_push_constant_object.outline_z_offset = 0.0;
                m_nbr_outline_push_constant_object.outline_gamma    = 128.0;

                vkCmdPushConstants(m_vulkan_rhi->m_current_command_buffer,
                                   m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0,
                                   sizeof(NBROutlinePushConstantObject),
                                   &m_nbr_outline_push_constant_object);

                // bind per material
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                                            2,
                                                            1,
                                                            &body_material.material_descriptor_set,
                                                            0,
                                                            NULL);

                // bind per mesh
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                                            1,
                                                            1,
                                                            &body_mesh.mesh_vertex_blending_descriptor_set,
                                                            0,
                                                            NULL);

                VkBuffer     vertex_buffers[] = {body_mesh.mesh_vertex_position_buffer,
                                                 body_mesh.mesh_vertex_varying_enable_blending_buffer,
                                                 body_mesh.mesh_vertex_varying_buffer,
                                                 body_mesh.mesh_vertex_color_buffer};
                VkDeviceSize offsets[]        = {0, 0, 0, 0};
                m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                           0,
                                                           (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                           vertex_buffers,
                                                           offsets);
                m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                    m_vulkan_rhi->m_current_command_buffer, body_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

                uint32_t current_instance_count = 1;

                // per drawcall storage buffer
                uint32_t perdrawcall_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        perdrawcall_dynamic_offset));

                perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *body_mesh_node.model_matrix;
                perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                    body_mesh_node.joint_matrices ? 1.0 : -1.0;

                // per drawcall vertex blending storage buffer
                uint32_t per_drawcall_vertex_blending_dynamic_offset;

                if (body_mesh_node.joint_matrices)
                {
                    per_drawcall_vertex_blending_dynamic_offset =
                        roundUp(m_global_render_resource->_storage_buffer
                                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                                m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                        per_drawcall_vertex_blending_dynamic_offset +
                        sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                    assert(m_global_render_resource->_storage_buffer
                               ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                           (m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                            m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                    MeshPerdrawcallVertexBlendingStorageBufferObject&
                        per_drawcall_vertex_blending_storage_buffer_object =
                            (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                                reinterpret_cast<uintptr_t>(m_global_render_resource->_storage_buffer
                                                                ._global_upload_ringbuffer_memory_pointer) +
                                per_drawcall_vertex_blending_dynamic_offset));

                    for (uint32_t j = 0; j < body_mesh_node.joint_count; ++j)
                    {
                        per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                            body_mesh_node.joint_matrices[j];
                    }
                }
                else
                {
                    per_drawcall_vertex_blending_dynamic_offset = 0;
                }

                // bind perdrawcall
                uint32_t dynamic_offsets[3] = {
                    perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(
                    m_vulkan_rhi->m_current_command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_render_pipelines[_nbr_pipeline_type_outline].layout,
                    0,
                    1,
                    &m_descriptor_infos[3].descriptor_set,
                    3,
                    dynamic_offsets);

                m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                    body_mesh.mesh_index_count,
                                                    current_instance_count,
                                                    0,
                                                    0,
                                                    0);
            }

            if (nbr_mesh_nodes[_nbr_mesh_hair].material)
            {
                VulkanNBRMaterial& hair_material = *(nbr_mesh_nodes[_nbr_mesh_hair].material);

                VulkanMesh& hair_mesh      = *(nbr_mesh_nodes[_nbr_mesh_hair].mesh);
                auto&       hair_mesh_node = nbr_mesh_nodes[_nbr_mesh_hair];

                uint32_t total_instance_count = 1;

                m_nbr_outline_push_constant_object.outline_width    = 1.09;
                m_nbr_outline_push_constant_object.outline_z_offset = 0.0;
                m_nbr_outline_push_constant_object.outline_gamma    = 5.0;

                vkCmdPushConstants(m_vulkan_rhi->m_current_command_buffer,
                                   m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0,
                                   sizeof(NBROutlinePushConstantObject),
                                   &m_nbr_outline_push_constant_object);

                // bind per material
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                                            2,
                                                            1,
                                                            &hair_material.material_descriptor_set,
                                                            0,
                                                            NULL);

                // bind per mesh
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_command_buffer,
                                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                            m_render_pipelines[_nbr_pipeline_type_outline].layout,
                                                            1,
                                                            1,
                                                            &hair_mesh.mesh_vertex_blending_descriptor_set,
                                                            0,
                                                            NULL);

                VkBuffer     vertex_buffers[] = {hair_mesh.mesh_vertex_position_buffer,
                                                 hair_mesh.mesh_vertex_varying_enable_blending_buffer,
                                                 hair_mesh.mesh_vertex_varying_buffer,
                                                 hair_mesh.mesh_vertex_color_buffer};
                VkDeviceSize offsets[]        = {0, 0, 0, 0};
                m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_command_buffer,
                                                           0,
                                                           (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                           vertex_buffers,
                                                           offsets);
                m_vulkan_rhi->m_vk_cmd_bind_index_buffer(
                    m_vulkan_rhi->m_current_command_buffer, hair_mesh.mesh_index_buffer, 0, VK_INDEX_TYPE_UINT32);

                uint32_t current_instance_count = 1;

                // per drawcall storage buffer
                uint32_t perdrawcall_dynamic_offset =
                    roundUp(m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                            m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                    perdrawcall_dynamic_offset + sizeof(MeshPerdrawcallStorageBufferObject);
                assert(m_global_render_resource->_storage_buffer
                           ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                       (m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                        m_global_render_resource->_storage_buffer
                            ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                MeshPerdrawcallStorageBufferObject& perdrawcall_storage_buffer_object =
                    (*reinterpret_cast<MeshPerdrawcallStorageBufferObject*>(
                        reinterpret_cast<uintptr_t>(
                            m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
                        perdrawcall_dynamic_offset));

                perdrawcall_storage_buffer_object.mesh_instances[0].model_matrix = *hair_mesh_node.model_matrix;
                perdrawcall_storage_buffer_object.mesh_instances[0].enable_vertex_blending =
                    hair_mesh_node.joint_matrices ? 1.0 : -1.0;

                // per drawcall vertex blending storage buffer
                uint32_t per_drawcall_vertex_blending_dynamic_offset;

                if (hair_mesh_node.joint_matrices)
                {
                    per_drawcall_vertex_blending_dynamic_offset =
                        roundUp(m_global_render_resource->_storage_buffer
                                    ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                                m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);
                    m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
                        per_drawcall_vertex_blending_dynamic_offset +
                        sizeof(MeshPerdrawcallVertexBlendingStorageBufferObject);
                    assert(m_global_render_resource->_storage_buffer
                               ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
                           (m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                            m_global_render_resource->_storage_buffer
                                ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

                    MeshPerdrawcallVertexBlendingStorageBufferObject&
                        per_drawcall_vertex_blending_storage_buffer_object =
                            (*reinterpret_cast<MeshPerdrawcallVertexBlendingStorageBufferObject*>(
                                reinterpret_cast<uintptr_t>(m_global_render_resource->_storage_buffer
                                                                ._global_upload_ringbuffer_memory_pointer) +
                                per_drawcall_vertex_blending_dynamic_offset));

                    for (uint32_t j = 0; j < hair_mesh_node.joint_count; ++j)
                    {
                        per_drawcall_vertex_blending_storage_buffer_object.joint_matrices[j] =
                            hair_mesh_node.joint_matrices[j];
                    }
                }
                else
                {
                    per_drawcall_vertex_blending_dynamic_offset = 0;
                }

                // bind perdrawcall
                uint32_t dynamic_offsets[3] = {
                    perframe_dynamic_offset, perdrawcall_dynamic_offset, per_drawcall_vertex_blending_dynamic_offset};
                m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(
                    m_vulkan_rhi->m_current_command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_render_pipelines[_nbr_pipeline_type_outline].layout,
                    0,
                    1,
                    &m_descriptor_infos[3].descriptor_set,
                    3,
                    dynamic_offsets);

                m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_command_buffer,
                                                    hair_mesh.mesh_index_count,
                                                    current_instance_count,
                                                    0,
                                                    0,
                                                    0);
            }
        }


        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            m_vulkan_rhi->m_vk_cmd_end_debug_utils_label_ext(m_vulkan_rhi->m_current_command_buffer);
        }

    }
} // namespace Piccolo
