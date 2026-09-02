#include "fairy_viewer_app.hpp"
#include "SDL3_image/SDL_image.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "../gpu/gpu_context.hpp"
#include "../gpu/gpu_texture.hpp"
#include "../fairy/fairy_surface.hpp"
#include "../fairy/fairy_pipeline.hpp"
#include "fairy_viewer_app_shaders.hpp"

#include <iostream>

namespace app
{

FairyViewerApp::FairyViewerApp(const char* title, int window_width, int window_height, int fairy_surface_width,
                               int fairy_surface_height)
    : title_(title), window_width_(window_width), window_height_(window_height),
      fairy_surface_width_(fairy_surface_width), fairy_surface_height_(fairy_surface_height)
{
}

FairyViewerApp::~FairyViewerApp() = default;

void FairyViewerApp::Run()
{
    gpu::GpuContext::Init();
    InitSDLContext();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer3_Init(renderer_);
    SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    InitFairy();
    bool is_app_run = true;
    SDL_Event event;
    while (is_app_run)
    {
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                is_app_run = false;
            }
        }

        current_render_index_ = (current_render_index_ + 1) % fairy_buffer_count_;
        fairy_surface_->AddSignalSemaphore(fairy_complete_signals_[current_render_index_]);
        RenderFairy(current_render_index_);
        SDL_AddVulkanRenderSemaphores(
            renderer_, static_cast<Uint32>(vk::PipelineStageFlagBits::eFragmentShader),
            reinterpret_cast<Sint64>((VkSemaphore)fairy_complete_signals_[current_render_index_]), 0);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, sdl_fairy_image_textures_[current_render_index_], nullptr, nullptr);
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ShowFairyWindow(current_render_index_);
        ImGui::Begin("test");
        ImGui::Image(sdl_image_texture_, ImVec2(200, 200));
        ImGui::End();
        ShowCodeEditorWindow();
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);
    }
    DestroyFairy();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    DestroySDLContext();
    gpu::GpuContext::Quit();
}

void FairyViewerApp::InitSDLContext()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    gpu::GpuContext::Init();
    window_ = SDL_CreateWindow(title_, window_width_, window_height_, SDL_WINDOW_RESIZABLE);
    SDL_ShowWindow(window_);
    SDL_StartTextInput(window_);

    gpu::GpuContext& gpu_context = gpu::GpuContext::Get();
    SDL_PropertiesID renderer_props = SDL_CreateProperties();
    SDL_SetStringProperty(renderer_props, SDL_PROP_RENDERER_CREATE_NAME_STRING, "vulkan");
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window_);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER,
                          SDL_COLORSPACE_SRGB_LINEAR);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_RENDERER_VSYNC_ADAPTIVE);
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_INSTANCE_POINTER, gpu_context.instance);
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_PHYSICAL_DEVICE_POINTER,
                           gpu_context.physical_device);
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_DEVICE_POINTER, gpu_context.device);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER,
                          gpu_context.queue_family_index);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER,
                          gpu_context.queue_family_index);
    renderer_ = SDL_CreateRendererWithProperties(renderer_props);
    SDL_DestroyProperties(renderer_props);
    if (renderer_ == nullptr)
        std::cerr << SDL_GetError() << std::endl;
    SDL_SetRenderDrawColorFloat(renderer_, 0.f, 0.f, 0.f, 1.0f);

    SDL_IOStream* png_io = SDL_IOFromFile(FAIRY_ASSETS_PATH "/nfl.png", "rb");
    SDL_Surface* surface = IMG_LoadPNG_IO(png_io);
    SDL_CloseIO(png_io);
    sdl_image_texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
}

void FairyViewerApp::DestroySDLContext()
{
    SDL_DestroyTexture(sdl_image_texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

void FairyViewerApp::InitFairy()
{
    fairy_buffer_count_ = SDL_GetNumberProperty(SDL_GetRendererProperties(renderer_),
                                                SDL_PROP_RENDERER_VULKAN_SWAPCHAIN_IMAGE_COUNT_NUMBER, 2);
    fairy_surface_format_ = vk::Format::eR8G8B8A8Srgb;
    fairy_surface_ = std::unique_ptr<fairy::FairySurface>(
        new fairy::FairySurface(fairy_surface_width_, fairy_surface_height_, fairy_surface_format_,
                                fairy::FairySurfaceUsage::eSample, fairy_buffer_count_));

    current_render_index_ = 0;
    fairy_complete_signals_.reserve(fairy_buffer_count_);
    sdl_fairy_image_textures_.reserve(fairy_buffer_count_);
    sdl_fairy_image_format_ = SDL_PIXELFORMAT_RGBA32;
    for (int i = 0; i < fairy_buffer_count_; ++i)
    {
        fairy_complete_signals_.emplace_back(gpu::GpuContext::Get().device.createSemaphore({}));
        SDL_PropertiesID texture_props = SDL_CreateProperties();
        VkImage fairy_surface_image = fairy_surface_->RenderTarget(i)->Image();
        SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, fairy_surface_width_);
        SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, fairy_surface_height_);
        SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, sdl_fairy_image_format_);
        SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER,
                              reinterpret_cast<Sint64>(fairy_surface_image));
        sdl_fairy_image_textures_.emplace_back(SDL_CreateTextureWithProperties(renderer_, texture_props));
        SDL_DestroyProperties(texture_props);
    }

    TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::GLSL();
    shader_editor_.SetLanguageDefinition(lang);
    shader_editor_.SetText(std::string(default_shader));
    shader_editor_.SetPalette(TextEditor::GetLightPalette());
    shader_editor_.SetShowWhitespaces(false);
    shader_editor_.SetTabSize(4);
    shader_editor_.SetReadOnly(false);

    fairy_pipeline_ = std::unique_ptr<fairy::FairyPipeline>(new fairy::FairyPipeline());
    ResetPipeline();
}

