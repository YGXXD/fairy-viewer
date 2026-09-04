#include "fairy_viewer_service.hpp"
#include "fairy_stream_thread.hpp"
#include "fairy_render_thread.hpp"
#include "../gpu/gpu_context.hpp"
#include "httplib.h"
#include "nlohmann/json.hpp"

namespace service
{

static FairyViewerService* g_fairy_viewer_service = nullptr;

FairyViewerService* FairyViewerService::Global()
{
    return g_fairy_viewer_service;
}

FairyViewerService::FairyViewerService(int fairy_surface_width, int fairy_surface_height, int fairy_buffer_count,
                                       int fairy_stream_fps)
    : fairy_surface_width_(fairy_surface_width), fairy_surface_height_(fairy_surface_height),
      fairy_buffer_count_(fairy_buffer_count), fairy_stream_fps_(fairy_stream_fps)
{
}

FairyViewerService::~FairyViewerService() = default;

void FairyViewerService::Run()
{
    g_fairy_viewer_service = this;
    rtc::InitLogger(rtc::LogLevel::Info);
    rtc::Preload();
    gpu::GpuContext::Init();
    render_thread_ =
        std::make_unique<FairyRenderThread>(fairy_surface_width_, fairy_surface_height_, fairy_buffer_count_);
    httplib::Server svr;
    std::thread http_server_thread([&svr]()
    {
        svr.set_mount_point("/", FAIRY_ASSETS_PATH);
        svr.Post("/offer", [](const httplib::Request& req, httplib::Response& res)
        {
            std::unique_ptr<FairyStreamThread> stream_thread =
                std::make_unique<FairyStreamThread>(g_fairy_viewer_service->fairy_stream_fps_);
            std::optional<rtc::Description> offer = stream_thread->CreateOffer();
            nlohmann::json response = { { "key", reinterpret_cast<ptrdiff_t>(stream_thread.get()) },
                                        { "sdp", std::string(*offer) },
                                        { "type", offer->typeString() } };
            res.set_content(response.dump(), "application/json");
            g_fairy_viewer_service->AddStreamThread(std::move(stream_thread));
        });
        svr.Post("/answer", [](const httplib::Request& req, httplib::Response& res)
        {
            nlohmann::json body;
            try
            {
                body = nlohmann::json::parse(req.body);
            }
            catch (const std::exception& e)
            {
                res.status = 400;
                res.set_content("json parse failed", "text/plain");
                return;
            }
            try
            {
                ptrdiff_t key = body["key"].get<ptrdiff_t>();
                std::string sdp = body["sdp"].get<std::string>();
                std::string type = body["type"].get<std::string>();
                FairyStreamThread* stream_thread = reinterpret_cast<FairyStreamThread*>(key);
                if (g_fairy_viewer_service->ContainsStreamThread(stream_thread))
                {
                    stream_thread->SetAnswer(rtc::Description(sdp, type));
                    res.set_content("webrtc answer set successfully", "text/plain");
                    return;
                }
                res.status = 400;
                res.set_content("webrtc invalid key", "text/plain");
            }
            catch (const std::exception& e)
            {
                res.status = 400;
                res.set_content("webrtc answer set failed", "text/plain");
            }
        });
        svr.Post("/pipeline_status", [](const httplib::Request& req, httplib::Response& res)
        {
            nlohmann::json response = { { "codes", g_fairy_viewer_service->RenderThread()->RequestCurrentResetCodes() } };
            res.set_content(response.dump(), "application/json");
        });
        svr.Post("/pipeline_reset", [](const httplib::Request& req, httplib::Response& res)
        {
            nlohmann::json body;
            try
            {
                body = nlohmann::json::parse(req.body);
            }
            catch (const std::exception& e)
            {
                res.status = 400;
                res.set_content("json parse failed", "text/plain");
                return;
            }
            try
            {
                std::string codes = body["codes"].get<std::string>();
                std::optional<std::string> reset_message =
                    g_fairy_viewer_service->RenderThread()->RequestResetPipeline(codes);
                if (reset_message.has_value())
                {
                    nlohmann::json response = { { "result", 0 }, { "message", reset_message.value() } };
                    res.set_content(response.dump(), "application/json");
                    return;
                }
                nlohmann::json response = { { "result", 1 }, { "message", "pipeline reset success" } };
                res.set_content(response.dump(), "application/json");
            }
            catch (const std::exception& e)
            {
                res.status = 400;
                res.set_content("pipeline reset failed", "text/plain");
            }
        });
        std::cout << "[HttpServer]:" << &svr << ": Start Http Server On http://0.0.0.0:8080" << std::endl;
        svr.listen("0.0.0.0", 8080);
        std::cout << "[HttpServer]:" << &svr << ": Stop Http Server" << std::endl;
    });
    http_server_thread.join();
    stream_threads_.clear();
    render_thread_.reset();
    gpu::GpuContext::Quit();
    rtc::Cleanup();
    g_fairy_viewer_service = nullptr;
}

void FairyViewerService::AddStreamThread(std::unique_ptr<FairyStreamThread> stream_thread)
{
    std::lock_guard lock(stream_threads_mutex_);
    stream_threads_.emplace(stream_thread.get(), std::move(stream_thread));
}

void FairyViewerService::RemoveStreamThread(FairyStreamThread* stream_thread)
{
    std::lock_guard lock(stream_threads_mutex_);
    stream_threads_.erase(stream_thread);
}

bool FairyViewerService::ContainsStreamThread(FairyStreamThread* stream_thread)
{
    std::lock_guard lock(stream_threads_mutex_);
    return stream_threads_.find(stream_thread) != stream_threads_.end();
}

} // namespace service
