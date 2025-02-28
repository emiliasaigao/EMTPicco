#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct ColorGradingPassInitInfo : RenderPassInitInfo
    {
        VkRenderPass render_pass;
        VkImageView  input_attachment;
    };

    struct ColorGradingConstantObject
    {
        float color_grading_effect;
        float brightness;
        float contrast;
        float saturation;
        float temperature;       
    };
    class ColorGradingPass : public RenderPass
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
        ColorGradingConstantObject m_color_grading_cosntant_object;
    };
} // namespace Piccolo
