#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    struct BlurPassInitInfo : RenderPassInitInfo
    {
        VkImage     input_attachment_image;
        VkImageView  input_attachment_view;
    };

    struct BlurPassPushConstantObject
    {
        std::pair<int, int> direction;
        int                 blur_kernal_size;
    };

    class BlurPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void preparePassData(std::shared_ptr<RenderResourceBase> render_resource) override final;
        void gassBlur();

    private:
        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();
        void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
        void updateDescriptorSet();

    private:
        VkCommandBuffer m_compute_command_buffer;
        VkImage         m_temp_blur_image;
        VkImageView     m_temp_blur_image_view;
        VkDeviceMemory  m_temp_blur_image_mem;

        VkImage     m_input_attachment_image;
        VkImageView m_input_attachment_view;

        BlurPassPushConstantObject m_blur_pass_push_constant_object;
        size_t  m_blur_times;
    };
} // namespace Piccolo
