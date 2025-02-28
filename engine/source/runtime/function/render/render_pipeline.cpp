#include "runtime/function/render/render_pipeline.h"
#include "runtime/function/render/rhi/vulkan/vulkan_rhi.h"

#include "runtime/function/render/passes/color_grading_pass.h"
#include "runtime/function/render/passes/ssao_generate_pass.h"
#include "runtime/function/render/passes/ssao_blur_pass.h"
#include "runtime/function/render/passes/combine_ui_pass.h"
#include "runtime/function/render/passes/directional_light_pass.h"
#include "runtime/function/render/passes/main_camera_pass.h"
#include "runtime/function/render/passes/post_process_pass.h"
#include "runtime/function/render/passes/pick_pass.h"
#include "runtime/function/render/passes/point_light_pass.h"
#include "runtime/function/render/passes/tone_mapping_pass.h"
#include "runtime/function/render/passes/ui_pass.h"
#include "runtime/function/render/passes/particle_pass.h"
#include "runtime/function/render/passes/blur_pass.h"
#include "runtime/function/render/passes/pre_depth_pass.h"
#include "runtime/function/render/passes/pcf_mask_gen_pass.h"
#include "runtime/function/render/passes/pcf_mask_blur_pass.h"
#include "runtime/function/render/passes/nbr_pass.h"

#include "runtime/core/base/macro.h"

