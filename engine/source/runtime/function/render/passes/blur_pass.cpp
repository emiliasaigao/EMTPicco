#include "runtime/function/render/rhi/vulkan/vulkan_rhi.h"
#include "runtime/function/render/rhi/vulkan/vulkan_util.h"

#include "runtime/function/render/passes/blur_pass.h"

#include "gass_blur_comp.h"

#include <stdexcept>

namespace Piccolo
{
    void BlurPass::initialize(const RenderPassInitInfo* init_info)
    {
        RenderPass::initialize(nullptr);

        const BlurPassInitInfo* _init_info = static_cast<const BlurPassInitInfo*>(init_info);
        m_input_attachment_image           = _init_info->input_attachment_image;
        m_input_attachment_view            = _init_info->input_attachment_view;

        setupDescriptorSetLayout();
        setupPipelines();
        setupDescriptorSet();


        VulkanUtil::createImage(m_vulkan_rhi->m_physical_device,
                                m_vulkan_rhi->m_device,
                                m_vulkan_rhi->m_swapchain_extent.width,
                                m_vulkan_rhi->m_swapchain_extent.height,
                                VK_FORMAT_R16G16B16A16_SFLOAT,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                m_temp_blur_image,
                                m_temp_blur_image_mem,
                                0,
                                1,
                                1);

        m_temp_blur_image_view = VulkanUtil::createImageView(m_vulkan_rhi->m_device,
                                                             m_temp_blur_image,
                                                             VK_FORMAT_R16G16B16A16_SFLOAT,
                                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                                             VK_IMAGE_VIEW_TYPE_2D,
                                                             1,
                                                             1);
        updateDescriptorSet();
    }

    void BlurPass::setupDescriptorSetLayout()
    {
        m_descriptor_infos.resize(2);

        VkDescriptorSetLayoutBinding blur_layout_bindings[2] = {};

        VkDescriptorSetLayoutBinding& input_color_binding = blur_layout_bindings[0];
        input_color_binding.binding                       = 0;
        input_color_binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        input_color_binding.descriptorCount = 1;
        input_color_binding.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding& output_color_binding = blur_layout_bindings[1];
        output_color_binding.binding                       = 1;
        output_color_binding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        output_color_binding.descriptorCount = 1;
        output_color_binding.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo blur_layout_create_info;
        blur_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        blur_layout_create_info.pNext = NULL;
        blur_layout_create_info.flags = 0;
        blur_layout_create_info.bindingCount =
            sizeof(blur_layout_bindings) / sizeof(blur_layout_bindings[0]);
        blur_layout_create_info.pBindings = blur_layout_bindings;

        if (VK_SUCCESS !=
            vkCreateDescriptorSetLayout(
                m_vulkan_rhi->m_device, &blur_layout_create_info, NULL, &m_descriptor_infos[0].layout))
        {
            throw std::runtime_error("create post process global layout");
        }
        m_descriptor_infos[1].layout = m_descriptor_infos[0].layout;
    }

    void BlurPass::preparePassData(std::shared_ptr<RenderResourceBase> render_resource)
    {
        const RenderResource* vulkan_resource = static_cast<const RenderResource*>(render_resource.get());
        if (vulkan_resource)
        {
            m_blur_times = vulkan_resource->m_render_global_effect_setting_object.bloomBlurTimes;
            m_blur_pass_push_constant_object.blur_kernal_size = vulkan_resource->m_render_global_effect_setting_object.bloomKernalSize;
        }
    }

    void BlurPass::setupPipelines()
    {
        m_render_pipelines.resize(1);

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(BlurPassPushConstantObject);

        VkDescriptorSetLayout descriptorset_layouts[2] = {m_descriptor_infos[0].layout, m_descriptor_infos[1].layout};
        VkPipelineLayoutCreateInfo pipeline_layout_create_info {};
        pipeline_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts    = descriptorset_layouts;
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(
                m_vulkan_rhi->m_device, &pipeline_layout_create_info, nullptr, &m_render_pipelines[0].layout) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("create post process pipeline layout");
        }

