#pragma once

#include <vector>
#include <memory>
#include <vulkan/vulkan.hpp>
#include "ktm/ktm.h"
#include "../fairy_viewer.hpp"

namespace gpu
{
class GpuBuffer;
class GpuTexture;
} // namespace gpu

namespace fairy
{

class FairyResource;
class FairyPipeline
{
public:
    FairyPipeline(vk::RenderPass render_pass, int resource_count);
    FV_DELETE_COPY_MOVE(FairyPipeline)
    ~FairyPipeline();

    // reset
    bool Reset(const std::string& shader);
    FV_INLINE const std::string& ResetErrorMessage() const { return reset_error_message_; }

    // getter
    FV_INLINE vk::PipelineLayout PipelineLayout() const { return pipeline_layout_; }
    FV_INLINE vk::Pipeline Pipeline() const { return pipeline_; }
    vk::Buffer IndexBuffer() const;
    FV_INLINE uint32_t IndexCount() const { return indices_count_; }
    FV_INLINE int ResourceCount() const { return resource_count_; }
    FV_INLINE const FairyResource& Resource(int index) const { return *resources_[index].get(); }
    FV_INLINE const vk::DescriptorSet& DescriptorSet(int index) const { return descriptor_sets_[index]; }

private:
    // pipeline context
    void CreateDescriptorSetLayouts();
    void CreatePipelineLayout();
    void CreateVertexShader();
    bool CreateFragmentShader(const std::string& shader);
    void CreatePipeline();
    void ClearFragmentShaderAndPipeline();

    // pipeline resource
    void CreateDescriptorPoolAndSets();
    void CreateDrawResource();
    void BindResourceToDescriptSets();

    vk::RenderPass render_pass_;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts_;
    vk::PipelineLayout pipeline_layout_;
    vk::ShaderModule vertex_shader_;
    vk::ShaderModule fragment_shader_;
    vk::Pipeline pipeline_;
    std::string reset_error_message_;

    vk::IndexType indices_type_;
    uint32_t indices_count_;
    std::unique_ptr<gpu::GpuBuffer> indices_buffer_;

    int resource_count_;
    vk::DescriptorPool descriptor_pool_;
    std::vector<vk::DescriptorSet> descriptor_sets_;
    std::vector<std::unique_ptr<FairyResource>> resources_;
};

} // namespace fairy