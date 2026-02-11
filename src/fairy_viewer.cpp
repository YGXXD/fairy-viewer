#include <iostream>
#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include "fairy_context.hpp"
#include "fairy_texture.hpp"
#include "fairy_buffer.hpp"
#include "fairy_pipeline.hpp"

const char* title = "hello fairy!";
const int window_width = 1280;
const int window_height = 720;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* sdl_image_texture;
SDL_PixelFormat sdl_fairy_image_format = SDL_PIXELFORMAT_RGBA8888;
SDL_Texture* sdl_fairy_image_texture;

const int fairy_width = 1280;
const int fairy_height = 720;
const std::array<uint16_t, 6> rect_indices = { 0, 1, 2, 1, 3, 2 };
std::unique_ptr<fv::FairyBuffer> index_buffer;
vk::Format render_target_format = vk::Format::eR8G8B8A8Unorm;
std::unique_ptr<fv::FairyTexture> render_target_texture;
std::unique_ptr<fv::FairyBuffer> cpu_dst_texture;
std::unique_ptr<fv::FairyBuffer> i_resolution_buffer;
vk::UniqueRenderPass fairy_render_pass;
vk::UniqueFramebuffer fairy_frame_buffer;
std::unique_ptr<fv::FairyPipeline> fairy_pipeline;
std::vector<vk::DescriptorSet> fairy_descriptor_sets;
vk::UniqueFence fairy_fence;

void InitRenderResource()
{
    index_buffer = std::unique_ptr<fv::FairyBuffer>(new fv::FairyBuffer(rect_indices.size() * sizeof(uint16_t),
                                                                        vk::BufferUsageFlagBits::eIndexBuffer,
                                                                        vk::MemoryPropertyFlagBits::eHostVisible));
    memcpy(index_buffer->HostPointer(), rect_indices.data(), rect_indices.size() * sizeof(uint16_t));

    render_target_texture = std::unique_ptr<fv::FairyTexture>(
        new fv::FairyTexture(fairy_width, fairy_height, render_target_format,
                             vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eDeviceLocal));
    cpu_dst_texture = std::unique_ptr<fv::FairyBuffer>(new fv::FairyBuffer(fairy_width * fairy_height * 4,
                                                                           vk::BufferUsageFlagBits::eTransferDst,
                                                                           vk::MemoryPropertyFlagBits::eHostVisible));
    i_resolution_buffer = std::unique_ptr<fv::FairyBuffer>(new fv::FairyBuffer(
        sizeof(float) * 3, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    float* i_resolution_ptr = static_cast<float*>(i_resolution_buffer->HostPointer());
    i_resolution_ptr[0] = static_cast<float>(fairy_width);
    i_resolution_ptr[1] = static_cast<float>(fairy_height);
    i_resolution_ptr[2] = 1.0f;

    vk::AttachmentDescription color_attachment = {};
    color_attachment.format = render_target_format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
    vk::AttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &color_attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    fairy_render_pass = fv::FairyContext::Get().device.createRenderPassUnique(renderPassInfo);

    vk::ImageView render_target_view = render_target_texture->ImageView();
    vk::FramebufferCreateInfo frame_buffer_create_info = {};
    frame_buffer_create_info.renderPass = fairy_render_pass.get();
    frame_buffer_create_info.attachmentCount = 1;
    frame_buffer_create_info.pAttachments = &render_target_view;
    frame_buffer_create_info.width = fairy_width;
    frame_buffer_create_info.height = fairy_height;
    frame_buffer_create_info.layers = 1;
    fairy_frame_buffer = fv::FairyContext::Get().device.createFramebufferUnique(frame_buffer_create_info);

    fairy_pipeline = std::make_unique<fv::FairyPipeline>(fairy_render_pass.get(), 0);

    vk::DescriptorSetAllocateInfo ds_allocate_info = {};
    ds_allocate_info.pSetLayouts = fairy_pipeline->DescriptorSetLayouts().data();
    ds_allocate_info.descriptorPool = fairy_pipeline->DescriptorPool();
    ds_allocate_info.descriptorSetCount = fairy_pipeline->DescriptorSetLayouts().size();
    fairy_descriptor_sets = fv::FairyContext::Get().device.allocateDescriptorSets(ds_allocate_info);

    vk::DescriptorBufferInfo i_resolution_buffer_info = {};
    i_resolution_buffer_info.buffer = i_resolution_buffer->Buffer();
    i_resolution_buffer_info.range = VK_WHOLE_SIZE;
    i_resolution_buffer_info.offset = 0;
    vk::WriteDescriptorSet write_i_resolution_descriptor_set = {};
    write_i_resolution_descriptor_set.dstSet = fairy_descriptor_sets[0];
    write_i_resolution_descriptor_set.dstBinding = 0;
    write_i_resolution_descriptor_set.descriptorCount = 1;
    write_i_resolution_descriptor_set.descriptorType = vk::DescriptorType::eUniformBuffer;
    write_i_resolution_descriptor_set.pBufferInfo = &i_resolution_buffer_info;
    fv::FairyContext::Get().device.updateDescriptorSets({ write_i_resolution_descriptor_set }, nullptr);

    fairy_fence = fv::FairyContext::Get().device.createFenceUnique({});
}

void RenderFairy()
{
    vk::CommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.commandPool = fv::FairyContext::Get().command_pool;
    command_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_allocate_info.commandBufferCount = 1;

    vk::UniqueCommandBuffer command_buffer_up =
        std::move(fv::FairyContext::Get().device.allocateCommandBuffersUnique(command_buffer_allocate_info)[0]);
    vk::CommandBuffer command_buffer = command_buffer_up.get();
    command_buffer.reset();

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    command_buffer.begin(begin_info);
    vk::ClearValue clear_value = {};
    vk::RenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.framebuffer = fairy_frame_buffer.get();
    render_pass_begin_info.renderPass = fairy_render_pass.get();
    render_pass_begin_info.renderArea.offset = vk::Offset2D(0, 0);
    render_pass_begin_info.renderArea.extent = vk::Extent2D(fairy_width, fairy_height);
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fairy_pipeline->Pipeline());
    vk::Viewport viewport = { 0.f, 0.f, static_cast<float>(fairy_width), static_cast<float>(fairy_height), 0.f, 1.f };
    vk::Rect2D scissor = { vk::Offset2D(0.f, 0.f), vk::Extent2D(fairy_width, fairy_height) };
    command_buffer.setViewport(0, viewport);
    command_buffer.setScissor(0, scissor);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, fairy_pipeline->PipelineLayout(), 0,
                                      fairy_descriptor_sets, {});
    command_buffer.bindIndexBuffer(index_buffer->Buffer(), vk::DeviceSize(0), vk::IndexType::eUint16);
    command_buffer.drawIndexed(rect_indices.size(), 1, 0, 0, 0);
    command_buffer.endRenderPass();
    vk::ImageMemoryBarrier render_target_image_barrier = {};
    render_target_image_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    render_target_image_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    render_target_image_barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
    render_target_image_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    render_target_image_barrier.image = render_target_texture->Image();
    render_target_image_barrier.subresourceRange = *render_target_texture->MakeSubresourceRange();
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eTransfer, static_cast<vk::DependencyFlags>(0), {}, {},
                                   render_target_image_barrier);
    vk::BufferImageCopy copy_region = {};
    copy_region.imageSubresource = *render_target_texture->MakeSubresourceLayers();
    copy_region.imageOffset = vk::Offset3D(0, 0, 0);
    copy_region.imageExtent = vk::Extent3D(fairy_width, fairy_height, 1);
    command_buffer.copyImageToBuffer(render_target_texture->Image(), vk::ImageLayout::eTransferSrcOptimal,
                                     cpu_dst_texture->Buffer(), 1, &copy_region);
    command_buffer.end();

    vk::SubmitInfo submit_info = {};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    fv::FairyContext::Get().queue.submit(submit_info, fairy_fence.get());
    auto _ =
        fv::FairyContext::Get().device.waitForFences(fairy_fence.get(), true, std::numeric_limits<uint64_t>::max());
    fv::FairyContext::Get().device.resetFences(fairy_fence.get());

    void* dst_pixels;
    int dst_pitch;
    if (SDL_LockTexture(sdl_fairy_image_texture, NULL, &dst_pixels, &dst_pitch))
    {
        char* src_pixels = static_cast<char*>(cpu_dst_texture->HostPointer());
        uint32_t* dst_pixels_u32 = static_cast<uint32_t*>(dst_pixels);
        const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(sdl_fairy_image_format);
        for (int y = 0; y < fairy_height; ++y)
        {
            for (int x = 0; x < fairy_width; ++x)
            {
                dst_pixels_u32[y * (dst_pitch / 4) + x] =
                    SDL_MapRGBA(details, NULL, src_pixels[0], src_pixels[1], src_pixels[2], src_pixels[3]);
                src_pixels += 4;
            }
        }
        SDL_UnlockTexture(sdl_fairy_image_texture);
    }
}

