
#include "fairy_stream_thread.hpp"
#include "fairy_render_thread.hpp"
#include "fairy_viewer_service.hpp"

#include <iostream>
#include <future>

namespace service
{

FairyStreamThread::FairyStreamThread()
    : fps_(60), frame_sync_time_(1000 / fps_), render_thread_(FairyViewerService::Global()->RenderThread()),
      peer_connection_(std::make_unique<rtc::PeerConnection>(rtc::Configuration { .disableAutoNegotiation = true })),
      track_(), is_run_(false), run_thread_()
{
    constexpr uint8_t payload_type = 102;
    const uint32_t ssrc = reinterpret_cast<ptrdiff_t>(this);
    const std::string ssrc_str = std::to_string(ssrc);
    const std::string cname = "cname-" + ssrc_str;
    rtc::Description::Video video("video", rtc::Description::Direction::SendOnly);
    video.addH264Codec(payload_type);
    video.addSSRC(ssrc, cname, "msid-" + ssrc_str, "trackId-" + ssrc_str);
    track_ = peer_connection_->addTrack(std::move(video));
    std::shared_ptr<rtc::RtpPacketizationConfig> packetizer_config =
        std::make_shared<rtc::RtpPacketizationConfig>(ssrc, cname, payload_type, rtc::H264RtpPacketizer::ClockRate);
    std::shared_ptr<rtc::H264RtpPacketizer> packetizer =
        std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, packetizer_config);
    std::shared_ptr<rtc::RtcpSrReporter> sr_reporter = std::make_shared<rtc::RtcpSrReporter>(packetizer_config);
    std::shared_ptr<rtc::RtcpNackResponder> nack_responder = std::make_shared<rtc::RtcpNackResponder>();
    packetizer->addToChain(std::move(sr_reporter));
    packetizer->addToChain(std::move(nack_responder));
    track_->setMediaHandler(packetizer);
    peer_connection_->onStateChange([this](rtc::PeerConnection::State state)
    {
        std::cout << "[FairyStreamThread]:" << this << ": PeerConnection State: " << state << std::endl;
        switch (state)
        {
        case rtc::PeerConnection::State::Connected:
            Start();
            break;
        case rtc::PeerConnection::State::Disconnected:
        case rtc::PeerConnection::State::Failed:
        case rtc::PeerConnection::State::Closed:
            Stop();
            break;
        default:
            break;
        }
    });
    track_->onOpen([this]()
    {
        std::cout << "[FairyStreamThread]:" << this << ": Vedio Track Is Open" << std::endl;
    });
}

FairyStreamThread::~FairyStreamThread() = default;

std::optional<rtc::Description> FairyStreamThread::CreateOffer()
{
    if (peer_connection_)
    {
        std::promise<void> gathering_done;
        peer_connection_->onGatheringStateChange([this, &gathering_done](rtc::PeerConnection::GatheringState state)
        {
            std::cout << "[FairyStreamThread]:" << this << ": ICE Gathering State: " << state << std::endl;
            if (state == rtc::PeerConnection::GatheringState::Complete)
            {
                gathering_done.set_value();
            }
        });
        peer_connection_->setLocalDescription();
        gathering_done.get_future().wait();
        return peer_connection_->localDescription();
    }
    return std::nullopt;
}

void FairyStreamThread::SetAnswer(rtc::Description answer)
{
    peer_connection_->setRemoteDescription(std::move(answer));
}

void FairyStreamThread::Start()
{
    if (is_run_.load())
        return;
    is_run_.store(true);
    run_thread_ = std::thread(&FairyStreamThread::FairyStreamThreadMain, this);
    std::cout << "[FairyStreamThread]:" << this << ": Start Run" << std::endl;
}

void FairyStreamThread::Stop()
{
    if (!is_run_.load())
        return;
    is_run_.store(false);
    if (run_thread_.joinable())
    {
        run_thread_.join();
    }
    std::cout << "[FairyStreamThread]:" << this << ": Stop Run" << std::endl;
}

