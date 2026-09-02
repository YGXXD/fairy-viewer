#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include "SDL3/SDL.h"
#include "ktm/ktm.h"
#include "TextEditor.h"

namespace fairy
{
class FairySurface;
class FairyPipeline;
} // namespace fairy

namespace app
{

class FairyViewerApp final
{
public:
    FairyViewerApp(const char* title, int window_width, int window_height, int fairy_surface_width,
                   int fairy_surface_height);
    ~FairyViewerApp();

    void Run();

private:
    void InitSDLContext();
    void DestroySDLContext();
    void InitFairy();
    void DestroyFairy();
    void RenderFairy(int index);
    void ResetPipeline();
    void ShowFairyWindow(int index);
    void ShowCodeEditorWindow();

    const char* title_;
    int window_width_;
    int window_height_;
    int fairy_surface_width_;
    int fairy_surface_height_;

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* sdl_image_texture_;

    int fairy_buffer_count_;
    vk::Format fairy_surface_format_;
    std::unique_ptr<fairy::FairySurface> fairy_surface_;
    std::unique_ptr<fairy::FairyPipeline> fairy_pipeline_;
    bool pipeline_reset_status_;
    std::vector<vk::Semaphore> fairy_complete_signals_;

    int current_render_index_;
    SDL_PixelFormat sdl_fairy_image_format_;
    std::vector<SDL_Texture*> sdl_fairy_image_textures_;
    TextEditor shader_editor_;

    uint64_t fairy_start_time_;
    float i_time_;
    float i_time_delta_;
    float i_frame_rate_;
    int i_frame_;
    ktm::fvec4 i_mouse_;
    ktm::fvec4 i_date_;

    float fps_;
    float fps_curr_time_;
    int fps_curr_frame_;
};

} // namespace app