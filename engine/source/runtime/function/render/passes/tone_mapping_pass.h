#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct ToneMappingPassInitInfo : RenderPassInitInfo
    {
        VkRenderPass render_pass;
        VkImageView  color_attachment;
        VkImageView  bright_color_attachment;
    };

    class ToneMappingPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;

        void updateAfterFramebufferRecreate(VkImageView color_attachment, VkImageView bright_color_attachment);

    private:
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
    };
} // namespace Piccolo
