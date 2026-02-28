#include "fairy_pipeline.hpp"
#include "../render_core/gpu_context.hpp"
#include "../render_core/gpu_buffer.hpp"
#include "../render_core/gpu_texture.hpp"
#include "shaders.hpp"
#include "shaderc/shaderc.hpp"

#include <sstream>
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

FairyPipeline::FairyPipeline(vk::RenderPass render_pass)
{
    // create pipeline context
    CreateShaders();
    CreateDescriptorSetLayouts();
    CreateDescriptorPoolAndSets();
    CreatePipelineLayout();
    CreatePipeline(render_pass);

    // create pipeline resource
    CreateDrawIndices();
    CreateDrawResource();
    BindResourceToDescriptSets();
}

FairyPipeline::~FairyPipeline()
{
    vk::Device device = GpuContext::Get().device;
    device.destroyPipeline(pipeline_);
    device.destroyPipelineLayout(pipeline_layout_);
    device.freeDescriptorSets(descriptor_pool_, descriptor_sets_);
    device.destroyDescriptorPool(descriptor_pool_);
    for (auto layout : descriptor_set_layouts_)
        device.destroyDescriptorSetLayout(layout);
    device.destroyShaderModule(vertex_shader_);
    device.destroyShaderModule(fragment_shader_);
}

void FairyPipeline::Update_iResolution(const ktm::fvec3& i_resolution)
{
    ktm::fvec3* i_resolution_ptr = static_cast<ktm::fvec3*>(i_resolution_buffer_->HostPointer());
    *i_resolution_ptr = i_resolution;
}

void FairyPipeline::Update_iTime(float i_time)
{
    float* i_time_ptr = static_cast<float*>(i_time_buffer_->HostPointer());
    *i_time_ptr = i_time;
}

void FairyPipeline::Update_iTimeDelta(float i_time_delta)
{
    float* i_time_delta_ptr = static_cast<float*>(i_time_delta_buffer_->HostPointer());
    *i_time_delta_ptr = i_time_delta;
}

void FairyPipeline::Update_iFrameRate(float i_frame_rate)
{
    float* i_frame_rate_ptr = static_cast<float*>(i_frame_rate_buffer_->HostPointer());
    *i_frame_rate_ptr = i_frame_rate;
}

void FairyPipeline::Update_iFrame(int i_frame)
{
    int* i_frame_ptr = static_cast<int*>(i_frame_buffer_->HostPointer());
    *i_frame_ptr = i_frame;
}

void FairyPipeline::Update_iChannelTime(int index)
{
    // todo
}

void FairyPipeline::Update_iChannelResolution(int index)
{
    // todo
}

void FairyPipeline::Update_iMouse(const ktm::fvec4& i_mouse)
{
    ktm::fvec4* i_mouse_ptr = static_cast<ktm::fvec4*>(i_mouse_buffer_->HostPointer());
    *i_mouse_ptr = i_mouse;
}

void FairyPipeline::Update_iChannel(int index)
{
    // todo
}

void FairyPipeline::Update_iDate(const ktm::fvec4& i_date)
{
    ktm::fvec4* i_date_ptr = static_cast<ktm::fvec4*>(i_date_buffer_->HostPointer());
    *i_date_ptr = i_date;
}

vk::Buffer FairyPipeline::IndexBuffer() const
{
    return indices_buffer_->Buffer();
}

void FairyPipeline::CreateShaders()
{
    std::stringstream glsl_source_builder;
    const char* source_0 = R"(
        #version 450

        layout(set = 0, binding = 0) uniform input00 { vec3 iResolution; } s0b0;
        layout(set = 0, binding = 1) uniform input01 { float iTime; } s0b1;
        layout(set = 0, binding = 2) uniform input02 { float iTimeDelta; } s0b2;
        layout(set = 0, binding = 3) uniform input03 { float iFrameRate; } s0b3;
        layout(set = 0, binding = 4) uniform input04 { int iFrame; } s0b4;
        layout(set = 0, binding = 5) uniform input05 { vec4 iMouse; } s0b5;
        layout(set = 0, binding = 6) uniform input06 { vec4 iDate; } s0b6;

        vec3 iResolution = s0b0.iResolution;
        float iTime = s0b1.iTime;
        float iTimeDelta = s0b2.iTimeDelta;
        float iFrameRate = s0b3.iFrameRate;
        int iFrame = s0b4.iFrame;
        vec4 iMouse = s0b5.iMouse;
        vec4 iDate = s0b6.iDate;
    )";
    const char* source_1 = R"(
        layout(location = 0) out vec4 outColor;
        void main()
        {
            vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y);
            vec4 fragColor = vec4(0.0);
            mainImage(fragColor, fragCoord);
            outColor = vec4(fragColor.xyz, 1.0);
        }
    )";
    const char* source_body = R"(
        void mainImage( out vec4 fragColor, in vec2 fragCoord )
        {
            // Normalized pixel coordinates (from 0 to 1)
            vec2 uv = fragCoord/iResolution.xy;

            // Time varying pixel color
            vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

            // Output to screen
            fragColor = vec4(col,1.0);
        }
    )";
    glsl_source_builder << source_0 << source_body << source_1 << std::endl;
    std::string glsl_source = glsl_source_builder.str();
    std::vector<uint32_t> fragment_shader_spirv =
        CompileShader(glsl_source.c_str(), glsl_source.size(), shaderc_shader_kind::shaderc_glsl_fragment_shader);

    vk::ShaderModuleCreateInfo shader_create_info = {};
    shader_create_info.codeSize = shader::fairy_vert_len;
    shader_create_info.pCode = reinterpret_cast<const uint32_t*>(shader::fairy_vert);
    vertex_shader_ = GpuContext::Get().device.createShaderModule(shader_create_info);

    shader_create_info.codeSize = fragment_shader_spirv.size() * 4;
    shader_create_info.pCode = fragment_shader_spirv.data();
    fragment_shader_ = GpuContext::Get().device.createShaderModule(shader_create_info);
}

