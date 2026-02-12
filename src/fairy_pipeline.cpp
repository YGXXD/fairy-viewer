#include "fairy_pipeline.hpp"
#include "render_core/gpu_context.hpp"
#include "shaders.hpp"
#include "shaderc/shaderc.hpp"

#if defined(FV_DEBUG_ENABLE)
#    include <iostream>
#endif

namespace fv
{

std::vector<uint32_t> CompileShader(const char* source, size_t source_size, shaderc_shader_kind kind)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, source_size, kind, "shader", options);
    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
#if defined(FV_DEBUG_ENABLE)
        std::cerr << "failed to compile shader: " << module.GetErrorMessage() << std::endl;
#endif
        return {};
    }
    std::vector<uint32_t> spirv = { module.cbegin(), module.cend() };
    return std::move(spirv);
}

FairyPipeline::FairyPipeline(vk::RenderPass render_pass, uint32_t subpass_index)
{
    CreateShaders();
    CreateDescriptorSetLayouts();
    CreateDescriptorPool();
    CreatePipelineLayout();
    CreatePipeline(render_pass, subpass_index);
}

FairyPipeline::~FairyPipeline()
{
    vk::Device device = GpuContext::Get().device;
    device.destroyPipeline(pipeline_);
    device.destroyPipelineLayout(pipeline_layout_);
    device.destroyDescriptorPool(descriptor_pool_);
    for (auto layout : descriptor_set_layouts_)
        device.destroyDescriptorSetLayout(layout);
    device.destroyShaderModule(vertex_shader_);
    device.destroyShaderModule(fragment_shader_);
}

void FairyPipeline::CreateShaders()
{
    const char* source = R"(
        #version 450

        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 0) uniform shaderInputs {
            vec3 iResolution;
        } ub;

        vec3 iResolution = ub.iResolution;

        void mainImage(out vec4 fragColor, in vec2 fragCoord)
        {
            vec2 uv = fragCoord / iResolution.xy;
            fragColor = vec4(uv, 0.0, 1.0);
        }

        void main() 
        {
            vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y);
            mainImage(outColor, fragCoord);
        }
    )";
    std::vector<uint32_t> fragment_shader_spirv = CompileShader(source, strlen(source), shaderc_shader_kind::shaderc_glsl_fragment_shader);

    vk::ShaderModuleCreateInfo shader_create_info = {};
    shader_create_info.codeSize = shader::fairy_vert_len;
    shader_create_info.pCode = reinterpret_cast<const uint32_t*>(shader::fairy_vert);
    vertex_shader_ = GpuContext::Get().device.createShaderModule(shader_create_info);

    shader_create_info.codeSize = shader::fairy_frag_len;
    shader_create_info.pCode = reinterpret_cast<const uint32_t*>(shader::fairy_frag);
    fragment_shader_ = GpuContext::Get().device.createShaderModule(shader_create_info);
}

void FairyPipeline::CreateDescriptorSetLayouts()
{
    vk::DescriptorSetLayoutBinding i_resolution_binding = {};
    i_resolution_binding.binding = 0;
    i_resolution_binding.descriptorCount = 1;
    i_resolution_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    i_resolution_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    std::vector<vk::DescriptorSetLayoutBinding> set0_bindings = { i_resolution_binding };
    descriptor_set_layouts_.push_back(GpuContext::Get().device.createDescriptorSetLayout(
        vk::DescriptorSetLayoutCreateInfo({}, set0_bindings.size(), set0_bindings.data())));
}

void FairyPipeline::CreateDescriptorPool()
{
    vk::DescriptorPoolSize i_resolution_pool_size = {};
    i_resolution_pool_size.type = vk::DescriptorType::eUniformBuffer;
    i_resolution_pool_size.descriptorCount = 1;
    std::vector<vk::DescriptorPoolSize> pool_sizes = { i_resolution_pool_size };
    vk::DescriptorPoolCreateInfo descriptor_pool_create_info = {};
    descriptor_pool_create_info.poolSizeCount = pool_sizes.size();
    descriptor_pool_create_info.pPoolSizes = pool_sizes.data();
    descriptor_pool_create_info.maxSets = 1;
    descriptor_pool_create_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptor_pool_ = GpuContext::Get().device.createDescriptorPool(descriptor_pool_create_info);
}