        VkComputePipelineCreateInfo computePipelineCreateInfo {};

        computePipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.layout = m_render_pipelines[0].layout;
        computePipelineCreateInfo.flags  = 0;

        VkPipelineShaderStageCreateInfo shaderStage = {};
        shaderStage.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStage.pName                           = "main";
        shaderStage.module = VulkanUtil::createShaderModule(m_vulkan_rhi->m_device, GASS_BLUR_COMP);
        shaderStage.pSpecializationInfo = nullptr;
        assert(shaderStage.module != VK_NULL_HANDLE);

        computePipelineCreateInfo.stage = shaderStage;
        if (VK_SUCCESS !=
            vkCreateComputePipelines(
                m_vulkan_rhi->m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,  &m_render_pipelines[0].pipeline))
        {
            throw std::runtime_error("create particle kickoff pipe");
        }
        
    }

    void BlurPass::setupDescriptorSet()
    {
        VkDescriptorSetLayout       descriptor_layout[2] = {m_descriptor_infos[0].layout, m_descriptor_infos[1].layout};
        VkDescriptorSetAllocateInfo blur_descriptor_set_alloc_info;
        blur_descriptor_set_alloc_info.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        blur_descriptor_set_alloc_info.pNext          = NULL;
        blur_descriptor_set_alloc_info.descriptorPool = m_vulkan_rhi->m_descriptor_pool;
        blur_descriptor_set_alloc_info.descriptorSetCount = 2;
        blur_descriptor_set_alloc_info.pSetLayouts        = descriptor_layout;

        VkDescriptorSet descriptor_sets[2];
        if (VK_SUCCESS != vkAllocateDescriptorSets(m_vulkan_rhi->m_device,
                                                   &blur_descriptor_set_alloc_info,
                                                   descriptor_sets))
        {
            throw std::runtime_error("allocate post process global descriptor set");
        }
        m_descriptor_infos[0].descriptor_set = descriptor_sets[0];
        m_descriptor_infos[1].descriptor_set = descriptor_sets[1];

    }

    void BlurPass::gassBlur()
    {
        m_compute_command_buffer = m_vulkan_rhi->m_bloom_blur_compute_command_buffers[m_vulkan_rhi->m_current_frame_index];
        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            VkDebugUtilsLabelEXT label_info = {
                VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "Gass Blur", {1.0f, 1.0f, 1.0f, 1.0f}};
            m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_compute_command_buffer, &label_info);
        }
        VkCommandBufferBeginInfo cmdBufInfo {};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VkSubmitInfo computeSubmitInfo {};
        computeSubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.pWaitDstStageMask  = 0;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers    = &m_compute_command_buffer;

        // compute pass
        if (VK_SUCCESS != m_vulkan_rhi->m_vk_begin_command_buffer(m_compute_command_buffer, &cmdBufInfo))
        {
            throw std::runtime_error("begin command buffer");
        }

        transitionImageLayout(m_temp_blur_image, 
                              VK_IMAGE_LAYOUT_UNDEFINED, 
                              VK_IMAGE_LAYOUT_GENERAL);


        for (size_t i = 0; i < m_blur_times; ++i) {

            if (m_vulkan_rhi->isDebugLabelEnabled())
            {
                VkDebugUtilsLabelEXT label_info = {
                    VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "Horizontal Blur", {1.0f, 1.0f, 1.0f, 1.0f}};
                m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_compute_command_buffer, &label_info);
            }
            m_blur_pass_push_constant_object.direction = {1, 0};
            vkCmdBindPipeline(m_compute_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_render_pipelines[0].pipeline);
            vkCmdPushConstants(m_compute_command_buffer, 
                               m_render_pipelines[0].layout, 
                               VK_SHADER_STAGE_COMPUTE_BIT, 
                               0, 
                               sizeof(BlurPassPushConstantObject),
                               &m_blur_pass_push_constant_object);
            vkCmdBindDescriptorSets(m_compute_command_buffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    m_render_pipelines[0].layout,
                                    0,
                                    1,
                                    &m_descriptor_infos[0].descriptor_set,
                                    0,
                                    0);
            vkCmdDispatch(m_compute_command_buffer,
                          m_vulkan_rhi->m_swapchain_extent.width / 16,
                          m_vulkan_rhi->m_swapchain_extent.height / 16,
                          1);

            transitionImageLayout(m_input_attachment_image, 
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                                  VK_IMAGE_LAYOUT_GENERAL);
            transitionImageLayout(m_temp_blur_image, 
                                  VK_IMAGE_LAYOUT_GENERAL, 
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            if (m_vulkan_rhi->isDebugLabelEnabled())
            {
                m_vulkan_rhi->m_vk_cmd_end_debug_utils_label_ext(m_compute_command_buffer);
            }

            if (m_vulkan_rhi->isDebugLabelEnabled())
            {
                VkDebugUtilsLabelEXT label_info = {
                    VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, NULL, "Vertical Blur", {1.0f, 1.0f, 1.0f, 1.0f}};
                m_vulkan_rhi->m_vk_cmd_begin_debug_utils_label_ext(m_compute_command_buffer, &label_info);
            }
            m_blur_pass_push_constant_object.direction = {0, 1};
            vkCmdBindPipeline(m_compute_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_render_pipelines[0].pipeline);
            vkCmdPushConstants(m_compute_command_buffer, 
                               m_render_pipelines[0].layout, 
                               VK_SHADER_STAGE_COMPUTE_BIT, 
                               0, 
                               sizeof(BlurPassPushConstantObject),
                               &m_blur_pass_push_constant_object);
            vkCmdBindDescriptorSets(m_compute_command_buffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    m_render_pipelines[0].layout,
                                    0,
                                    1,
                                    &m_descriptor_infos[1].descriptor_set,
                                    0,
                                    0);
            vkCmdDispatch(m_compute_command_buffer,
                          m_vulkan_rhi->m_swapchain_extent.width / 16,
                          m_vulkan_rhi->m_swapchain_extent.height / 16, 
                          1);

            transitionImageLayout(m_input_attachment_image, 
                                  VK_IMAGE_LAYOUT_GENERAL, 
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            transitionImageLayout(m_temp_blur_image, 
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                                  VK_IMAGE_LAYOUT_GENERAL);
            if (m_vulkan_rhi->isDebugLabelEnabled())
            {
                m_vulkan_rhi->m_vk_cmd_end_debug_utils_label_ext(m_compute_command_buffer);
            }

        }

        if (VK_SUCCESS != m_vulkan_rhi->m_vk_end_command_buffer(m_compute_command_buffer))
        {
            throw std::runtime_error("end command buffer");
        }

        if (m_vulkan_rhi->isDebugLabelEnabled())
        {
            m_vulkan_rhi->m_vk_cmd_end_debug_utils_label_ext(m_compute_command_buffer);
        }

        // submit compute mission
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
        VkSubmitInfo         submit_info   = {};
        submit_info.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount     = 1;
        submit_info.pWaitSemaphores        = &m_vulkan_rhi->m_image_available_for_bloom_blur_semaphores[m_vulkan_rhi->m_current_frame_index];
        submit_info.pWaitDstStageMask      = wait_stages;
        submit_info.commandBufferCount     = 1;
        submit_info.pCommandBuffers        = &m_compute_command_buffer;
        submit_info.signalSemaphoreCount   = 1;
        submit_info.pSignalSemaphores      = &m_vulkan_rhi->m_image_available_for_post_process_semaphores[m_vulkan_rhi->m_current_frame_index];
        VkResult res_queue_submit = vkQueueSubmit(m_vulkan_rhi->m_compute_queue, 1, &submit_info, VK_NULL_HANDLE);
    }

    void BlurPass::transitionImageLayout(VkImage image, 
                           VkImageLayout oldLayout, 
                           VkImageLayout newLayout) 
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage, destinationStage;
        if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && 
            newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } 
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && 
            newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } 
        else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && 
                    newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }

        vkCmdPipelineBarrier(m_compute_command_buffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void BlurPass::updateDescriptorSet() 
    {
        VkDescriptorImageInfo input_attachment_info_set0 {};
        input_attachment_info_set0.sampler =
            VulkanUtil::getOrCreateNearestSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
        input_attachment_info_set0.imageView   = m_input_attachment_view;
        input_attachment_info_set0.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo output_attachment_info_set0 {};
        output_attachment_info_set0.sampler     = VK_NULL_HANDLE;
        output_attachment_info_set0.imageView   = m_temp_blur_image_view;
        output_attachment_info_set0.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo input_attachment_info_set1 {};
        input_attachment_info_set1.sampler =
            VulkanUtil::getOrCreateNearestSampler(m_vulkan_rhi->m_physical_device, m_vulkan_rhi->m_device);
        input_attachment_info_set1.imageView   = m_temp_blur_image_view; 
        input_attachment_info_set1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo output_attachment_info_set1 {};
        output_attachment_info_set1.sampler     = VK_NULL_HANDLE;
        output_attachment_info_set1.imageView   = m_input_attachment_view; 
        output_attachment_info_set1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet blur_descriptor_writes_info_odd[2];

        blur_descriptor_writes_info_odd[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blur_descriptor_writes_info_odd[0].pNext           = NULL;
        blur_descriptor_writes_info_odd[0].dstSet          = m_descriptor_infos[0].descriptor_set;
        blur_descriptor_writes_info_odd[0].dstBinding      = 0;
        blur_descriptor_writes_info_odd[0].dstArrayElement = 0;
        blur_descriptor_writes_info_odd[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blur_descriptor_writes_info_odd[0].descriptorCount = 1;
        blur_descriptor_writes_info_odd[0].pImageInfo      = &input_attachment_info_set0;

        blur_descriptor_writes_info_odd[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blur_descriptor_writes_info_odd[1].pNext           = NULL;
        blur_descriptor_writes_info_odd[1].dstSet          = m_descriptor_infos[0].descriptor_set;
        blur_descriptor_writes_info_odd[1].dstBinding      = 1;
        blur_descriptor_writes_info_odd[1].dstArrayElement = 0;
        blur_descriptor_writes_info_odd[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        blur_descriptor_writes_info_odd[1].descriptorCount = 1;
        blur_descriptor_writes_info_odd[1].pImageInfo      = &output_attachment_info_set0;

        vkUpdateDescriptorSets(m_vulkan_rhi->m_device, 2, blur_descriptor_writes_info_odd, 0, NULL);

        VkWriteDescriptorSet blur_descriptor_writes_info_even[2];
        blur_descriptor_writes_info_even[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blur_descriptor_writes_info_even[0].pNext           = NULL;
        blur_descriptor_writes_info_even[0].dstSet          = m_descriptor_infos[1].descriptor_set;
        blur_descriptor_writes_info_even[0].dstBinding      = 0;
        blur_descriptor_writes_info_even[0].dstArrayElement = 0;
        blur_descriptor_writes_info_even[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blur_descriptor_writes_info_even[0].descriptorCount = 1;
        blur_descriptor_writes_info_even[0].pImageInfo      = &input_attachment_info_set1;

        blur_descriptor_writes_info_even[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blur_descriptor_writes_info_even[1].pNext           = NULL;
        blur_descriptor_writes_info_even[1].dstSet          = m_descriptor_infos[1].descriptor_set;
        blur_descriptor_writes_info_even[1].dstBinding      = 1;
        blur_descriptor_writes_info_even[1].dstArrayElement = 0;
        blur_descriptor_writes_info_even[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        blur_descriptor_writes_info_even[1].descriptorCount = 1;
        blur_descriptor_writes_info_even[1].pImageInfo      = &output_attachment_info_set1;

        vkUpdateDescriptorSets(m_vulkan_rhi->m_device, 2, blur_descriptor_writes_info_even, 0, NULL);
    }

} // namespace Piccolo