namespace Piccolo
{
    void RenderPipeline::initialize(RenderPipelineInitInfo init_info)
    {
        m_point_light_shadow_pass = std::make_shared<PointLightShadowPass>();
        m_directional_light_pass  = std::make_shared<DirectionalLightShadowPass>();
        m_main_camera_pass        = std::make_shared<MainCameraPass>();
        m_post_process_pass       = std::make_shared<PostProcessPass>();
        m_tone_mapping_pass       = std::make_shared<ToneMappingPass>();
        m_color_grading_pass      = std::make_shared<ColorGradingPass>();
        m_vignette_pass           = std::make_shared<VignettePass>();
        m_remap_pass              = std::make_shared<RemapPass>();
        m_ssao_generate_pass      = std::make_shared<SSAOGeneratePass>();
        m_ssao_blur_pass          = std::make_shared<SSAOBlurPass>();
        m_ui_pass                 = std::make_shared<UIPass>();
        m_combine_ui_pass         = std::make_shared<CombineUIPass>();
        m_pick_pass               = std::make_shared<PickPass>();
        m_fxaa_pass               = std::make_shared<FXAAPass>();
        m_particle_pass           = std::make_shared<ParticlePass>();
        m_blur_pass               = std::make_shared<BlurPass>();
        m_pre_depth_pass          = std::make_shared<PreDepthPass>();
        m_pcf_mask_gen_pass       = std::make_shared<PCFMaskGenPass>();
        m_pcf_mask_blur_pass      = std::make_shared<PCFMaskBlurPass>();
        m_nbr_pass                = std::make_shared<NBRPass>();

        RenderPassCommonInfo pass_common_info;
        pass_common_info.rhi             = m_rhi;
        pass_common_info.render_resource = init_info.render_resource;

        m_point_light_shadow_pass->setCommonInfo(pass_common_info);
        m_directional_light_pass->setCommonInfo(pass_common_info);
        m_main_camera_pass->setCommonInfo(pass_common_info);
        m_post_process_pass->setCommonInfo(pass_common_info);
        m_tone_mapping_pass->setCommonInfo(pass_common_info);
        m_ssao_generate_pass->setCommonInfo(pass_common_info);
        m_ssao_blur_pass->setCommonInfo(pass_common_info);
        m_color_grading_pass->setCommonInfo(pass_common_info);
        m_vignette_pass->setCommonInfo(pass_common_info);
        m_remap_pass->setCommonInfo(pass_common_info);
        m_ui_pass->setCommonInfo(pass_common_info);
        m_combine_ui_pass->setCommonInfo(pass_common_info);
        m_pick_pass->setCommonInfo(pass_common_info);
        m_fxaa_pass->setCommonInfo(pass_common_info);
        m_particle_pass->setCommonInfo(pass_common_info);
        m_blur_pass->setCommonInfo(pass_common_info);
        m_pre_depth_pass->setCommonInfo(pass_common_info);
        m_pcf_mask_gen_pass->setCommonInfo(pass_common_info);
        m_pcf_mask_blur_pass->setCommonInfo(pass_common_info);
        m_nbr_pass->setCommonInfo(pass_common_info);

        m_point_light_shadow_pass->initialize(nullptr);
        m_directional_light_pass->initialize(nullptr);
        m_pre_depth_pass->initialize(nullptr);

        std::shared_ptr<MainCameraPass> main_camera_pass = std::static_pointer_cast<MainCameraPass>(m_main_camera_pass);
        std::shared_ptr<RenderPass>     _main_camera_pass = std::static_pointer_cast<RenderPass>(m_main_camera_pass);
        std::shared_ptr<PostProcessPass> post_process_pass = std::static_pointer_cast<PostProcessPass>(m_post_process_pass);
        std::shared_ptr<RenderPass>   _post_process_pass = std::static_pointer_cast<RenderPass>(m_post_process_pass);
        std::shared_ptr<ParticlePass> particle_pass = std::static_pointer_cast<ParticlePass>(m_particle_pass);
        std::shared_ptr<PCFMaskGenPass> pcf_mask_gen_pass  = std::static_pointer_cast<PCFMaskGenPass>(m_pcf_mask_gen_pass);
        std::shared_ptr<PCFMaskBlurPass> pcf_mask_blur_pass  = std::static_pointer_cast<PCFMaskBlurPass>(m_pcf_mask_blur_pass);
        std::shared_ptr<NBRPass> nbr_pass  = std::static_pointer_cast<NBRPass>(m_nbr_pass);
        std::shared_ptr<RenderPass> _pre_depth_pass = std::static_pointer_cast<RenderPass>(m_pre_depth_pass);
        

        ParticlePassInitInfo particle_init_info{};
        particle_init_info.m_particle_manager = g_runtime_global_context.m_particle_manager;
        m_particle_pass->initialize(&particle_init_info);

        main_camera_pass->m_point_light_shadow_color_image_view =
            std::static_pointer_cast<RenderPass>(m_point_light_shadow_pass)->getFramebufferImageViews()[0];
        main_camera_pass->m_directional_light_shadow_color_image_view =
            std::static_pointer_cast<RenderPass>(m_directional_light_pass)->m_framebuffer.attachments[0].view;
        
        pcf_mask_gen_pass->m_directional_light_shadow_color_image_view =
            std::static_pointer_cast<RenderPass>(m_directional_light_pass)->m_framebuffer.attachments[0].view;
        m_pcf_mask_gen_pass->initialize(nullptr);

        pcf_mask_blur_pass->m_pcf_mask_gen_image_view =
            std::static_pointer_cast<RenderPass>(m_pcf_mask_gen_pass)->m_framebuffer.attachments[0].view;
        m_pcf_mask_blur_pass->initialize(nullptr);

        main_camera_pass->m_pcf_mask_image_view =
            std::static_pointer_cast<RenderPass>(m_pcf_mask_blur_pass)->m_framebuffer.attachments[0].view;

        main_camera_pass->setParticlePass(particle_pass);
        m_main_camera_pass->initialize(nullptr);

        std::static_pointer_cast<ParticlePass>(m_particle_pass)->setupParticlePass();

        std::vector<VkDescriptorSetLayout> descriptor_layouts = _main_camera_pass->getDescriptorSetLayouts();
        std::static_pointer_cast<PointLightShadowPass>(m_point_light_shadow_pass)
            ->setPerMeshLayout(descriptor_layouts[MainCameraPass::LayoutType::_per_mesh]);
        std::static_pointer_cast<PointLightShadowPass>(m_directional_light_pass)
            ->setPerMeshLayout(descriptor_layouts[MainCameraPass::LayoutType::_per_mesh]);
        std::static_pointer_cast<PointLightShadowPass>(m_pre_depth_pass)
            ->setPerMeshLayout(descriptor_layouts[MainCameraPass::LayoutType::_per_mesh]);

        m_point_light_shadow_pass->postInitialize();
        m_directional_light_pass->postInitialize();
        m_pre_depth_pass->postInitialize();

        NBRPassInitInfo nbr_init_info;
        nbr_init_info.render_pass = _main_camera_pass->getRenderPass();
        nbr_pass->m_depth_attachment = _pre_depth_pass->getFramebufferImageViews()[0];
        m_nbr_pass->initialize(&nbr_init_info);
        
        SSAOGeneratePassInitInfo ssao_generate_init_info;
        ssao_generate_init_info.render_pass = _main_camera_pass->getRenderPass();
        ssao_generate_init_info.normal_attachment =
            _main_camera_pass->getFramebufferImageViews()[_main_camera_pass_gbuffer_a];
        m_ssao_generate_pass->initialize(&ssao_generate_init_info);

        SSAOBlurPassInitInfo ssao_blur_init_info;
        ssao_blur_init_info.render_pass = _main_camera_pass->getRenderPass();
        ssao_blur_init_info.color_input_attachment =
            _main_camera_pass->getFramebufferImageViews()[_main_camera_pass_backup_buffer_odd];
        ssao_blur_init_info.ssao_attachment = 
            _main_camera_pass->getFramebufferImageViews()[_main_camera_pass_backup_buffer_even];
        m_ssao_blur_pass->initialize(&ssao_blur_init_info);

        PostProcessPassInitInfo post_process_init_info;
        post_process_init_info.enable_fxaa = false;
        post_process_init_info.color_input_image_view =
            _main_camera_pass->getFramebufferImageViews()[_main_camera_pass_color_output_image];
        post_process_init_info.bright_color_input_image_view =
            _main_camera_pass->getFramebufferImageViews()[_main_camera_pass_bright_color_output_image];
        m_post_process_pass->initialize(&post_process_init_info);

        ToneMappingPassInitInfo tone_mapping_init_info;
        tone_mapping_init_info.render_pass = _post_process_pass->getRenderPass();
        tone_mapping_init_info.color_attachment =
            _main_camera_pass->getFramebufferImageViews()[_main_camera_pass_color_output_image];
        tone_mapping_init_info.bright_color_attachment =
            _main_camera_pass->getFramebufferImageViews()[_main_camera_pass_bright_color_output_image];
        m_tone_mapping_pass->initialize(&tone_mapping_init_info);

        ColorGradingPassInitInfo color_grading_init_info;
        color_grading_init_info.render_pass = _post_process_pass->getRenderPass();
        color_grading_init_info.input_attachment =
            _post_process_pass->getFramebufferImageViews()[_post_process_pass_backup_buffer_odd];
        m_color_grading_pass->initialize(&color_grading_init_info);
        
        FXAAPassInitInfo fxaa_init_info;
        fxaa_init_info.render_pass = _post_process_pass->getRenderPass();
        fxaa_init_info.input_attachment =
            _post_process_pass->getFramebufferImageViews()[_post_process_pass_backup_buffer_even];
        m_fxaa_pass->initialize(&fxaa_init_info);
        
        VignettePassInitInfo vignette_init_info;
        vignette_init_info.render_pass = _post_process_pass->getRenderPass();
        vignette_init_info.input_attachment =
            _post_process_pass->getFramebufferImageViews()[_post_process_pass_backup_buffer_extra];
        m_vignette_pass->initialize(&vignette_init_info);

        RemapPassInitInfo remap_init_info;
        remap_init_info.render_pass = _post_process_pass->getRenderPass();
        remap_init_info.input_attachment =
            _post_process_pass->getFramebufferImageViews()[_post_process_pass_backup_buffer_ultra];
        m_remap_pass->initialize(&remap_init_info);

        UIPassInitInfo ui_init_info;
        ui_init_info.render_pass = _post_process_pass->getRenderPass();
        m_ui_pass->initialize(&ui_init_info);

        CombineUIPassInitInfo combine_ui_init_info;
        combine_ui_init_info.render_pass = _post_process_pass->getRenderPass();
        combine_ui_init_info.scene_input_attachment =
            _post_process_pass->getFramebufferImageViews()[_post_process_pass_backup_buffer_odd];
        combine_ui_init_info.ui_input_attachment =
            _post_process_pass->getFramebufferImageViews()[_post_process_pass_backup_buffer_even];
        m_combine_ui_pass->initialize(&combine_ui_init_info);

        PickPassInitInfo pick_init_info;
        pick_init_info.per_mesh_layout = descriptor_layouts[MainCameraPass::LayoutType::_per_mesh];
        m_pick_pass->initialize(&pick_init_info);


        BlurPassInitInfo blur_pass_init_info;
        blur_pass_init_info.input_attachment_image = static_cast<MainCameraPass*>(m_main_camera_pass.get())
                                                         ->getFramebufferImages()[_main_camera_pass_bright_color_output_image];
        blur_pass_init_info.input_attachment_view = static_cast<MainCameraPass*>(m_main_camera_pass.get())
                                                         ->getFramebufferImageViews()[_main_camera_pass_bright_color_output_image];
        m_blur_pass->initialize(&blur_pass_init_info);

    }