void FairyPipeline::CreatePipelineLayout()
{
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.setLayoutCount = descriptor_set_layouts_.size();
    pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts_.data();
    pipeline_layout_ = GpuContext::Get().device.createPipelineLayout(pipeline_layout_create_info);
}

void FairyPipeline::CreatePipeline(vk::RenderPass render_pass, uint32_t subpass_index)
{
    std::vector<vk::DynamicState> dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamic_states_create_info = {};
    dynamic_states_create_info.pDynamicStates = dynamic_states.data();
    dynamic_states_create_info.dynamicStateCount = dynamic_states.size();

    vk::PipelineVertexInputStateCreateInfo vertex_input_state = {};
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_state = {};
    input_assembly_state.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = nullptr;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = nullptr;

    vk::PipelineRasterizationStateCreateInfo rasterization_state = {};
    rasterization_state.depthClampEnable = true;
    rasterization_state.polygonMode = vk::PolygonMode::eFill;
    rasterization_state.cullMode = vk::CullModeFlagBits::eBack;
    rasterization_state.frontFace = vk::FrontFace::eClockwise;
    rasterization_state.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisample_state = {};
    multisample_state.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineShaderStageCreateInfo vertex_shader_stage_info = {};
    vertex_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vertex_shader_stage_info.module = vertex_shader_;
    vertex_shader_stage_info.pName = "main";
    vk::PipelineShaderStageCreateInfo fragment_shader_stage_info = {};
    fragment_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    fragment_shader_stage_info.module = fragment_shader_;
    fragment_shader_stage_info.pName = "main";
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = { vertex_shader_stage_info,
                                                                     fragment_shader_stage_info };

    vk::PipelineColorBlendAttachmentState color_blend_state = {};
    color_blend_state.blendEnable = true;
    color_blend_state.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    color_blend_state.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    color_blend_state.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    color_blend_state.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    color_blend_state.colorBlendOp = vk::BlendOp::eAdd;
    color_blend_state.alphaBlendOp = vk::BlendOp::eAdd;
    color_blend_state.colorWriteMask |= vk::ColorComponentFlagBits::eR;
    color_blend_state.colorWriteMask |= vk::ColorComponentFlagBits::eG;
    color_blend_state.colorWriteMask |= vk::ColorComponentFlagBits::eB;
    color_blend_state.colorWriteMask |= vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo color_blend_info = {};
    color_blend_info.logicOpEnable = false;
    color_blend_info.logicOp = vk::LogicOp::eCopy;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_state;

    vk::GraphicsPipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.pVertexInputState = &vertex_input_state;
    pipeline_create_info.pInputAssemblyState = &input_assembly_state;
    pipeline_create_info.pViewportState = &viewport_state;
    pipeline_create_info.pRasterizationState = &rasterization_state;
    pipeline_create_info.pMultisampleState = &multisample_state;
    pipeline_create_info.pDepthStencilState = nullptr;
    pipeline_create_info.pColorBlendState = &color_blend_info;
    pipeline_create_info.pDynamicState = nullptr;
    pipeline_create_info.stageCount = shader_stages.size();
    pipeline_create_info.pStages = shader_stages.data();
    pipeline_create_info.layout = pipeline_layout_;
    pipeline_create_info.pDynamicState = &dynamic_states_create_info;
    pipeline_create_info.renderPass = render_pass;
    pipeline_create_info.subpass = subpass_index;
    pipeline_ = GpuContext::Get().device.createGraphicsPipeline(nullptr, pipeline_create_info).value;
}

} // namespace fv