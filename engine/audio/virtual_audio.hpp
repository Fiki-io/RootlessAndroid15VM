#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef __ANDROID__
#include <aaudio/AAudio.h>
#endif

class VirtualAudio {
public:
    VirtualAudio();
    ~VirtualAudio();

    bool init_audio(const std::string& rootfs_path, int sample_rate = 48000, int channels = 2);
    void write_pcm(const void* buffer, size_t size);
    void stop();

private:
    std::atomic<bool> is_running{false};
    int audio_fifo_fd = -1;
    std::string fifo_path;
    int sample_rate = 48000;
    int channels = 2;

    std::thread audio_thread;
    std::mutex audio_mutex;

#ifdef __ANDROID__
    AAudioStream* aaudio_stream = nullptr;
    bool init_aaudio();
    void close_aaudio();
#endif

    void audio_loop();
};
