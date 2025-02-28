#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct NBRPassInitInfo : RenderPassInitInfo
    {
        VkRenderPass render_pass;
    };

    struct NBROutlinePushConstantObject
    {
        float outline_width;
        float outline_z_offset;
        float outline_gamma;
    };

    class NBRPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        VkImageView                                  m_depth_attachment;

    private:
        enum
        {
            _nbr_pipeline_type_eyes_and_eyebrows,
            _nbr_pipeline_type_face_and_mouth,
            _nbr_pipeline_type_body,
            _nbr_pipeline_type_hair,
            _nbr_pipeline_type_hair_alpha,
            _nbr_pipeline_type_eye_black,
            _nbr_pipeline_type_outline,
            _nbr_pipeline_type_count
        };

        enum
        {
            _nbr_mesh_eyes,
            _nbr_mesh_eyebrows,
            _nbr_mesh_face,
            _nbr_mesh_mouth,
            _nbr_mesh_body,
            _nbr_mesh_hair,
            _nbr_mesh_eye_black,
            _nbr_mesh_count
        };

        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
        NBRMeshPerframeStorageBufferObject           m_nbr_mesh_perframe_storage_buffer_object;
        NBROutlineMeshPerframeStorageBufferObject    m_nbr_outline_mesh_perframe_storage_buffer_object;
        NBROutlinePushConstantObject                 m_nbr_outline_push_constant_object;
    };
} //