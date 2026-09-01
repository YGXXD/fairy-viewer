#pragma once

#include <unordered_map>
#include <memory>
#include <string>

namespace service
{

class FairyStreamThread;
class FairyRenderThread;
class FairyViewerService final
{
public:
    static FairyViewerService* Global();

    FairyViewerService(int fairy_surface_width, int fairy_surface_height, int fairy_buffer_count);
    ~FairyViewerService();

    void Run();
    inline FairyRenderThread* RenderThread() const { return render_thread_.get(); }

private:
    int fairy_surface_width_;
    int fairy_surface_height_;
    int fairy_buffer_count_;

    std::unordered_map<std::string, std::unique_ptr<FairyStreamThread>> stream_threads_;
    std::unique_ptr<FairyRenderThread> render_thread_;
};

}; // namespace service