void DestroyRenderResource()
{
    index_buffer.reset();
    render_target_texture.reset();
    cpu_dst_texture.reset();
    i_resolution_buffer.reset();
    fv::FairyContext::Get().device.freeDescriptorSets(fairy_pipeline->DescriptorPool(), fairy_descriptor_sets);
    fairy_descriptor_sets.clear();
    fairy_pipeline.reset();
    fairy_frame_buffer.reset();
    fairy_render_pass.reset();
    fairy_fence.reset();
}

int main(int argc, char* argv[])
{
    std::cout << "Hello World!" << std::endl;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    fv::FairyContext::Init();
    window = SDL_CreateWindow(title, window_width, window_height, SDL_WINDOW_RESIZABLE);
    SDL_ShowWindow(window);

    renderer = SDL_CreateRenderer(window, nullptr);
    SDL_IOStream* png_io = SDL_IOFromFile(ASSETS_PATH "nfl.png", "rb");
    SDL_Surface* surface = IMG_LoadPNG_IO(png_io);
    SDL_CloseIO(png_io);
    sdl_image_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    sdl_fairy_image_texture =
        SDL_CreateTexture(renderer, sdl_fairy_image_format, SDL_TEXTUREACCESS_STREAMING, fairy_width, fairy_height);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    SDL_SetRenderDrawColorFloat(renderer, 0.5f, 0.5f, 0.2f, 1.0f);
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

    InitRenderResource();

    bool isAppRun = true;
    SDL_Event event;
    while (isAppRun)
    {
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                isAppRun = false;
            }
        }

        RenderFairy();

        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow(&isAppRun);
        ImGui::Image(sdl_image_texture, ImVec2(300, 300));
        ImGui::Image(sdl_fairy_image_texture, ImVec2(640, 360));
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    DestroyRenderResource();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    SDL_DestroyTexture(sdl_image_texture);
    SDL_DestroyTexture(sdl_fairy_image_texture);
    SDL_DestroyWindow(window);
    fv::FairyContext::Quit();
    SDL_Quit();
    return 0;
}