#pragma once

#include <memory>
#include <atomic>
#include <chrono>
#include "rtc/rtc.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace service
{

class FairyRenderThread;
class FairyStreamThread final
{
public:
    FairyStreamThread();
    ~FairyStreamThread();

    std::optional<rtc::Description> CreateOffer();
    void SetAnswer(rtc::Description answer);

private:
    void Start();
    void Stop();
    void FairyStreamThreadMain();
    bool InitCodec();
    void DestoryCodec();
    void SendFrame(uint8_t* rgba_data);
    void FlushFrame();

    int fps_;
    int frame_sync_time_;
    FairyRenderThread* render_thread_;

    std::unique_ptr<rtc::PeerConnection> peer_connection_;
    std::shared_ptr<rtc::Track> track_;

    std::atomic<bool> is_run_;
    std::thread run_thread_;

    const AVCodec* codec_;
    AVCodecContext* codec_context_;
    AVFrame* frame_;
    AVPacket* packet_;
    std::chrono::steady_clock::time_point encode_start_ticks_;
    SwsContext* sws_context_;
};

}; // namespace service
