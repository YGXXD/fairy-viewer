#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>
#include "../fairy_viewer.hpp"

namespace fv
{

class GpuBuffer;
class FairyPipeline
{
public:
    FairyPipeline(vk::RenderPass render_pass);
    FV_DELETE_COPY_MOVE(FairyPipeline)
    ~FairyPipeline();

    // pipeline resource update
    void Update_iResolution(float x, float y, float z);

    // getter
    FV_INLINE vk::PipelineLayout PipelineLayout() const { return pipeline_layout_; }
    FV_INLINE vk::Pipeline Pipeline() const { return pipeline_; }

    vk::Buffer IndexBuffer() const;
    FV_INLINE uint32_t IndexCount() const { return indices_count_; }
    FV_INLINE vk::DescriptorSet DescriptorSet() const { return descriptor_set_; }

private:
    // pipeline context
    void CreateShaders();
    void CreateDescriptorSetLayouts();
    void CreateDescriptorPool();
    void CreatePipelineLayout();
    void CreatePipeline(vk::RenderPass render_pass);

    // pipeline resource
    void CreateDrawIndices();
    void CreateDrawResource();
    void CreateDescriptSet();

    vk::ShaderModule vertex_shader_;
    vk::ShaderModule fragment_shader_;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts_;
    vk::DescriptorPool descriptor_pool_;
    vk::PipelineLayout pipeline_layout_;
    vk::Pipeline pipeline_;

    vk::IndexType indices_type_;
    uint32_t indices_count_;
    std::unique_ptr<GpuBuffer> indices_buffer_;
    std::unique_ptr<GpuBuffer> i_resolution_buffer_;
    vk::DescriptorSet descriptor_set_;
};

} // namespace fv