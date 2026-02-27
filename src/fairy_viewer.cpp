#include <iostream>
#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include "render_core/gpu_context.hpp"
#include "render_core/gpu_texture.hpp"
#include "render_fairy/fairy_surface.hpp"
#include "render_fairy/fairy_pipeline.hpp"

const char* title = "hello fairy!";
const int window_width = 1280;
const int window_height = 720;
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* sdl_image_texture;
SDL_PixelFormat sdl_fairy_image_format = SDL_PIXELFORMAT_RGBA32;
SDL_Texture* sdl_fairy_image_texture;

const int fairy_surface_width = 1280;
const int fairy_surface_height = 720;
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

void InitFairy()
{
    fairy_surface = std::unique_ptr<fv::FairySurface>(
        new fv::FairySurface(fairy_surface_width, fairy_surface_height, fairy_surface_format));
    fairy_pipeline = std::unique_ptr<fv::FairyPipeline>(new fv::FairyPipeline(fairy_surface->RenderPass()));
    fairy_start_time = SDL_GetTicks();
    i_time = 0;
    float i_time_delta = 0;
    float i_frame_rate = 0;
    i_frame = 0;
    i_mouse = ktm::fvec4 { 0, 0, 0, 0 };
    i_date = ktm::fvec4 { 0, 0, 0, 0 };

    SDL_PropertiesID texture_props = SDL_CreateProperties();
    VkImage fairy_surface_image = fairy_surface->RenderTarget()->Image();
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, fairy_surface_width);
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, fairy_surface_height);
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, sdl_fairy_image_format);
    SDL_SetNumberProperty(texture_props, SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER,
                          reinterpret_cast<Sint64>(fairy_surface_image));
    sdl_fairy_image_texture = SDL_CreateTextureWithProperties(renderer, texture_props);
    SDL_DestroyProperties(texture_props);
    SDL_SetTextureBlendMode(sdl_fairy_image_texture, SDL_BLENDMODE_NONE);
}

void RenderFairy()
{
    uint64_t fairy_current_ticks = SDL_GetTicks() - fairy_start_time;
    float fairy_current_time = fairy_current_ticks / 1000.f;
    i_time_delta = fairy_current_time - i_time;
    i_time = fairy_current_time;
    i_frame_rate = 1.f / i_time_delta;

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

int main(int argc, char* argv[])
{
    std::cout << "Hello World!" << std::endl;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    fv::GpuContext::Init();
    window = SDL_CreateWindow(title, window_width, window_height, SDL_WINDOW_RESIZABLE);
    SDL_ShowWindow(window);

    fv::GpuContext& gpu_context = fv::GpuContext::Get();
    SDL_PropertiesID renderer_props = SDL_CreateProperties();
    SDL_SetStringProperty(renderer_props, SDL_PROP_RENDERER_CREATE_NAME_STRING, "vulkan");
    SDL_SetPointerProperty(renderer_props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetNumberProperty(renderer_props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB_LINEAR);
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

    float start_time;

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
        ImGui::ShowDemoWindow(&isAppRun);
        ImGui::Image(sdl_fairy_image_texture, ImVec2(800, 450));
        ImGui::Image(sdl_image_texture, ImVec2(200, 200));
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    DestroyFairy();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    SDL_DestroyTexture(sdl_image_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    fv::GpuContext::Quit();
    SDL_Quit();
    return 0;
}