void FairyPipeline::CreateDescriptorSetLayouts()
{
    constexpr auto uniform_desc_binding_lambda = [](int binding_index)
    {
        vk::DescriptorSetLayoutBinding binding = {};
        binding.binding = binding_index;
        binding.descriptorCount = 1;
        binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
        return binding;
    };
    vk::DescriptorSetLayoutBinding i_resolution_binding = uniform_desc_binding_lambda(0);
    vk::DescriptorSetLayoutBinding i_time_binding = uniform_desc_binding_lambda(1);
    vk::DescriptorSetLayoutBinding i_time_delta_binding = uniform_desc_binding_lambda(2);
    vk::DescriptorSetLayoutBinding i_frame_rate_binding = uniform_desc_binding_lambda(3);
    vk::DescriptorSetLayoutBinding i_frame_binding = uniform_desc_binding_lambda(4);
    vk::DescriptorSetLayoutBinding i_mouse_binding = uniform_desc_binding_lambda(5);
    vk::DescriptorSetLayoutBinding i_date_binding = uniform_desc_binding_lambda(6);

    std::vector<vk::DescriptorSetLayoutBinding> set0_bindings = { i_resolution_binding, i_time_binding,
                                                                  i_time_delta_binding, i_frame_rate_binding,
                                                                  i_frame_binding,      i_mouse_binding,
                                                                  i_date_binding };
    descriptor_set_layouts_.push_back(GpuContext::Get().device.createDescriptorSetLayout(
        vk::DescriptorSetLayoutCreateInfo({}, set0_bindings.size(), set0_bindings.data())));
}

void FairyPipeline::CreateDescriptorPoolAndSets()
{
    constexpr auto uniform_desc_size_lambda = []()
    {
        vk::DescriptorPoolSize uniform_pool_size = {};
        uniform_pool_size.type = vk::DescriptorType::eUniformBuffer;
        uniform_pool_size.descriptorCount = 1;
        return uniform_pool_size;
    };
    vk::DescriptorPoolSize i_resolution_pool_size = uniform_desc_size_lambda();
    vk::DescriptorPoolSize i_time_pool_size = uniform_desc_size_lambda();
    vk::DescriptorPoolSize i_time_delta_pool_size = uniform_desc_size_lambda();
    vk::DescriptorPoolSize i_frame_rate_pool_size = uniform_desc_size_lambda();
    vk::DescriptorPoolSize i_frame_pool_size = uniform_desc_size_lambda();
    vk::DescriptorPoolSize i_mouse_pool_size = uniform_desc_size_lambda();
    vk::DescriptorPoolSize i_date_pool_size = uniform_desc_size_lambda();

    std::vector<vk::DescriptorPoolSize> pool_sizes = { i_resolution_pool_size, i_time_pool_size,
                                                       i_time_delta_pool_size, i_frame_rate_pool_size,
                                                       i_frame_pool_size,      i_mouse_pool_size,
                                                       i_date_pool_size };
    vk::DescriptorPoolCreateInfo descriptor_pool_create_info = {};
    descriptor_pool_create_info.poolSizeCount = pool_sizes.size();
    descriptor_pool_create_info.pPoolSizes = pool_sizes.data();
    descriptor_pool_create_info.maxSets = 1;
    descriptor_pool_create_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptor_pool_ = GpuContext::Get().device.createDescriptorPool(descriptor_pool_create_info);

    vk::DescriptorSetAllocateInfo ds_allocate_info = {};
    ds_allocate_info.pSetLayouts = descriptor_set_layouts_.data();
    ds_allocate_info.descriptorPool = descriptor_pool_;
    ds_allocate_info.descriptorSetCount = descriptor_set_layouts_.size();
    descriptor_sets_ = fv::GpuContext::Get().device.allocateDescriptorSets(ds_allocate_info);
}

void FairyPipeline::CreatePipelineLayout()
{
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.setLayoutCount = descriptor_set_layouts_.size();
    pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts_.data();
    pipeline_layout_ = GpuContext::Get().device.createPipelineLayout(pipeline_layout_create_info);
}

void FairyPipeline::CreatePipeline(vk::RenderPass render_pass)
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
    pipeline_create_info.subpass = 0;
    pipeline_ = GpuContext::Get().device.createGraphicsPipeline(nullptr, pipeline_create_info).value;
}

