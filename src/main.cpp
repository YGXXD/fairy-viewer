#include <iostream>
#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include "vk_context.h"

const char* title = "hello fairy!";
const int width = 1280;
const int height = 720;
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* targetTexture;

int main(int argc, char* argv[]) {
    std::cout << "Hello World!" << std::endl;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    fv::VkContext::Init();
    window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    SDL_ShowWindow(window);

    renderer = SDL_CreateRenderer(window, nullptr);
    SDL_IOStream* png_io = SDL_IOFromFile(ASSETS_PATH "nfl.png", "rb");
    SDL_Surface* surface = IMG_LoadPNG_IO(png_io);
    SDL_CloseIO(png_io);
    targetTexture = SDL_CreateTextureFromSurface(renderer, surface);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    SDL_SetRenderDrawColorFloat(renderer, 0.5f, 0.5f, 0.2f, 1.0f);
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
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

        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow(&isAppRun);
        ImGui::Image(targetTexture, ImVec2(width / 3.f, height / 3.f));
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    SDL_DestroyTexture(targetTexture);
    SDL_DestroyWindow(window);
    fv::VkContext::Quit();
    SDL_Quit();
    return 0;
}