    void RenderPipeline::forwardRender(std::shared_ptr<RHI> rhi, std::shared_ptr<RenderResourceBase> render_resource)
    {
        VulkanRHI*      vulkan_rhi      = static_cast<VulkanRHI*>(rhi.get());
        RenderResource* vulkan_resource = static_cast<RenderResource*>(render_resource.get());

        vulkan_resource->resetRingBufferOffset(vulkan_rhi->m_current_frame_index);

        vulkan_rhi->waitForFences();

        vulkan_rhi->resetCommandPool();

        bool recreate_swapchain =
            vulkan_rhi->prepareBeforePass(std::bind(&RenderPipeline::passUpdateAfterRecreateSwapchain, this));
        if (recreate_swapchain)
        {
            return;
        }

        static_cast<DirectionalLightShadowPass*>(m_directional_light_pass.get())->draw();

        static_cast<PointLightShadowPass*>(m_point_light_shadow_pass.get())->draw();

        ColorGradingPass& color_grading_pass = *(static_cast<ColorGradingPass*>(m_color_grading_pass.get()));
        VignettePass&     vignette_pass      = *(static_cast<VignettePass*>(m_vignette_pass.get()));
        RemapPass&        remap_pass         = *(static_cast<RemapPass*>(m_remap_pass.get()));
        FXAAPass&         fxaa_pass          = *(static_cast<FXAAPass*>(m_fxaa_pass.get()));
        ToneMappingPass&  tone_mapping_pass  = *(static_cast<ToneMappingPass*>(m_tone_mapping_pass.get()));
        SSAOBlurPass&     ssao_blur_pass     = *(static_cast<SSAOBlurPass*>(m_ssao_blur_pass.get()));
        SSAOGeneratePass& ssao_generate_pass = *(static_cast<SSAOGeneratePass*>(m_ssao_generate_pass.get()));
        UIPass&           ui_pass            = *(static_cast<UIPass*>(m_ui_pass.get()));
        CombineUIPass&    combine_ui_pass    = *(static_cast<CombineUIPass*>(m_combine_ui_pass.get()));
        ParticlePass&     particle_pass      = *(static_cast<ParticlePass*>(m_particle_pass.get()));

        static_cast<ParticlePass*>(m_particle_pass.get())
            ->setRenderCommandBufferHandle(
                static_cast<MainCameraPass*>(m_main_camera_pass.get())->getRenderCommandBuffer());

        static_cast<MainCameraPass*>(m_main_camera_pass.get())
            ->drawForward(ssao_blur_pass,
                          ssao_generate_pass,
                          particle_pass,
                          vulkan_rhi->m_current_swapchain_image_index);

        vulkan_rhi->submitRendering(std::bind(&RenderPipeline::passUpdateAfterRecreateSwapchain, this));
        static_cast<ParticlePass*>(m_particle_pass.get())->copyNormalAndDepthImage();
        static_cast<ParticlePass*>(m_particle_pass.get())->simulate();
    }