void FairyPipeline::CreateDrawIndices()
{
    const uint16_t rect_indices[] = { 0, 1, 2, 1, 3, 2 };
    indices_type_ = vk::IndexType::eUint16;
    indices_count_ = sizeof(rect_indices) / sizeof(uint16_t);
    indices_buffer_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(
        sizeof(rect_indices), vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    memcpy(indices_buffer_->HostPointer(), rect_indices, sizeof(rect_indices));
}

void FairyPipeline::CreateDrawResource()
{
    i_resolution_buffer_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(
        sizeof(ktm::fvec3), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    i_time_buffer_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer,
                                                              vk::MemoryPropertyFlagBits::eHostVisible));
    i_time_delta_buffer_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(
        sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    i_frame_rate_buffer_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(
        sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    i_frame_buffer_ = std::unique_ptr<GpuBuffer>(
        new GpuBuffer(sizeof(int), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    // todo i_channel_time_4_buffer_
    // todo i_channel_resolution_4_buffer_
    i_mouse_buffer_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(
        sizeof(ktm::fvec4), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    // todo i_channel_4_texture_
    i_date_buffer_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(
        sizeof(ktm::fvec4), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
}

void FairyPipeline::BindResourceToDescriptSets()
{
    constexpr auto desc_buffer_info_lambda = [](vk::Buffer buffer)
    {
        vk::DescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = buffer;
        buffer_info.range = VK_WHOLE_SIZE;
        buffer_info.offset = 0;
        return buffer_info;
    };
    vk::DescriptorBufferInfo i_resolution_buffer_info = desc_buffer_info_lambda(i_resolution_buffer_->Buffer());
    vk::DescriptorBufferInfo i_time_buffer_info = desc_buffer_info_lambda(i_time_buffer_->Buffer());
    vk::DescriptorBufferInfo i_time_delta_buffer_info = desc_buffer_info_lambda(i_time_delta_buffer_->Buffer());
    vk::DescriptorBufferInfo i_frame_rate_buffer_info = desc_buffer_info_lambda(i_frame_rate_buffer_->Buffer());
    vk::DescriptorBufferInfo i_frame_buffer_info = desc_buffer_info_lambda(i_frame_buffer_->Buffer());
    vk::DescriptorBufferInfo i_mouse_buffer_info = desc_buffer_info_lambda(i_mouse_buffer_->Buffer());
    vk::DescriptorBufferInfo i_date_buffer_info = desc_buffer_info_lambda(i_date_buffer_->Buffer());

    vk::WriteDescriptorSet write_uniform_set0 = {};
    write_uniform_set0.dstSet = descriptor_sets_[0];
    write_uniform_set0.descriptorCount = 1;
    write_uniform_set0.descriptorType = vk::DescriptorType::eUniformBuffer;
    vk::WriteDescriptorSet write_i_resolution_descriptor_set = write_uniform_set0;
    write_i_resolution_descriptor_set.dstBinding = 0;
    write_i_resolution_descriptor_set.pBufferInfo = &i_resolution_buffer_info;
    vk::WriteDescriptorSet write_i_time_descriptor_set = write_uniform_set0;
    write_i_time_descriptor_set.dstBinding = 1;
    write_i_time_descriptor_set.pBufferInfo = &i_time_buffer_info;
    vk::WriteDescriptorSet write_i_time_delta_descriptor_set = write_uniform_set0;
    write_i_time_delta_descriptor_set.dstBinding = 2;
    write_i_time_delta_descriptor_set.pBufferInfo = &i_time_delta_buffer_info;
    vk::WriteDescriptorSet write_i_frame_rate_descriptor_set = write_uniform_set0;
    write_i_frame_rate_descriptor_set.dstBinding = 3;
    write_i_frame_rate_descriptor_set.pBufferInfo = &i_frame_rate_buffer_info;
    vk::WriteDescriptorSet write_i_frame_descriptor_set = write_uniform_set0;
    write_i_frame_descriptor_set.dstBinding = 4;
    write_i_frame_descriptor_set.pBufferInfo = &i_frame_buffer_info;
    vk::WriteDescriptorSet write_i_mouse_descriptor_set = write_uniform_set0;
    write_i_mouse_descriptor_set.dstBinding = 5;
    write_i_mouse_descriptor_set.pBufferInfo = &i_mouse_buffer_info;
    vk::WriteDescriptorSet write_i_date_descriptor_set = write_uniform_set0;
    write_i_date_descriptor_set.dstBinding = 6;
    write_i_date_descriptor_set.pBufferInfo = &i_date_buffer_info;

    std::vector<vk::WriteDescriptorSet> write_descriptor_sets = {
        write_i_resolution_descriptor_set, write_i_time_descriptor_set,  write_i_time_delta_descriptor_set,
        write_i_frame_rate_descriptor_set, write_i_frame_descriptor_set, write_i_mouse_descriptor_set,
        write_i_date_descriptor_set
    };
    fv::GpuContext::Get().device.updateDescriptorSets(write_descriptor_sets, nullptr);
}

} // namespace fv