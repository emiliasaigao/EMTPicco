#pragma once

#include "runtime/function/render/render_pass.h"

#include "runtime/function/render/passes/color_grading_pass.h"
#include "runtime/function/render/passes/combine_ui_pass.h"
#include "runtime/function/render/passes/fxaa_pass.h"
#include "runtime/function/render/passes/tone_mapping_pass.h"
#include "runtime/function/render/passes/ui_pass.h"
#include "runtime/function/render/passes/vignette_pass.h"
#include "runtime/function/render/passes/remap_pass.h"

namespace Piccolo
{
    class RenderResourceBase;

    struct PostProcessPassInitInfo : RenderPassInitInfo
    {
        bool enable_fxaa;
        VkImageView color_input_image_view;
        VkImageView bright_color_input_image_view;
    };

    class PostProcessPass : public RenderPass
    {
    public:

        enum LayoutType : uint8_t
        {
            _axis,
            _layout_type_count
        };


        enum RenderPipeLineType : uint8_t
        {
            _render_pipeline_type_axis,
            _render_pipeline_type_count
        };

        void initialize(const RenderPassInitInfo* init_info) override final;

        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;

        void draw(VignettePass& vignette_pass,
            RemapPass& remap_pass,
            ColorGradingPass& color_grading_pass,
            FXAAPass& fxaa_pass,
            ToneMappingPass& tone_mapping_pass,
            UIPass& ui_pass,
            CombineUIPass& combine_ui_pass,
            uint32_t          current_swapchain_image_index);


        bool                                         m_is_show_axis{ false };
        bool                                         m_enable_fxaa{ true };
        size_t                                       m_selected_axis{ 3 };
        MeshPerframeStorageBufferObject              m_mesh_perframe_storage_buffer_object;
        AxisStorageBufferObject                      m_axis_storage_buffer_object;
        VkImageView                                  m_color_input_image_view;
        VkImageView                                  m_bright_color_input_image_view;

        void updateAfterFramebufferRecreate(VkImageView color_input_attachment);

        VkCommandBuffer getRenderCommandBuffer();

    private:
        void setupAttachments();
        void setupRenderPass();
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
        void setupSwapchainFramebuffers();

        void setupAxisDescriptorSet();

        void drawAxis();


    private:
        std::vector<VkFramebuffer> m_swapchain_framebuffers;
    };
} // namespace Piccolo
