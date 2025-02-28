#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct VignettePassInitInfo : RenderPassInitInfo
    {
        VkRenderPass render_pass;
        VkImageView  input_attachment;
    };

    struct VignettePushConstantObject
    {
        float vignette_cutoff;
        float vignette_exponent;
    };

    class VignettePass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void updateAfterFramebufferRecreate(VkImageView input_attachment);

    private:
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();

    private:
        VignettePushConstantObject m_vignette_push_constant_object;
    };
} // namespace Piccolo
