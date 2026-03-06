#include <iostream>
#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "TextEditor.h"

#include "render_core/gpu_context.hpp"
#include "render_core/gpu_texture.hpp"
#include "render_fairy/fairy_surface.hpp"
#include "render_fairy/fairy_pipeline.hpp"

#include "fairy_shader_test.hpp"

const char* title = "fairy vewer";
const int window_width = 1600;
const int window_height = 900;
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* sdl_image_texture;
SDL_PixelFormat sdl_fairy_image_format = SDL_PIXELFORMAT_RGBA32;
SDL_Texture* sdl_fairy_image_texture;

const int fairy_surface_width = 1600;
const int fairy_surface_height = 900;
const vk::Format fairy_surface_format = vk::Format::eR8G8B8A8Srgb;
std::unique_ptr<fv::FairySurface> fairy_surface;
std::unique_ptr<fv::FairyPipeline> fairy_pipeline;
uint64_t fairy_start_time;
float i_time;
float i_time_delta;
float i_frame_rate;
int i_frame;
ktm::fvec4 i_mouse;
ktm::fvec4 i_date;

std::string default_shader = R"(/*
    shadertoy.com input variables
    uniform vec3      iResolution;           // viewport resolution (in pixels)
    uniform float     iTime;                 // shader playback time (in seconds)
    uniform float     iTimeDelta;            // render time (in seconds)
    uniform float     iFrameRate;            // shader frame rate
    uniform int       iFrame;                // shader playback frame
    uniform float     iChannelTime[4];       // channel playback time (in seconds)
    uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
    uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
    uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube
    uniform vec4      iDate;                 // (year, month, day, time in seconds)
*/

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
})";
TextEditor shader_editor;
bool pipeline_reset_status;

float fps;
float fps_curr_time;
int fps_curr_frame;

void ResetPipeline()
{
    std::string codes = shader_editor.GetText();
    fairy_surface->WaitGpuIfNeeded();
    pipeline_reset_status = fairy_pipeline->Reset(fairy_surface->RenderPass(), codes);
    fairy_start_time = SDL_GetTicks();
    i_time = 0;
    float i_time_delta = 0;
    float i_frame_rate = 0;
    i_frame = 0;
    i_mouse = ktm::fvec4 { 0, 0, 0, 0 };
    i_date = ktm::fvec4 { 0, 0, 0, 0 };

    // fps calc
    fps = 0;
    fps_curr_time = 0;
    fps_curr_frame = 0;
}

void InitFairy()
{
    fairy_surface = std::unique_ptr<fv::FairySurface>(
        new fv::FairySurface(fairy_surface_width, fairy_surface_height, fairy_surface_format));

    SDL_PropertiesID texture_props = SDL_CreateProperties();
    VkImage fairy_surface_image = fairy_surface->RenderTarget()->Image();
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, fairy_surface_width);
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, fairy_surface_height);
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, sdl_fairy_image_format);
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER,
                          reinterpret_cast<Sint64>(fairy_surface_image));
    sdl_fairy_image_texture = SDL_CreateTextureWithProperties(renderer, texture_props);
    SDL_DestroyProperties(texture_props);

    TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::GLSL();
    shader_editor.SetLanguageDefinition(lang);
    shader_editor.SetText(default_shader);
    shader_editor.SetPalette(TextEditor::GetLightPalette());
    shader_editor.SetShowWhitespaces(false);
    shader_editor.SetTabSize(4);
    shader_editor.SetReadOnly(false);

    fairy_pipeline = std::unique_ptr<fv::FairyPipeline>(new fv::FairyPipeline());
    ResetPipeline();
}