void FairyStreamThread::FairyStreamThreadMain()
{
    InitCodec();
    while (is_run_.load())
    {
        std::chrono::time_point frame_start = std::chrono::steady_clock::now();
        std::unique_ptr<uint8_t[]> rgba_data = render_thread_->RequestCopySurface();
        SendFrame(rgba_data.get());
        std::chrono::time_point frame_end = std::chrono::steady_clock::now();
        auto frame_delta = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        if (frame_delta < frame_sync_time_)
            std::this_thread::sleep_for(std::chrono::milliseconds(frame_sync_time_ - frame_delta));
    }
    SendFrame(nullptr);
    DestoryCodec();
}

bool FairyStreamThread::InitCodec()
{
    codec_ = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec_)
        codec_ = avcodec_find_encoder_by_name("libx264");
    if (!codec_)
        return false;
    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_)
        return false;
    codec_context_->bit_rate = 4000000;
    codec_context_->width = render_thread_->SurfaceWidth();
    codec_context_->height = render_thread_->SurfaceHeight();
    codec_context_->time_base = (AVRational) { 1, fps_ };
    codec_context_->framerate = (AVRational) { fps_, 1 };
    codec_context_->max_b_frames = 0;
    codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context_->gop_size = fps_;
    if (strcmp(codec_->name, "h264_nvenc") == 0)
    {
        av_opt_set(codec_context_->priv_data, "preset", "p4", 0);
        av_opt_set(codec_context_->priv_data, "tune", "ll", 0);
    }
    else
    {
        av_opt_set(codec_context_->priv_data, "preset", "ultrafast", 0);
        av_opt_set(codec_context_->priv_data, "tune", "zerolatency", 0);
    }
    av_opt_set(codec_context_->priv_data, "profile", "baseline", 0);
    if (avcodec_open2(codec_context_, codec_, nullptr) < 0)
        return false;
    packet_ = av_packet_alloc();
    if (!packet_)
        return false;
    frame_ = av_frame_alloc();
    if (!frame_)
        return false;
    frame_->format = codec_context_->pix_fmt;
    frame_->width = codec_context_->width;
    frame_->height = codec_context_->height;
    if (av_frame_get_buffer(frame_, 0) < 0)
        return false;
    encode_start_ticks_ = std::chrono::steady_clock::now();
    sws_context_ = sws_getContext(frame_->width, frame_->height, AV_PIX_FMT_RGBA, frame_->width, frame_->height,
                                  AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_context_)
        return false;
    std::cout << "[FairyStreamThread]:" << this << ": Codec Init Success" << std::endl;
    return true;
}

void FairyStreamThread::DestoryCodec()
{
    int ret = avcodec_send_frame(codec_context_, nullptr);
    if (ret < 0)
        std::cout << "[FairyStreamThread]:" << this << ": Failed To Send Flush Frame" << std::endl;
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(codec_context_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0)
            break;
        av_packet_unref(packet_);
    }
    sws_freeContext(sws_context_);
    avcodec_free_context(&codec_context_);
    av_frame_free(&frame_);
    av_packet_free(&packet_);
    std::cout << "[FairyStreamThread]:" << this << ": Codec Destory Success" << std::endl;
}

void FairyStreamThread::SendFrame(uint8_t* rgba_data)
{
    int ret;
    int64_t delta_time_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - encode_start_ticks_)
            .count();
    av_frame_make_writable(frame_);
    const uint8_t* src_slice[1] = { rgba_data };
    int src_stride[1] = { frame_->width * 4 };
    sws_scale(sws_context_, src_slice, src_stride, 0, frame_->height, frame_->data, frame_->linesize);
    frame_->pts = av_rescale_q(delta_time_, (AVRational) { 1, 1000 }, codec_context_->time_base);
    ret = avcodec_send_frame(codec_context_, frame_);
    if (ret < 0)
        std::cout << "[FairyStreamThread]:" << this << ": Failed To Send Frame On Pts:" << frame_->pts;
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(codec_context_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0)
            break;
        rtc::binary frame_data(packet_->size);
        std::memcpy(frame_data.data(), packet_->data, packet_->size);
        rtc::FrameInfo frame_info { std::chrono::duration<double>(static_cast<double>(delta_time_) / 1000) };
        frame_info.isKeyFrame = (packet_->flags & AV_PKT_FLAG_KEY);
        if (track_->isOpen())
            track_->sendFrame(std::move(frame_data), std::move(frame_info));
        av_packet_unref(packet_);
    }
}

} // namespace service
