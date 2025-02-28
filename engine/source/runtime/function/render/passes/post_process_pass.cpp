#include "runtime/function/render/passes/post_process_pass.h"
#include "runtime/function/render/render_helper.h"
#include "runtime/function/render/render_mesh.h"
#include "runtime/function/render/render_resource.h"

#include "runtime/function/render/rhi/vulkan/vulkan_rhi.h"
#include "runtime/function/render/rhi/vulkan/vulkan_util.h"

#include <map>
#include <stdexcept>

#include <axis_frag.h>
#include <axis_vert.h>


namespace Piccolo
{
    void PostProcessPass::initialize(const RenderPassInitInfo* init_info)
    {
        RenderPass::initialize(nullptr);

        const PostProcessPassInitInfo* _init_info = static_cast<const PostProcessPassInitInfo*>(init_info);
        m_enable_fxaa                            = _init_info->enable_fxaa;
        m_color_input_image_view                 = _init_info->color_input_image_view;
        m_bright_color_input_image_view          = _init_info->bright_color_input_image_view;
        setupAttachments();
        setupRenderPass();
        setupDescriptorSetLayout();
        setupPipelines();
        setupDescriptorSet();
        setupSwapchainFramebuffers();
    }

    void PostProcessPass::preparePassData(std::shared_ptr<RenderResourceBase> render_resource)
    {
        const RenderResource* vulkan_resource = static_cast<const RenderResource*>(render_resource.get());
        if (vulkan_resource)
        {
            m_mesh_perframe_storage_buffer_object = vulkan_resource->m_mesh_perframe_storage_buffer_object;
            m_axis_storage_buffer_object          = vulkan_resource->m_axis_storage_buffer_object;
        }
    }

    void PostProcessPass::setupAttachments()
    {
        m_framebuffer.attachments.resize(_post_process_pass_custom_attachment_count);

        m_framebuffer.attachments[_post_process_pass_backup_buffer_odd].format  = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_framebuffer.attachments[_post_process_pass_backup_buffer_even].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_framebuffer.attachments[_post_process_pass_backup_buffer_extra].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_framebuffer.attachments[_post_process_pass_backup_buffer_ultra].format = VK_FORMAT_R16G16B16A16_SFLOAT;

        for (int buffer_index = 0; buffer_index < _post_process_pass_custom_attachment_count; ++buffer_index)
        {
            
            VulkanUtil::createImage(m_vulkan_rhi->m_physical_device,
                                    m_vulkan_rhi->m_device,
                                    m_vulkan_rhi->m_swapchain_extent.width,
                                    m_vulkan_rhi->m_swapchain_extent.height,
                                    m_framebuffer.attachments[buffer_index].format,
                                    VK_IMAGE_TILING_OPTIMAL,
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    m_framebuffer.attachments[buffer_index].image,
                                    m_framebuffer.attachments[buffer_index].mem,
                                    0,
                                    1,
                                    1);
            
            m_framebuffer.attachments[buffer_index].view =
                VulkanUtil::createImageView(m_vulkan_rhi->m_device,
                                            m_framebuffer.attachments[buffer_index].image,
                                            m_framebuffer.attachments[buffer_index].format,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            VK_IMAGE_VIEW_TYPE_2D,
                                            1,
                                            1);
        }

    }

