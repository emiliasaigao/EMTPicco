#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    class RenderResourceBase;

    class PreDepthPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void postInitialize() override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void draw() override final;
        void setPerMeshLayout(const VkDescriptorSetLayout& layout) { m_per_mesh_layout = layout; }

    private:
        void setupAttachments();
        void setupRenderPass();
        void setupFramebuffer();
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
        void drawModel();

    private:
        VkDescriptorSetLayout m_per_mesh_layout;
        Matrix4x4 m_proj_view_matrix;
    };
} // namespace Piccolo
