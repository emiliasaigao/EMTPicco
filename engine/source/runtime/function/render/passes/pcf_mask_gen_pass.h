#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    class RenderResourceBase;

    struct PCFMaskGenPushConstantsObject
    {
        Matrix4x4 inverse_proj_view_matrix;
        Matrix4x4 light_proj_view_matrix;
    };

    class PCFMaskGenPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void draw() override final;

    public:
        VkImageView                   m_directional_light_shadow_color_image_view;

    private:
        void setupAttachments();
        void setupRenderPass();
        void setupFramebuffer();
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();

    private:
        PCFMaskGenPushConstantsObject m_pcf_mask_gen_push_constants_object;
    };
} // namespace Piccolo