void RenderFairy()
{
    uint64_t fairy_current_ticks = SDL_GetTicks() - fairy_start_time;
    float fairy_current_time = fairy_current_ticks / 1000.f;
    i_time_delta = fairy_current_time - i_time;
    i_time = fairy_current_time;
    i_frame_rate = 1.f / i_time_delta;
    fps_curr_time += i_time_delta;
    if (fps_curr_time > 0.333f)
    {
        fps = static_cast<float>(i_frame - fps_curr_frame) / fps_curr_time;
        fps_curr_frame = i_frame;
        fps_curr_time = 0.f;
    }

    fairy_surface->WaitGpuIfNeeded();

    fairy_pipeline->Update_iResolution(ktm::fvec3 { fairy_surface_width, fairy_surface_height, 1 });
    fairy_pipeline->Update_iTime(i_time);
    fairy_pipeline->Update_iTimeDelta(i_time_delta);
    fairy_pipeline->Update_iFrameRate(i_frame_rate);
    fairy_pipeline->Update_iFrame(i_frame++);
    fairy_pipeline->Update_iMouse(i_mouse);
    fairy_pipeline->Update_iDate(i_date);

    vk::Semaphore signal_semaphore;
    fairy_surface->Render(fairy_pipeline.get(), signal_semaphore);
    SDL_AddVulkanRenderSemaphores(renderer, static_cast<Uint32>(vk::PipelineStageFlagBits::eFragmentShader),
                                  reinterpret_cast<Sint64>(VkSemaphore { signal_semaphore }), 0);
}

void DestroyFairy()
{
    SDL_DestroyTexture(sdl_fairy_image_texture);
    fairy_pipeline.reset();
    fairy_surface.reset();
}

void ShowFairyWindow()
{
    ImGui::Begin("fairy");
    ImGui::Image(sdl_fairy_image_texture, ImVec2(800, 450));
    if (!pipeline_reset_status)
    {
        ImVec2 curr_cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(20, 40));
        ImGui::TextWrapped("log error:\n%s", fairy_pipeline->ResetErrorMessage().c_str());
        ImGui::SetCursorPos(curr_cursor_pos);
    }
    ImGui::Text("time: %.2f s", i_time);
    ImGui::SameLine(0, 30);
    ImGui::Text("fps: %.1f", fps);
    ImGui::SameLine(0, 30);
    ImGui::Text("frame: %d", i_frame);

    ImGui::End();
}

void ShowCodeEditorWindow()
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
            shader_editor.SetText(std::string(shaders[i]));
            ResetPipeline();
        }
    }
    shader_editor.Render("text region");
    ImGui::End();
}

int main(int argc, char* argv[])
{
    std::cout << "Hello World!" << std::endl;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    fv::GpuContext::Init();
    window = SDL_CreateWindow(title, window_width, window_height, SDL_WINDOW_RESIZABLE);
    SDL_ShowWindow(window);
    SDL_StartTextInput(window);

    fv::GpuContext& gpu_context = fv::GpuContext::Get();
    SDL_PropertiesID renderer_props = SDL_CreateProperties();
    SDL_SetStringProperty(renderer_props, SDL_PROP_RENDERER_CREATE_NAME_STRING, "vulkan");
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER,
                          SDL_COLORSPACE_SRGB_LINEAR);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_RENDERER_VSYNC_ADAPTIVE);
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_INSTANCE_POINTER, gpu_context.instance);
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_PHYSICAL_DEVICE_POINTER,
                           gpu_context.physical_device);
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_DEVICE_POINTER, gpu_context.device);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER, 0);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER, 0);
    renderer = SDL_CreateRendererWithProperties(renderer_props);
    if (renderer == nullptr)
        std::cerr << SDL_GetError() << std::endl;

    SDL_IOStream* png_io = SDL_IOFromFile(ASSETS_PATH "nfl.png", "rb");
    SDL_Surface* surface = IMG_LoadPNG_IO(png_io);
    SDL_CloseIO(png_io);
    sdl_image_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    SDL_SetRenderDrawColorFloat(renderer, 0.f, 0.f, 0.f, 1.0f);
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

    InitFairy();

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
        SDL_RenderTexture(renderer, sdl_fairy_image_texture, nullptr, nullptr);
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ShowFairyWindow();
        ImGui::Begin("test");
        ImGui::Image(sdl_image_texture, ImVec2(200, 200));
        ImGui::End();
        ShowCodeEditorWindow();
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    DestroyFairy();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(sdl_image_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    fv::GpuContext::Quit();
    SDL_Quit();
    return 0;
}