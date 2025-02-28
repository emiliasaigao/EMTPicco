#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct SSAOBlurPassInitInfo : RenderPassInitInfo
    {
        VkRenderPass render_pass;
        VkImageView  color_input_attachment;
        VkImageView  ssao_attachment;
    };

    class SSAOBlurPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void updateAfterFramebufferRecreate(VkImageView color_input_attachment, VkImageView ssao_attachment);

    private:
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
        float m_brightness_threshold;
    };
} // namespace Piccolo
