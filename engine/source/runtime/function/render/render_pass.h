#pragma once

#include "runtime/function/render/render_common.h"
#include "runtime/function/render/render_pass_base.h"
#include "runtime/function/render/render_resource.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace Piccolo
{
    class VulkanRHI;

    enum
    {
        _main_camera_pass_gbuffer_a                     = 0,
        _main_camera_pass_gbuffer_b                     = 1,
        _main_camera_pass_gbuffer_c                     = 2,
        _main_camera_pass_backup_buffer_odd             = 3,
        _main_camera_pass_backup_buffer_even            = 4,
        _main_camera_pass_color_output_image            = 5,
        _main_camera_pass_bright_color_output_image     = 6,
        _main_camera_pass_depth                         = 7,
        _main_camera_pass_custom_attachment_count       = 7,
        _main_camera_pass_attachment_count              = 8,
    };

    enum
    {
        _post_process_pass_backup_buffer_odd            = 0,
        _post_process_pass_backup_buffer_even           = 1,
        _post_process_pass_backup_buffer_extra          = 2,
        _post_process_pass_backup_buffer_ultra          = 3,
        _post_process_pass_swap_chain_image             = 4,
        _post_process_pass_color_input_image            = 5,
        _post_process_pass_bright_color_input_image     = 6,
        _post_process_pass_custom_attachment_count      = 4,
        _post_process_pass_attachment_count             = 7,
    };

    enum
    {
        _main_camera_subpass_basepass = 0,
        _main_camera_subpass_deferred_lighting,
        _main_camera_subpass_forward_lighting,
        _main_camera_subpass_ssao_generate,
        _main_camera_subpass_ssao_blur,
        _main_camera_subpass_count
    };

    enum
    {
        _post_process_subpass_tone_mapping,
        _post_process_subpass_color_grading,
        _post_process_subpass_fxaa,
        _post_process_subpass_vignette,
        _post_process_subpass_remap,
        _post_process_subpass_ui,
        _post_process_subpass_combine_ui,
        _post_process_subpass_count
    };

    struct VisiableNodes
    {
        std::vector<RenderMeshNode>*              p_directional_light_visible_mesh_nodes {nullptr};
        std::vector<RenderMeshNode>*              p_point_lights_visible_mesh_nodes {nullptr};
        std::vector<RenderMeshNode>*              p_main_camera_visible_mesh_nodes {nullptr};
        RenderAxisNode*                           p_axis_node {nullptr};
    };

    class RenderPass : public RenderPassBase
    {
    public:
        struct FrameBufferAttachment
        {
            VkImage        image;
            VkDeviceMemory mem;
            VkImageView    view;
            VkFormat       format;
        };

        struct Framebuffer
        {
            int           width;
            int           height;
            VkFramebuffer framebuffer;
            VkRenderPass  render_pass;

            std::vector<FrameBufferAttachment> attachments;
        };

        struct Descriptor
        {
            VkDescriptorSetLayout layout;
            VkDescriptorSet       descriptor_set;
        };

        struct RenderPipelineBase
        {
            VkPipelineLayout layout;
            VkPipeline       pipeline;
        };

        std::shared_ptr<VulkanRHI> m_vulkan_rhi {nullptr};
        GlobalRenderResource*      m_global_render_resource {nullptr};

        std::vector<Descriptor>         m_descriptor_infos;
        std::vector<RenderPipelineBase> m_render_pipelines;
        Framebuffer                     m_framebuffer;

        void initialize(const RenderPassInitInfo* init_info) override;
        void postInitialize() override;

        virtual void draw();

        virtual VkRenderPass                       getRenderPass() const;
        virtual std::vector<VkImageView>           getFramebufferImageViews() const;
        virtual std::vector<VkImage>               getFramebufferImages() const;
        virtual std::vector<VkDescriptorSetLayout> getDescriptorSetLayouts() const;

        static VisiableNodes m_visiable_nodes;

    private:
    };
} // namespace Piccolo