    void RenderPipeline::deferredRender(std::shared_ptr<RHI> rhi, std::shared_ptr<RenderResourceBase> render_resource)
    {
        VulkanRHI*      vulkan_rhi      = static_cast<VulkanRHI*>(rhi.get());
        RenderResource* vulkan_resource = static_cast<RenderResource*>(render_resource.get());

        vulkan_resource->resetRingBufferOffset(vulkan_rhi->m_current_frame_index);

        vulkan_rhi->waitForFences();

        vulkan_rhi->resetCommandPool();

        bool recreate_swapchain =
            vulkan_rhi->prepareBeforePass(std::bind(&RenderPipeline::passUpdateAfterRecreateSwapchain, this));
        if (recreate_swapchain)
        {
            return;
        }

         // begin command buffer
        VkCommandBufferBeginInfo command_buffer_begin_info {};
        command_buffer_begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        command_buffer_begin_info.flags            = 0;
        command_buffer_begin_info.pInheritanceInfo = nullptr;

        VkResult res_begin_command_buffer = vulkan_rhi->m_vk_begin_command_buffer(
            vulkan_rhi->m_command_buffers[vulkan_rhi->m_current_frame_index], &command_buffer_begin_info);
        assert(VK_SUCCESS == res_begin_command_buffer);

        static_cast<DirectionalLightShadowPass*>(m_directional_light_pass.get())->draw();

        static_cast<PointLightShadowPass*>(m_point_light_shadow_pass.get())->draw();

        static_cast<PreDepthPass*>(m_pre_depth_pass.get())->draw();

        static_cast<PCFMaskGenPass*>(m_pcf_mask_gen_pass.get())->draw();

        static_cast<PCFMaskBlurPass*>(m_pcf_mask_blur_pass.get())->draw();

        ColorGradingPass& color_grading_pass = *(static_cast<ColorGradingPass*>(m_color_grading_pass.get()));
        VignettePass&     vignette_pass      = *(static_cast<VignettePass*>(m_vignette_pass.get()));
        RemapPass&        remap_pass         = *(static_cast<RemapPass*>(m_remap_pass.get()));
        FXAAPass&         fxaa_pass          = *(static_cast<FXAAPass*>(m_fxaa_pass.get()));
        ToneMappingPass&  tone_mapping_pass  = *(static_cast<ToneMappingPass*>(m_tone_mapping_pass.get()));
        NBRPass&          nbr_pass           = *(static_cast<NBRPass*>(m_nbr_pass.get()));
        SSAOBlurPass&     ssao_blur_pass     = *(static_cast<SSAOBlurPass*>(m_ssao_blur_pass.get()));
        SSAOGeneratePass& ssao_generate_pass = *(static_cast<SSAOGeneratePass*>(m_ssao_generate_pass.get()));
        UIPass&           ui_pass            = *(static_cast<UIPass*>(m_ui_pass.get()));
        CombineUIPass&    combine_ui_pass    = *(static_cast<CombineUIPass*>(m_combine_ui_pass.get()));
        ParticlePass&     particle_pass      = *(static_cast<ParticlePass*>(m_particle_pass.get()));

        static_cast<ParticlePass*>(m_particle_pass.get())
            ->setRenderCommandBufferHandle(
                static_cast<MainCameraPass*>(m_main_camera_pass.get())->getRenderCommandBuffer());

        static_cast<MainCameraPass*>(m_main_camera_pass.get())
            ->draw(nbr_pass,
                   ssao_blur_pass,
                   ssao_generate_pass,
                   particle_pass,
                   vulkan_rhi->m_current_swapchain_image_index);
        
         // end command buffer
        VkResult res_end_command_buffer_0 = vulkan_rhi->m_vk_end_command_buffer(
            vulkan_rhi->m_command_buffers[vulkan_rhi->m_current_frame_index]);
        assert(VK_SUCCESS == res_end_command_buffer_0);

        // first submit & release synchronization amount
        VkPipelineStageFlags wait_stages_0[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo         submit_info_0   = {};
        submit_info_0.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info_0.waitSemaphoreCount     = 1;
        submit_info_0.pWaitSemaphores        = &(vulkan_rhi->m_image_available_for_render_semaphores[vulkan_rhi->m_current_frame_index]);
        submit_info_0.pWaitDstStageMask      = wait_stages_0;
        submit_info_0.commandBufferCount     = 1;
        submit_info_0.pCommandBuffers        = &(vulkan_rhi->m_command_buffers[vulkan_rhi->m_current_frame_index]);
        submit_info_0.signalSemaphoreCount   = 1;
        submit_info_0.pSignalSemaphores      = &(vulkan_rhi->m_image_available_for_bloom_blur_semaphores[vulkan_rhi->m_current_frame_index]);
        VkResult res_queue_submit_0 = vkQueueSubmit(vulkan_rhi->m_graphics_queue,
                                                    1,
                                                    &submit_info_0, 
                                                    VK_NULL_HANDLE);
        assert(VK_SUCCESS == res_queue_submit_0);


        static_cast<BlurPass*>(m_blur_pass.get())->gassBlur();

        VkResult res_begin_post_process_command_buffer = vulkan_rhi->m_vk_begin_command_buffer(
            vulkan_rhi->m_post_process_command_buffers[vulkan_rhi->m_current_frame_index], &command_buffer_begin_info);
        assert(VK_SUCCESS == res_begin_post_process_command_buffer);

        static_cast<PostProcessPass*>(m_post_process_pass.get())
            ->draw(vignette_pass,
                   remap_pass,
                   color_grading_pass,
                   fxaa_pass,
                   tone_mapping_pass,
                   ui_pass,
                   combine_ui_pass,
                   vulkan_rhi->m_current_swapchain_image_index);

        // end command buffer
        VkResult res_end_command_buffer_1 = vulkan_rhi->m_vk_end_command_buffer(
            vulkan_rhi->m_post_process_command_buffers[vulkan_rhi->m_current_frame_index]);
        assert(VK_SUCCESS == res_end_command_buffer_1);

        VkSemaphore semaphores[2] = {
            vulkan_rhi->m_image_available_for_texturescopy_semaphores[vulkan_rhi->m_current_frame_index],
            vulkan_rhi->m_image_finished_for_presentation_semaphores[vulkan_rhi->m_current_frame_index]};
        // submit command buffer
        VkPipelineStageFlags wait_stages_1[] = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
        VkSubmitInfo         submit_info_1   = {};
        submit_info_1.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info_1.waitSemaphoreCount     = 1;
        submit_info_1.pWaitSemaphores        = &vulkan_rhi->m_image_available_for_post_process_semaphores[vulkan_rhi->m_current_frame_index];
        submit_info_1.pWaitDstStageMask      = wait_stages_1;
        submit_info_1.commandBufferCount     = 1;
        submit_info_1.pCommandBuffers        = &vulkan_rhi->m_post_process_command_buffers[vulkan_rhi->m_current_frame_index];
        submit_info_1.signalSemaphoreCount   = 2;
        submit_info_1.pSignalSemaphores      = semaphores;

        VkResult res_reset_fences = vulkan_rhi->m_vk_reset_fences(
            vulkan_rhi->m_device, 1, &vulkan_rhi->m_is_frame_in_flight_fences[vulkan_rhi->m_current_frame_index]);
        assert(VK_SUCCESS == res_reset_fences);

        VkResult res_queue_submit = vkQueueSubmit(vulkan_rhi->m_graphics_queue,
                                                  1,
                                                  &submit_info_1,
                                                  vulkan_rhi->m_is_frame_in_flight_fences[vulkan_rhi->m_current_frame_index]);
        assert(VK_SUCCESS == res_queue_submit);

        vulkan_rhi->submitRendering(std::bind(&RenderPipeline::passUpdateAfterRecreateSwapchain, this));
        static_cast<ParticlePass*>(m_particle_pass.get())->copyNormalAndDepthImage();
        static_cast<ParticlePass*>(m_particle_pass.get())->simulate();
    }

    void RenderPipeline::passUpdateAfterRecreateSwapchain()
    {
        MainCameraPass&   main_camera_pass   = *(static_cast<MainCameraPass*>(m_main_camera_pass.get()));
        PostProcessPass&  post_process_pass  = *(static_cast<PostProcessPass*>(m_main_camera_pass.get()));
        ColorGradingPass& color_grading_pass = *(static_cast<ColorGradingPass*>(m_color_grading_pass.get()));
        VignettePass&     vignette_pass      = *(static_cast<VignettePass*>(m_vignette_pass.get()));
        RemapPass&        remap_pass         = *(static_cast<RemapPass*>(m_remap_pass.get()));
        FXAAPass&         fxaa_pass          = *(static_cast<FXAAPass*>(m_fxaa_pass.get()));
        ToneMappingPass&  tone_mapping_pass  = *(static_cast<ToneMappingPass*>(m_tone_mapping_pass.get()));
        SSAOBlurPass&     ssao_blur_pass     = *(static_cast<SSAOBlurPass*>(m_ssao_blur_pass.get()));
        SSAOGeneratePass& ssao_generate_pass = *(static_cast<SSAOGeneratePass*>(m_ssao_generate_pass.get()));
        CombineUIPass&    combine_ui_pass    = *(static_cast<CombineUIPass*>(m_combine_ui_pass.get()));
        PickPass&         pick_pass          = *(static_cast<PickPass*>(m_pick_pass.get()));
        ParticlePass&     particle_pass      = *(static_cast<ParticlePass*>(m_particle_pass.get()));

        main_camera_pass.updateAfterFramebufferRecreate();
        ssao_generate_pass.updateAfterFramebufferRecreate(
            main_camera_pass.getFramebufferImageViews()[_main_camera_pass_gbuffer_a]);
        ssao_blur_pass.updateAfterFramebufferRecreate(
            main_camera_pass.getFramebufferImageViews()[_main_camera_pass_backup_buffer_odd],
            main_camera_pass.getFramebufferImageViews()[_main_camera_pass_backup_buffer_even]);
        tone_mapping_pass.updateAfterFramebufferRecreate(
            main_camera_pass.getFramebufferImageViews()[_main_camera_pass_color_output_image],
            main_camera_pass.getFramebufferImageViews()[_main_camera_pass_bright_color_output_image]);
        color_grading_pass.updateAfterFramebufferRecreate(
            post_process_pass.getFramebufferImageViews()[_post_process_pass_backup_buffer_odd]);
        fxaa_pass.updateAfterFramebufferRecreate(
            post_process_pass.getFramebufferImageViews()[_post_process_pass_backup_buffer_even]);
        vignette_pass.updateAfterFramebufferRecreate(
            post_process_pass.getFramebufferImageViews()[_post_process_pass_backup_buffer_extra]);
        remap_pass.updateAfterFramebufferRecreate(
            post_process_pass.getFramebufferImageViews()[_post_process_pass_backup_buffer_ultra]);
        combine_ui_pass.updateAfterFramebufferRecreate(
            post_process_pass.getFramebufferImageViews()[_post_process_pass_backup_buffer_odd],
            post_process_pass.getFramebufferImageViews()[_post_process_pass_backup_buffer_even]);
        pick_pass.recreateFramebuffer();
        particle_pass.updateAfterFramebufferRecreate();
    }
    uint32_t RenderPipeline::getGuidOfPickedMesh(const Vector2& picked_uv)
    {
        PickPass& pick_pass = *(static_cast<PickPass*>(m_pick_pass.get()));
        return pick_pass.pick(picked_uv);
    }

    void RenderPipeline::setAxisVisibleState(bool state)
    {
        PostProcessPass& post_process_pass = *(static_cast<PostProcessPass*>(m_post_process_pass.get()));
        post_process_pass.m_is_show_axis   = state;
    }

    void RenderPipeline::setSelectedAxis(size_t selected_axis)
    {
        PostProcessPass& post_process_pass = *(static_cast<PostProcessPass*>(m_post_process_pass.get()));
        post_process_pass.m_selected_axis = selected_axis;
    }
} // namespace Piccolo