    void PostProcessPass::setupRenderPass()
    {
        VkAttachmentDescription attachments_dscp[_post_process_pass_attachment_count] = {};

        VkAttachmentDescription& backup_odd_color_attachment_description =
            attachments_dscp[_post_process_pass_backup_buffer_odd];
        backup_odd_color_attachment_description.format =
            m_framebuffer.attachments[_post_process_pass_backup_buffer_odd].format;
        backup_odd_color_attachment_description.samples        = VK_SAMPLE_COUNT_1_BIT;
        backup_odd_color_attachment_description.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        backup_odd_color_attachment_description.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_odd_color_attachment_description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        backup_odd_color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_odd_color_attachment_description.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        backup_odd_color_attachment_description.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription& backup_even_color_attachment_description =
            attachments_dscp[_post_process_pass_backup_buffer_even];
        backup_even_color_attachment_description.format =
            m_framebuffer.attachments[_post_process_pass_backup_buffer_even].format;
        backup_even_color_attachment_description.samples        = VK_SAMPLE_COUNT_1_BIT;
        backup_even_color_attachment_description.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        backup_even_color_attachment_description.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_even_color_attachment_description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        backup_even_color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_even_color_attachment_description.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        backup_even_color_attachment_description.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription& backup_extra_color_attachment_description =
            attachments_dscp[_post_process_pass_backup_buffer_extra];
        backup_extra_color_attachment_description.format =
            m_framebuffer.attachments[_post_process_pass_backup_buffer_extra].format;
        backup_extra_color_attachment_description.samples       = VK_SAMPLE_COUNT_1_BIT;
        backup_extra_color_attachment_description.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        backup_extra_color_attachment_description.storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_extra_color_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        backup_extra_color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_extra_color_attachment_description.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        backup_extra_color_attachment_description.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkAttachmentDescription& backup_ultra_color_attachment_description =
            attachments_dscp[_post_process_pass_backup_buffer_ultra];
        backup_ultra_color_attachment_description.format =
            m_framebuffer.attachments[_post_process_pass_backup_buffer_ultra].format;
        backup_ultra_color_attachment_description.samples       = VK_SAMPLE_COUNT_1_BIT;
        backup_ultra_color_attachment_description.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        backup_ultra_color_attachment_description.storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_ultra_color_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        backup_ultra_color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        backup_ultra_color_attachment_description.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        backup_ultra_color_attachment_description.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription& input_color_attachment_description = attachments_dscp[_post_process_pass_color_input_image];
        input_color_attachment_description.format                   = m_framebuffer.attachments[_post_process_pass_backup_buffer_odd].format; // just VK_FORMAT_R16G16B16A16_SFLOAT
        input_color_attachment_description.samples                  = VK_SAMPLE_COUNT_1_BIT;
        input_color_attachment_description.loadOp                   = VK_ATTACHMENT_LOAD_OP_LOAD;
        input_color_attachment_description.storeOp                  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        input_color_attachment_description.stencilLoadOp            = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        input_color_attachment_description.stencilStoreOp           = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        input_color_attachment_description.initialLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_color_attachment_description.finalLayout              = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkAttachmentDescription& input_bight_color_attachment_description = attachments_dscp[_post_process_pass_bright_color_input_image];
        input_bight_color_attachment_description.format                   = m_framebuffer.attachments[_post_process_pass_backup_buffer_odd].format; // just VK_FORMAT_R16G16B16A16_SFLOAT
        input_bight_color_attachment_description.samples                  = VK_SAMPLE_COUNT_1_BIT;
        input_bight_color_attachment_description.loadOp                   = VK_ATTACHMENT_LOAD_OP_LOAD;
        input_bight_color_attachment_description.storeOp                  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        input_bight_color_attachment_description.stencilLoadOp            = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        input_bight_color_attachment_description.stencilStoreOp           = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        input_bight_color_attachment_description.initialLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_bight_color_attachment_description.finalLayout              = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription& swapchain_image_attachment_description =
            attachments_dscp[_post_process_pass_swap_chain_image];
        swapchain_image_attachment_description.format         = m_vulkan_rhi->m_swapchain_image_format;
        swapchain_image_attachment_description.samples        = VK_SAMPLE_COUNT_1_BIT;
        swapchain_image_attachment_description.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        swapchain_image_attachment_description.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        swapchain_image_attachment_description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        swapchain_image_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        swapchain_image_attachment_description.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchain_image_attachment_description.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkSubpassDescription subpasses[_post_process_subpass_count] = {};

        VkAttachmentReference tone_mapping_pass_input_attachment_reference[2] {};
        tone_mapping_pass_input_attachment_reference[0].attachment =
            &input_color_attachment_description - attachments_dscp;
        tone_mapping_pass_input_attachment_reference[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        tone_mapping_pass_input_attachment_reference[1].attachment =
            &input_bight_color_attachment_description - attachments_dscp;
        tone_mapping_pass_input_attachment_reference[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference tone_mapping_pass_color_attachment_reference {};
        tone_mapping_pass_color_attachment_reference.attachment =
            &backup_odd_color_attachment_description - attachments_dscp;
        tone_mapping_pass_color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription& tone_mapping_pass   = subpasses[_post_process_subpass_tone_mapping];
        tone_mapping_pass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        tone_mapping_pass.inputAttachmentCount    = 2;
        tone_mapping_pass.pInputAttachments       = tone_mapping_pass_input_attachment_reference;
        tone_mapping_pass.colorAttachmentCount    = 1;
        tone_mapping_pass.pColorAttachments       = &tone_mapping_pass_color_attachment_reference;
        tone_mapping_pass.pDepthStencilAttachment = NULL;
        tone_mapping_pass.preserveAttachmentCount = 0;
        tone_mapping_pass.pPreserveAttachments    = NULL;

        VkAttachmentReference color_grading_pass_input_attachment_reference {};
        color_grading_pass_input_attachment_reference.attachment =
            &backup_odd_color_attachment_description - attachments_dscp;
        color_grading_pass_input_attachment_reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference color_grading_pass_color_attachment_reference {};
        color_grading_pass_color_attachment_reference.attachment =
            &backup_even_color_attachment_description - attachments_dscp;
        color_grading_pass_color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription& color_grading_pass   = subpasses[_post_process_subpass_color_grading];
        color_grading_pass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        color_grading_pass.inputAttachmentCount    = 1;
        color_grading_pass.pInputAttachments       = &color_grading_pass_input_attachment_reference;
        color_grading_pass.colorAttachmentCount    = 1;
        color_grading_pass.pColorAttachments       = &color_grading_pass_color_attachment_reference;
        color_grading_pass.pDepthStencilAttachment = NULL;
        color_grading_pass.preserveAttachmentCount = 0;
        color_grading_pass.pPreserveAttachments    = NULL;

        VkAttachmentReference fxaa_pass_input_attachment_reference {};
        fxaa_pass_input_attachment_reference.attachment = 
            &backup_even_color_attachment_description - attachments_dscp;
        fxaa_pass_input_attachment_reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference fxaa_pass_color_attachment_reference {};
        fxaa_pass_color_attachment_reference.attachment = 
            &backup_extra_color_attachment_description - attachments_dscp;
        fxaa_pass_color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription& fxaa_pass   = subpasses[_post_process_subpass_fxaa];
        fxaa_pass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        fxaa_pass.inputAttachmentCount    = 1;
        fxaa_pass.pInputAttachments       = &fxaa_pass_input_attachment_reference;
        fxaa_pass.colorAttachmentCount    = 1;
        fxaa_pass.pColorAttachments       = &fxaa_pass_color_attachment_reference;
        fxaa_pass.pDepthStencilAttachment = NULL;
        fxaa_pass.preserveAttachmentCount = 0;
        fxaa_pass.pPreserveAttachments    = NULL;

        VkAttachmentReference vignette_pass_input_attachment_reference {};
        vignette_pass_input_attachment_reference.attachment = 
            &backup_extra_color_attachment_description - attachments_dscp;
        vignette_pass_input_attachment_reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference vignette_pass_color_attachment_reference {};
        vignette_pass_color_attachment_reference.attachment = 
            &backup_ultra_color_attachment_description - attachments_dscp;
        vignette_pass_color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription& vignette_pass   = subpasses[_post_process_subpass_vignette];
        vignette_pass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vignette_pass.inputAttachmentCount    = 1;
        vignette_pass.pInputAttachments       = &vignette_pass_input_attachment_reference;
        vignette_pass.colorAttachmentCount    = 1;
        vignette_pass.pColorAttachments       = &vignette_pass_color_attachment_reference;
        vignette_pass.pDepthStencilAttachment = NULL;
        vignette_pass.preserveAttachmentCount = 0;
        vignette_pass.pPreserveAttachments    = NULL;
        
        VkAttachmentReference remap_pass_input_attachment_reference {};
        remap_pass_input_attachment_reference.attachment = 
            &backup_ultra_color_attachment_description - attachments_dscp;
        remap_pass_input_attachment_reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference remap_pass_color_attachment_reference {};
        remap_pass_color_attachment_reference.attachment = 
            &backup_odd_color_attachment_description - attachments_dscp;
        remap_pass_color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription& remap_pass   = subpasses[_post_process_subpass_remap];
        remap_pass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        remap_pass.inputAttachmentCount    = 1;
        remap_pass.pInputAttachments       = &remap_pass_input_attachment_reference;
        remap_pass.colorAttachmentCount    = 1;
        remap_pass.pColorAttachments       = &remap_pass_color_attachment_reference;
        remap_pass.pDepthStencilAttachment = NULL;
        remap_pass.preserveAttachmentCount = 0;
        remap_pass.pPreserveAttachments    = NULL;

        VkAttachmentReference ui_pass_color_attachment_reference {};
        ui_pass_color_attachment_reference.attachment = &backup_even_color_attachment_description - attachments_dscp;
        ui_pass_color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        uint32_t ui_pass_preserve_attachment = &backup_odd_color_attachment_description - attachments_dscp;

        VkSubpassDescription& ui_pass   = subpasses[_post_process_subpass_ui];
        ui_pass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        ui_pass.inputAttachmentCount    = 0;
        ui_pass.pInputAttachments       = NULL;
        ui_pass.colorAttachmentCount    = 1;
        ui_pass.pColorAttachments       = &ui_pass_color_attachment_reference;
        ui_pass.pDepthStencilAttachment = NULL;
        ui_pass.preserveAttachmentCount = 1;
        ui_pass.pPreserveAttachments    = &ui_pass_preserve_attachment;

        VkAttachmentReference combine_ui_pass_input_attachments_reference[2] = {};
        combine_ui_pass_input_attachments_reference[0].attachment =
            &backup_odd_color_attachment_description - attachments_dscp;
        combine_ui_pass_input_attachments_reference[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        combine_ui_pass_input_attachments_reference[1].attachment =
            &backup_even_color_attachment_description - attachments_dscp;
        combine_ui_pass_input_attachments_reference[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference combine_ui_pass_color_attachment_reference {};
        combine_ui_pass_color_attachment_reference.attachment = &swapchain_image_attachment_description - attachments_dscp;
        combine_ui_pass_color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription& combine_ui_pass = subpasses[_post_process_subpass_combine_ui];
        combine_ui_pass.pipelineBindPoint     = VK_PIPELINE_BIND_POINT_GRAPHICS;
        combine_ui_pass.inputAttachmentCount  = sizeof(combine_ui_pass_input_attachments_reference) /
                                               sizeof(combine_ui_pass_input_attachments_reference[0]);
        combine_ui_pass.pInputAttachments       = combine_ui_pass_input_attachments_reference;
        combine_ui_pass.colorAttachmentCount    = 1;
        combine_ui_pass.pColorAttachments       = &combine_ui_pass_color_attachment_reference;
        combine_ui_pass.pDepthStencilAttachment = NULL;
        combine_ui_pass.preserveAttachmentCount = 0;
        combine_ui_pass.pPreserveAttachments    = NULL;

        VkSubpassDependency dependencies[6] = {};
        uint32_t dependency_index = 0;
        
        VkSubpassDependency& color_grading_pass_depend_on_tone_mapping_pass = dependencies[dependency_index++];
        color_grading_pass_depend_on_tone_mapping_pass.srcSubpass           = _post_process_subpass_tone_mapping;
        color_grading_pass_depend_on_tone_mapping_pass.dstSubpass           = _post_process_subpass_color_grading;
        color_grading_pass_depend_on_tone_mapping_pass.srcStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        color_grading_pass_depend_on_tone_mapping_pass.dstStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        color_grading_pass_depend_on_tone_mapping_pass.srcAccessMask =
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        color_grading_pass_depend_on_tone_mapping_pass.dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        color_grading_pass_depend_on_tone_mapping_pass.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkSubpassDependency& fxaa_pass_depend_on_color_grading_pass = dependencies[dependency_index++];
        fxaa_pass_depend_on_color_grading_pass.srcSubpass           = _post_process_subpass_color_grading;
        fxaa_pass_depend_on_color_grading_pass.dstSubpass           = _post_process_subpass_fxaa;
        fxaa_pass_depend_on_color_grading_pass.srcStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        fxaa_pass_depend_on_color_grading_pass.dstStageMask         = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        fxaa_pass_depend_on_color_grading_pass.srcAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        fxaa_pass_depend_on_color_grading_pass.dstAccessMask        = VK_ACCESS_SHADER_READ_BIT;
        
        VkSubpassDependency& vignette_pass_depend_on_fxaa_pass = dependencies[dependency_index++];
        vignette_pass_depend_on_fxaa_pass.srcSubpass           = _post_process_subpass_fxaa;
        vignette_pass_depend_on_fxaa_pass.dstSubpass           = _post_process_subpass_vignette;
        vignette_pass_depend_on_fxaa_pass.srcStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        vignette_pass_depend_on_fxaa_pass.dstStageMask         = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        vignette_pass_depend_on_fxaa_pass.srcAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vignette_pass_depend_on_fxaa_pass.dstAccessMask        = VK_ACCESS_SHADER_READ_BIT;
        vignette_pass_depend_on_fxaa_pass.dependencyFlags      = VK_DEPENDENCY_BY_REGION_BIT;
        
        VkSubpassDependency& remap_pass_depend_on_vignette_pass = dependencies[dependency_index++];
        remap_pass_depend_on_vignette_pass.srcSubpass           = _post_process_subpass_vignette;
        remap_pass_depend_on_vignette_pass.dstSubpass           = _post_process_subpass_remap;
        remap_pass_depend_on_vignette_pass.srcStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        remap_pass_depend_on_vignette_pass.dstStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        remap_pass_depend_on_vignette_pass.srcAccessMask =
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        remap_pass_depend_on_vignette_pass.dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        VkSubpassDependency& ui_pass_depend_on_remap_pass = dependencies[dependency_index++];
        ui_pass_depend_on_remap_pass.srcSubpass           = _post_process_subpass_remap;
        ui_pass_depend_on_remap_pass.dstSubpass           = _post_process_subpass_ui;
        ui_pass_depend_on_remap_pass.srcStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ui_pass_depend_on_remap_pass.dstStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ui_pass_depend_on_remap_pass.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ui_pass_depend_on_remap_pass.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        ui_pass_depend_on_remap_pass.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkSubpassDependency& combine_ui_pass_depend_on_ui_pass = dependencies[dependency_index++];
        combine_ui_pass_depend_on_ui_pass.srcSubpass           = _post_process_subpass_ui;
        combine_ui_pass_depend_on_ui_pass.dstSubpass           = _post_process_subpass_combine_ui;
        combine_ui_pass_depend_on_ui_pass.srcStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        combine_ui_pass_depend_on_ui_pass.dstStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        combine_ui_pass_depend_on_ui_pass.srcAccessMask =
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        combine_ui_pass_depend_on_ui_pass.dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        combine_ui_pass_depend_on_ui_pass.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

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
            throw std::runtime_error("failed to create render pass");
        }
    }

    void PostProcessPass::setupDescriptorSetLayout()
    {
        m_descriptor_infos.resize(_layout_type_count);

        {
            VkDescriptorSetLayoutBinding axis_layout_bindings[2];

            VkDescriptorSetLayoutBinding& axis_layout_perframe_storage_buffer_binding = axis_layout_bindings[0];
            axis_layout_perframe_storage_buffer_binding.binding                       = 0;
            axis_layout_perframe_storage_buffer_binding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            axis_layout_perframe_storage_buffer_binding.descriptorCount    = 1;
            axis_layout_perframe_storage_buffer_binding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
            axis_layout_perframe_storage_buffer_binding.pImmutableSamplers = NULL;

            VkDescriptorSetLayoutBinding& axis_layout_storage_buffer_binding = axis_layout_bindings[1];
            axis_layout_storage_buffer_binding.binding                       = 1;
            axis_layout_storage_buffer_binding.descriptorType                = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            axis_layout_storage_buffer_binding.descriptorCount               = 1;
            axis_layout_storage_buffer_binding.stageFlags                    = VK_SHADER_STAGE_VERTEX_BIT;
            axis_layout_storage_buffer_binding.pImmutableSamplers            = NULL;

            VkDescriptorSetLayoutCreateInfo axis_layout_create_info {};
            axis_layout_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            axis_layout_create_info.bindingCount = 2;
            axis_layout_create_info.pBindings    = axis_layout_bindings;

            if (VK_SUCCESS !=
                vkCreateDescriptorSetLayout(
                    m_vulkan_rhi->m_device, &axis_layout_create_info, NULL, &m_descriptor_infos[_axis].layout))
            {
                throw std::runtime_error("create axis layout");
            }
        }

    }

    void PostProcessPass::setupPipelines()
    {
        m_render_pipelines.resize(_render_pipeline_type_count);

        // draw axis
        {
            VkDescriptorSetLayout      descriptorset_layouts[1] = {m_descriptor_infos[_axis].layout};
            VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
            pipeline_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipeline_layout_create_info.setLayoutCount = 1;
            pipeline_layout_create_info.pSetLayouts    = descriptorset_layouts;

            if (vkCreatePipelineLayout(m_vulkan_rhi->m_device,
                                       &pipeline_layout_create_info,
                                       nullptr,
                                       &m_render_pipelines[_render_pipeline_type_axis].layout) != VK_SUCCESS)
            {
                throw std::runtime_error("create axis pipeline layout");
            }

            VkShaderModule vert_shader_module = VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, AXIS_VERT);
            VkShaderModule frag_shader_module = VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, AXIS_FRAG);

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
            rasterization_state_create_info.cullMode                = VK_CULL_MODE_NONE;
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
            depth_stencil_create_info.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil_create_info.depthTestEnable  = VK_FALSE;
            depth_stencil_create_info.depthWriteEnable = VK_FALSE;
            depth_stencil_create_info.depthCompareOp   = VK_COMPARE_OP_LESS;
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
            pipelineInfo.layout              = m_render_pipelines[_render_pipeline_type_axis].layout;
            pipelineInfo.renderPass          = m_framebuffer.render_pass;
            pipelineInfo.subpass             = _post_process_subpass_ui;
            pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
            pipelineInfo.pDynamicState       = &dynamic_state_create_info;

            if (vkCreateGraphicsPipelines(m_vulkan_rhi->m_device,
                                          VK_NULL_HANDLE,
                                          1,
                                          &pipelineInfo,
                                          nullptr,
                                          &m_render_pipelines[_render_pipeline_type_axis].pipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("create axis graphics pipeline");
            }

            vkDestroyShaderModule(m_vulkan_rhi->m_device, vert_shader_module, nullptr);
            vkDestroyShaderModule(m_vulkan_rhi->m_device, frag_shader_module, nullptr);
        }
    }

    void PostProcessPass::setupDescriptorSet()
    {
        setupAxisDescriptorSet();
    }


    void PostProcessPass::setupAxisDescriptorSet()
    {
        VkDescriptorSetAllocateInfo axis_descriptor_set_alloc_info;
        axis_descriptor_set_alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        axis_descriptor_set_alloc_info.pNext              = NULL;
        axis_descriptor_set_alloc_info.descriptorPool     = m_vulkan_rhi->m_descriptor_pool;
        axis_descriptor_set_alloc_info.descriptorSetCount = 1;
        axis_descriptor_set_alloc_info.pSetLayouts        = &m_descriptor_infos[_axis].layout;

        if (VK_SUCCESS != vkAllocateDescriptorSets(m_vulkan_rhi->m_device,
                                                   &axis_descriptor_set_alloc_info,
                                                   &m_descriptor_infos[_axis].descriptor_set))
        {
            throw std::runtime_error("allocate axis descriptor set");
        }

        VkDescriptorBufferInfo mesh_perframe_storage_buffer_info = {};
        mesh_perframe_storage_buffer_info.offset                 = 0;
        mesh_perframe_storage_buffer_info.range                  = sizeof(MeshPerframeStorageBufferObject);
        mesh_perframe_storage_buffer_info.buffer = m_global_render_resource->_storage_buffer._global_upload_ringbuffer;
        assert(mesh_perframe_storage_buffer_info.range <
               m_global_render_resource->_storage_buffer._max_storage_buffer_range);

        VkDescriptorBufferInfo axis_storage_buffer_info = {};
        axis_storage_buffer_info.offset                 = 0;
        axis_storage_buffer_info.range                  = sizeof(AxisStorageBufferObject);
        axis_storage_buffer_info.buffer = m_global_render_resource->_storage_buffer._axis_inefficient_storage_buffer;

        VkWriteDescriptorSet axis_descriptor_writes_info[2];

        axis_descriptor_writes_info[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        axis_descriptor_writes_info[0].pNext           = NULL;
        axis_descriptor_writes_info[0].dstSet          = m_descriptor_infos[_axis].descriptor_set;
        axis_descriptor_writes_info[0].dstBinding      = 0;
        axis_descriptor_writes_info[0].dstArrayElement = 0;
        axis_descriptor_writes_info[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        axis_descriptor_writes_info[0].descriptorCount = 1;
        axis_descriptor_writes_info[0].pBufferInfo     = &mesh_perframe_storage_buffer_info;

        axis_descriptor_writes_info[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        axis_descriptor_writes_info[1].pNext           = NULL;
        axis_descriptor_writes_info[1].dstSet          = m_descriptor_infos[_axis].descriptor_set;
        axis_descriptor_writes_info[1].dstBinding      = 1;
        axis_descriptor_writes_info[1].dstArrayElement = 0;
        axis_descriptor_writes_info[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        axis_descriptor_writes_info[1].descriptorCount = 1;
        axis_descriptor_writes_info[1].pBufferInfo     = &axis_storage_buffer_info;

        vkUpdateDescriptorSets(m_vulkan_rhi->m_device,
                               (uint32_t)(sizeof(axis_descriptor_writes_info) / sizeof(axis_descriptor_writes_info[0])),
                               axis_descriptor_writes_info,
                               0,
                               NULL);
    }

    

    void PostProcessPass::setupSwapchainFramebuffers()
    {
        m_swapchain_framebuffers.resize(m_vulkan_rhi->m_swapchain_imageviews.size());

        // create frame buffer for every imageview
        for (size_t i = 0; i < m_vulkan_rhi->m_swapchain_imageviews.size(); i++)
        {
            VkImageView framebuffer_attachments_for_image_view[_post_process_pass_attachment_count] = {
                m_framebuffer.attachments[_post_process_pass_backup_buffer_odd].view,
                m_framebuffer.attachments[_post_process_pass_backup_buffer_even].view,
                m_framebuffer.attachments[_post_process_pass_backup_buffer_extra].view,
                m_framebuffer.attachments[_post_process_pass_backup_buffer_ultra].view,
                m_vulkan_rhi->m_swapchain_imageviews[i],
                m_color_input_image_view,
                m_bright_color_input_image_view
            };

            VkFramebufferCreateInfo framebuffer_create_info {};
            framebuffer_create_info.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.flags      = 0U;
            framebuffer_create_info.renderPass = m_framebuffer.render_pass;
            framebuffer_create_info.attachmentCount =
                (sizeof(framebuffer_attachments_for_image_view) / sizeof(framebuffer_attachments_for_image_view[0]));
            framebuffer_create_info.pAttachments = framebuffer_attachments_for_image_view;
            framebuffer_create_info.width        = m_vulkan_rhi->m_swapchain_extent.width;
            framebuffer_create_info.height       = m_vulkan_rhi->m_swapchain_extent.height;
            framebuffer_create_info.layers       = 1;

            if (vkCreateFramebuffer(
                    m_vulkan_rhi->m_device, &framebuffer_create_info, nullptr, &m_swapchain_framebuffers[i]) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("create main camera framebuffer");
            }
        }
    }

    void PostProcessPass::updateAfterFramebufferRecreate(VkImageView color_input_attachment)
    {
        for (size_t i = 0; i < m_framebuffer.attachments.size(); i++)
        {
            vkDestroyImage(m_vulkan_rhi->m_device, m_framebuffer.attachments[i].image, nullptr);
            vkDestroyImageView(m_vulkan_rhi->m_device, m_framebuffer.attachments[i].view, nullptr);
            vkFreeMemory(m_vulkan_rhi->m_device, m_framebuffer.attachments[i].mem, nullptr);
        }

        for (auto framebuffer : m_swapchain_framebuffers)
        {
            vkDestroyFramebuffer(m_vulkan_rhi->m_device, framebuffer, NULL);
        }

        setupAttachments();

        setupSwapchainFramebuffers();

    }

    void PostProcessPass::draw(VignettePass&     vignette_pass,
                               RemapPass&        remap_pass,
                               ColorGradingPass& color_grading_pass,
                               FXAAPass&         fxaa_pass,
                               ToneMappingPass&  tone_mapping_pass,
                               UIPass&           ui_pass,
                               CombineUIPass&    combine_ui_pass,
                               uint32_t          current_swapchain_image_index)
    {
        {
            VkRenderPassBeginInfo renderpass_begin_info {};
            renderpass_begin_info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderpass_begin_info.renderPass        = m_framebuffer.render_pass;
            renderpass_begin_info.framebuffer       = m_swapchain_framebuffers[current_swapchain_image_index];
            renderpass_begin_info.renderArea.offset = {0, 0};
            renderpass_begin_info.renderArea.extent = m_vulkan_rhi->m_swapchain_extent;

            VkClearValue clear_values[_post_process_pass_attachment_count-1];
            clear_values[_post_process_pass_backup_buffer_odd].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clear_values[_post_process_pass_backup_buffer_even].color       = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clear_values[_post_process_pass_swap_chain_image].color         = {{0.0f, 0.0f, 0.0f, 1.0f}};
            renderpass_begin_info.clearValueCount = (sizeof(clear_values) / sizeof(clear_values[0]));
            renderpass_begin_info.pClearValues    = clear_values;

            m_vulkan_rhi->m_vk_cmd_begin_render_pass(
                m_vulkan_rhi->m_current_post_process_command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        }

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            VkDebugUtilsLabelEXT label_info = {
                VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "ToneMapping", {1.0f, 1.0f, 1.0f, 1.0f}};
            m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_vulkan_rhi->m_current_post_process_command_buffer, &label_info);
        }

        tone_mapping_pass.draw();

        m_vulkan_rhi->m_vk_cmd_next_subpass(m_vulkan_rhi->m_current_post_process_command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        color_grading_pass.draw();

        m_vulkan_rhi->m_vk_cmd_next_subpass(m_vulkan_rhi->m_current_post_process_command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        fxaa_pass.draw();
       
        m_vulkan_rhi->m_vk_cmd_next_subpass(m_vulkan_rhi->m_current_post_process_command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        vignette_pass.draw();

        m_vulkan_rhi->m_vk_cmd_next_subpass(m_vulkan_rhi->m_current_post_process_command_buffer, VK_SUBPASS_CONTENTS_INLINE);
        
        {
            VkClearAttachment clear_attachments[1];
            clear_attachments[0].aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
            clear_attachments[0].colorAttachment             = _post_process_pass_backup_buffer_odd;
            clear_attachments[0].clearValue.color.float32[0] = 0.0;
            clear_attachments[0].clearValue.color.float32[1] = 0.0;
            clear_attachments[0].clearValue.color.float32[2] = 0.0;
            clear_attachments[0].clearValue.color.float32[3] = 0.0;
            VkClearRect clear_rects[1];
            clear_rects[0].baseArrayLayer     = 0;
            clear_rects[0].layerCount         = 1;
            clear_rects[0].rect.offset.x      = 0;
            clear_rects[0].rect.offset.y      = 0;
            clear_rects[0].rect.extent.width  = m_vulkan_rhi->m_swapchain_extent.width;
            clear_rects[0].rect.extent.height = m_vulkan_rhi->m_swapchain_extent.height;
            m_vulkan_rhi->m_vk_cmd_clear_attachments(m_vulkan_rhi->m_current_post_process_command_buffer,
                                                     sizeof(clear_attachments) / sizeof(clear_attachments[0]),
                                                     clear_attachments,
                                                     sizeof(clear_rects) / sizeof(clear_rects[0]),
                                                     clear_rects);
        }
        
        remap_pass.draw();

        m_vulkan_rhi->m_vk_cmd_next_subpass(m_vulkan_rhi->m_current_post_process_command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        VkClearAttachment clear_attachments[1];
        clear_attachments[0].aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
        clear_attachments[0].colorAttachment             = 0;
        clear_attachments[0].clearValue.color.float32[0] = 0.0;
        clear_attachments[0].clearValue.color.float32[1] = 0.0;
        clear_attachments[0].clearValue.color.float32[2] = 0.0;
        clear_attachments[0].clearValue.color.float32[3] = 0.0;
        VkClearRect clear_rects[1];
        clear_rects[0].baseArrayLayer     = 0;
        clear_rects[0].layerCount         = 1;
        clear_rects[0].rect.offset.x      = 0;
        clear_rects[0].rect.offset.y      = 0;
        clear_rects[0].rect.extent.width  = m_vulkan_rhi->m_swapchain_extent.width;
        clear_rects[0].rect.extent.height = m_vulkan_rhi->m_swapchain_extent.height;
        m_vulkan_rhi->m_vk_cmd_clear_attachments(m_vulkan_rhi->m_current_post_process_command_buffer,
                                                 sizeof(clear_attachments) / sizeof(clear_attachments[0]),
                                                 clear_attachments,
                                                 sizeof(clear_rects) / sizeof(clear_rects[0]),
                                                 clear_rects);

        drawAxis();

        ui_pass.draw();

        m_vulkan_rhi->m_vk_cmd_next_subpass(m_vulkan_rhi->m_current_post_process_command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        combine_ui_pass.draw();

        m_vulkan_rhi->m_vk_cmd_end_render_pass(m_vulkan_rhi->m_current_post_process_command_buffer);
    }


    void PostProcessPass::drawAxis()
    {
        if (!m_is_show_axis)
            return;

        uint32_t perframe_dynamic_offset =
            roundUp(m_global_render_resource->_storage_buffer
                        ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index],
                    m_global_render_resource->_storage_buffer._min_storage_buffer_offset_alignment);

        m_global_render_resource->_storage_buffer._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] =
            perframe_dynamic_offset + sizeof(MeshPerframeStorageBufferObject);
        assert(m_global_render_resource->_storage_buffer
                   ._global_upload_ringbuffers_end[m_vulkan_rhi->m_current_frame_index] <=
               (m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_begin[m_vulkan_rhi->m_current_frame_index] +
                m_global_render_resource->_storage_buffer
                    ._global_upload_ringbuffers_size[m_vulkan_rhi->m_current_frame_index]));

        (*reinterpret_cast<MeshPerframeStorageBufferObject*>(
            reinterpret_cast<uintptr_t>(
                m_global_render_resource->_storage_buffer._global_upload_ringbuffer_memory_pointer) +
            perframe_dynamic_offset)) = m_mesh_perframe_storage_buffer_object;

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            VkDebugUtilsLabelEXT label_info = {
                VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "Axis", {1.0f, 1.0f, 1.0f, 1.0f}};
            m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_vulkan_rhi->m_current_post_process_command_buffer, &label_info);
        }

        m_vulkan_rhi->m_vk_cmd_bind_pipeline(m_vulkan_rhi->m_current_post_process_command_buffer,
                                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                                             m_render_pipelines[_render_pipeline_type_axis].pipeline);

        m_vulkan_rhi->m_vk_cmd_set_viewport(m_vulkan_rhi->m_current_post_process_command_buffer, 0, 1, &m_vulkan_rhi->m_viewport);
        m_vulkan_rhi->m_vk_cmd_set_scissor(m_vulkan_rhi->m_current_post_process_command_buffer, 0, 1, &m_vulkan_rhi->m_scissor);

        m_vulkan_rhi->m_vk_cmd_bind_descriptor_sets(m_vulkan_rhi->m_current_post_process_command_buffer,
                                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                    m_render_pipelines[_render_pipeline_type_axis].layout,
                                                    0,
                                                    1,
                                                    &m_descriptor_infos[_axis].descriptor_set,
                                                    1,
                                                    &perframe_dynamic_offset);

        m_axis_storage_buffer_object.selected_axis = m_selected_axis;
        m_axis_storage_buffer_object.model_matrix  = m_visiable_nodes.p_axis_node->model_matrix;

        VkBuffer     vertex_buffers[] = {m_visiable_nodes.p_axis_node->ref_mesh->mesh_vertex_position_buffer,
                                     m_visiable_nodes.p_axis_node->ref_mesh->mesh_vertex_varying_enable_blending_buffer,
                                     m_visiable_nodes.p_axis_node->ref_mesh->mesh_vertex_varying_buffer};
        VkDeviceSize offsets[]        = {0, 0, 0};
        m_vulkan_rhi->m_vk_cmd_bind_vertex_buffers(m_vulkan_rhi->m_current_post_process_command_buffer,
                                                   0,
                                                   (sizeof(vertex_buffers) / sizeof(vertex_buffers[0])),
                                                   vertex_buffers,
                                                   offsets);
        m_vulkan_rhi->m_vk_cmd_bind_index_buffer(m_vulkan_rhi->m_current_post_process_command_buffer,
                                                 m_visiable_nodes.p_axis_node->ref_mesh->mesh_index_buffer,
                                                 0,
                                                 VK_INDEX_TYPE_UINT32);
        (*reinterpret_cast<AxisStorageBufferObject*>(reinterpret_cast<uintptr_t>(
            m_global_render_resource->_storage_buffer._axis_inefficient_storage_buffer_memory_pointer))) =
            m_axis_storage_buffer_object;

        m_vulkan_rhi->m_vk_cmd_draw_indexed(m_vulkan_rhi->m_current_post_process_command_buffer,
                                            m_visiable_nodes.p_axis_node->ref_mesh->mesh_index_count,
                                            1,
                                            0,
                                            0,
                                            0);

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            m_vulkan_rhi->m_vk_cmd_end_debug_utils_label_ext(m_vulkan_rhi->m_current_post_process_command_buffer);
        }
    }

    VkCommandBuffer PostProcessPass::getRenderCommandBuffer() { return m_vulkan_rhi->m_current_post_process_command_buffer; }

} // namespace Piccolo