void FairyViewerApp::RenderFairy(int index)
{
    uint64_t fairy_current_ticks = SDL_GetTicks() - fairy_start_time_;
    float fairy_current_time = fairy_current_ticks / 1000.f;
    i_time_delta_ = fairy_current_time - i_time_;
    i_time_ = fairy_current_time;
    i_frame_rate_ = 1.f / i_time_delta_;
    fps_curr_time_ += i_time_delta_;
    if (fps_curr_time_ > 0.333f)
    {
        fps_ = static_cast<float>(i_frame_ - fps_curr_frame_) / fps_curr_time_;
        fps_curr_frame_ = i_frame_;
        fps_curr_time_ = 0.f;
    }
    fairy_pipeline_->Update_iResolution(
        ktm::fvec3 { static_cast<float>(fairy_surface_width_), static_cast<float>(fairy_surface_height_), 1.f });
    fairy_pipeline_->Update_iTime(i_time_);
    fairy_pipeline_->Update_iTimeDelta(i_time_delta_);
    fairy_pipeline_->Update_iFrameRate(i_frame_rate_);
    fairy_pipeline_->Update_iFrame(i_frame_++);
    fairy_pipeline_->Update_iMouse(i_mouse_);
    fairy_pipeline_->Update_iDate(i_date_);
    fairy_surface_->Render(fairy_pipeline_.get(), index);
}

void FairyViewerApp::DestroyFairy()
{
    for (const auto& texture : sdl_fairy_image_textures_)
        SDL_DestroyTexture(texture);
    fairy_surface_.reset();
    for (const auto& semaphore : fairy_complete_signals_)
        gpu::GpuContext::Get().device.destroySemaphore(semaphore);
    fairy_pipeline_.reset();
}

void FairyViewerApp::ResetPipeline()
{
    std::string codes = shader_editor_.GetText();
    fairy_surface_->WaitRenderComplete();
    pipeline_reset_status_ = fairy_pipeline_->Reset(fairy_surface_->RenderPass(), codes);
    fairy_start_time_ = SDL_GetTicks();
    i_time_ = 0;
    i_time_delta_ = 0;
    i_frame_rate_ = 0;
    i_frame_ = 0;
    i_mouse_ = ktm::fvec4 { 0, 0, 0, 0 };
    i_date_ = ktm::fvec4 { 0, 0, 0, 0 };

    // fps calc
    fps_ = 0;
    fps_curr_time_ = 0;
    fps_curr_frame_ = 0;
}

void FairyViewerApp::ShowFairyWindow(int index)
{
    ImGui::Begin("fairy");
    ImGui::Image(sdl_fairy_image_textures_[index], ImVec2(800, 450));
    if (!pipeline_reset_status_)
    {
        ImVec2 curr_cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(20, 40));
        ImGui::TextWrapped("log error:\n%s", fairy_pipeline_->ResetErrorMessage().c_str());
        ImGui::SetCursorPos(curr_cursor_pos);
    }
    ImGui::Text("time: %.2f s", i_time_);
    ImGui::SameLine(0, 30);
    ImGui::Text("fps: %.1f", fps_);
    ImGui::SameLine(0, 30);
    ImGui::Text("frame: %d", i_frame_);

    ImGui::End();
}

void FairyViewerApp::ShowCodeEditorWindow()
{
    ImGui::Begin("shader editor");
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
    if (ImGui::Button("compile", ImVec2(100, 24)))
    {
        ResetPipeline();
    }
    for (int i = 0; i < shaders.size(); i++)
    {
        ImGui::SameLine(0, 10);
        if (ImGui::Button((std::string("test") + std::to_string(i)).c_str(), ImVec2(100, 24)))
        {
            shader_editor_.SetText(std::string(shaders[i]));
            ResetPipeline();
        }
    }
    shader_editor_.Render("text region");
    ImGui::End();
}

} // namespace app
