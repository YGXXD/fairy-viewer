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

FairyViewerService::FairyViewerService(int fairy_surface_width, int fairy_surface_height, int fairy_buffer_count)
    : fairy_surface_width_(fairy_surface_width), fairy_surface_height_(fairy_surface_height),
      fairy_buffer_count_(fairy_buffer_count)
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
    svr.set_mount_point("/", FAIRY_ASSETS_PATH);
    svr.Post("/offer", [](const httplib::Request& req, httplib::Response& res)
    {
        std::unique_ptr<FairyStreamThread> stream_thread = std::make_unique<FairyStreamThread>();
        std::optional<rtc::Description> offer = stream_thread->CreateOffer();
        std::string key = std::to_string(reinterpret_cast<ptrdiff_t>(stream_thread.get()));
        g_fairy_viewer_service->stream_threads_.emplace(key, std::move(stream_thread));
        nlohmann::json response = { { "key", key }, { "sdp", std::string(*offer) }, { "type", offer->typeString() } };
        res.set_content(response.dump(), "application/json");
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
            std::string key = body["key"].get<std::string>();
            std::string sdp = body["sdp"].get<std::string>();
            std::string type = body["type"].get<std::string>();
            FairyStreamThread* app = g_fairy_viewer_service->stream_threads_.at(key).get();
            app->SetAnswer(rtc::Description(sdp, type));
            res.set_content("ok", "text/plain");
        }
        catch (const std::exception& e)
        {
            res.status = 400;
            res.set_content("webrtc answer set failed", "text/plain");
            return;
        }
    });
    std::cout << "[HttpServer]:" << &svr << ": Start HttpServer On http://0.0.0.0:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    render_thread_.reset();
    gpu::GpuContext::Quit();
    rtc::Cleanup();
    g_fairy_viewer_service = nullptr;
}

} // namespace service