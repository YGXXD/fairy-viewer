#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>

namespace service
{

class FairyStreamThread;
class FairyRenderThread;
class FairyViewerService final
{
public:
    static FairyViewerService* Global();

    FairyViewerService(int fairy_surface_width, int fairy_surface_height, int fairy_buffer_count, int fairy_stream_fps);
    ~FairyViewerService();

    void Run();
    inline FairyRenderThread* RenderThread() const { return render_thread_.get(); }
    void AddStreamThread(std::unique_ptr<FairyStreamThread> stream_thread);
    void RemoveStreamThread(FairyStreamThread* stream_thread);
    bool ContainsStreamThread(FairyStreamThread* stream_thread);

private:
    int fairy_surface_width_;
    int fairy_surface_height_;
    int fairy_buffer_count_;
    int fairy_stream_fps_;

    std::unique_ptr<FairyRenderThread> render_thread_;
    std::mutex stream_threads_mutex_;
    std::unordered_map<FairyStreamThread*, std::unique_ptr<FairyStreamThread>> stream_threads_;
};

}; // namespace service
