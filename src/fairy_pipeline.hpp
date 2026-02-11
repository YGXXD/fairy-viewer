#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>
#include "fairy_viewer.hpp"

namespace fv
{

class FairyPipeline
{
public:
    FairyPipeline(vk::RenderPass render_pass, uint32_t subpass_index);
    FV_DELETE_COPY_MOVE(FairyPipeline)
    ~FairyPipeline();

    FV_INLINE const std::vector<vk::DescriptorSetLayout>& DescriptorSetLayouts() const
    {
        return descriptor_set_layouts_;
    }
    FV_INLINE vk::DescriptorPool DescriptorPool() const { return descriptor_pool_; }
    FV_INLINE vk::PipelineLayout PipelineLayout() const { return pipeline_layout_; }
    FV_INLINE vk::Pipeline Pipeline() const { return pipeline_; }

private:
    void CreateShaders();
    void CreateDescriptorSetLayouts();
    void CreateDescriptorPool();
    void CreatePipelineLayout();
    void CreatePipeline(vk::RenderPass render_pass, uint32_t subpass_index);

    vk::ShaderModule vertex_shader_;
    vk::ShaderModule fragment_shader_;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts_;
    vk::DescriptorPool descriptor_pool_;
    vk::PipelineLayout pipeline_layout_;
    vk::Pipeline pipeline_;
};

} // namespace fv