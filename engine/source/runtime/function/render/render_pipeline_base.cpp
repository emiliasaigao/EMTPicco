#include "runtime/function/render/render_pipeline_base.h"

#include "runtime/core/base/macro.h"

namespace Piccolo
{
    void RenderPipelineBase::preparePassData(std::shared_ptr<RenderResourceBase> render_resource)
    {
        m_main_camera_pass->preparePassData(render_resource);
        m_post_process_pass->preparePassData(render_resource);
        m_pick_pass->preparePassData(render_resource);
        m_directional_light_pass->preparePassData(render_resource);
        m_ssao_generate_pass->preparePassData(render_resource);
        m_ssao_blur_pass->preparePassData(render_resource);
        m_vignette_pass->preparePassData(render_resource);
        m_color_grading_pass->preparePassData(render_resource);
        m_blur_pass->preparePassData(render_resource);
        m_point_light_shadow_pass->preparePassData(render_resource);
        m_particle_pass->preparePassData(render_resource);
        m_pre_depth_pass->preparePassData(render_resource);
        m_pcf_mask_gen_pass->preparePassData(render_resource);
        m_nbr_pass->preparePassData(render_resource);
    }
    void RenderPipelineBase::forwardRender(std::shared_ptr<RHI>                rhi,
                                           std::shared_ptr<RenderResourceBase> render_resource)
    {}
    void RenderPipelineBase::deferredRender(std::shared_ptr<RHI>                rhi,
                                            std::shared_ptr<RenderResourceBase> render_resource)
    {}
    void RenderPipelineBase::initializeUIRenderBackend(WindowUI* window_ui)
    {
        m_ui_pass->initializeUIRenderBackend(window_ui);
    }
} // namespace Piccolo
