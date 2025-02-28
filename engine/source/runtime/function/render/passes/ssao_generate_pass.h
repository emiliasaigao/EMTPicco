#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct SSAOGeneratePassInitInfo : RenderPassInitInfo
    {
        VkRenderPass render_pass;
        VkImageView  normal_attachment;
    };

    class SSAOGeneratePass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void updateAfterFramebufferRecreate(VkImageView normal_attachment);

    private:
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
        SSAOGeneratePerframeStorageBufferObject m_ssao_generate_perframe_storage_buffer_object;
        VkImageView                             m_normal_attachment;
    };
} // namespace Piccolo