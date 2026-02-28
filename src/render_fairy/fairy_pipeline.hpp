#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>
#include "ktm/ktm.h"
#include "../fairy_viewer.hpp"

namespace fv
{

class GpuBuffer;
class GpuTexture;
class FairyPipeline
{
public:
    FairyPipeline();
    FV_DELETE_COPY_MOVE(FairyPipeline)
    ~FairyPipeline();

    // update
    void Update_iResolution(const ktm::fvec3& i_resolution);
    void Update_iTime(float i_time);
    void Update_iTimeDelta(float i_time_delta);
    void Update_iFrameRate(float i_frame_rate);
    void Update_iFrame(int i_frame);
    void Update_iChannelTime(int index);
    void Update_iChannelResolution(int index);
    void Update_iMouse(const ktm::fvec4& i_mouse);
    void Update_iChannel(int index);
    void Update_iDate(const ktm::fvec4& i_date);

    // reset
    bool Reset(vk::RenderPass render_pass, const std::string& shader);
    FV_INLINE const std::string& ResetErrorMessage() const { return reset_error_message_; }

    // getter
    FV_INLINE vk::PipelineLayout PipelineLayout() const { return pipeline_layout_; }
    FV_INLINE vk::Pipeline Pipeline() const { return pipeline_; }

    vk::Buffer IndexBuffer() const;
    FV_INLINE uint32_t IndexCount() const { return indices_count_; }
    FV_INLINE const std::vector<vk::DescriptorSet>& DescriptorSets() const { return descriptor_sets_; }

private:
    // pipeline context
    void CreateDescriptorSetLayouts();
    void CreateDescriptorPoolAndSets();
    void CreatePipelineLayout();
    void CreateVertexShader();
    bool CreateFragmentShader(const std::string& shader);
    void CreatePipeline(vk::RenderPass render_pass);
    void ClearFragmentShaderAndPipeline();

    // pipeline resource
    void CreateDrawIndices();
    void CreateDrawResource();
    void BindResourceToDescriptSets();

    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts_;
    vk::DescriptorPool descriptor_pool_;
    std::vector<vk::DescriptorSet> descriptor_sets_;
    vk::PipelineLayout pipeline_layout_;
    vk::ShaderModule vertex_shader_;
    vk::ShaderModule fragment_shader_;
    vk::Pipeline pipeline_;

    vk::IndexType indices_type_;
    uint32_t indices_count_;
    std::unique_ptr<GpuBuffer> indices_buffer_;
    std::unique_ptr<GpuBuffer> i_resolution_buffer_;
    std::unique_ptr<GpuBuffer> i_time_buffer_;
    std::unique_ptr<GpuBuffer> i_time_delta_buffer_;
    std::unique_ptr<GpuBuffer> i_frame_rate_buffer_;
    std::unique_ptr<GpuBuffer> i_frame_buffer_;
    std::unique_ptr<GpuBuffer> i_channel_time_4_buffer_;
    std::unique_ptr<GpuBuffer> i_channel_resolution_4_buffer_;
    std::unique_ptr<GpuBuffer> i_mouse_buffer_;
    std::unique_ptr<GpuTexture> i_channel_4_texture_[4];
    std::unique_ptr<GpuBuffer> i_date_buffer_;

    std::string reset_error_message_;
};

} // namespace fv