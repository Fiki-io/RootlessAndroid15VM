#include "virtual_audio.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "VMEngine_Audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

VirtualAudio::VirtualAudio() {}
VirtualAudio::~VirtualAudio() {
    stop();
}

#ifdef __ANDROID__
bool VirtualAudio::init_aaudio() {
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
        LOGE("Gagal membuat AAudio Stream Builder!");
        return false;
    }

    AAudioStreamBuilder_setSampleRate(builder, sample_rate);
    AAudioStreamBuilder_setChannelCount(builder, channels);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);

    aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &aaudio_stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK) {
        LOGE("Gagal membuka AAudio Stream (Error: %d)!", result);
        return false;
    }

    AAudioStream_requestStart(aaudio_stream);
    LOGI("AAudio Stream berhasil dimulai (%d Hz, %d channels)", sample_rate, channels);
    return true;
}

void VirtualAudio::close_aaudio() {
    if (aaudio_stream != nullptr) {
        AAudioStream_requestStop(aaudio_stream);
        AAudioStream_close(aaudio_stream);
        aaudio_stream = nullptr;
    }
}
#endif

bool VirtualAudio::init_audio(const std::string& rootfs_path, int rate, int ch) {
    stop();
    sample_rate = rate;
    channels = ch;

    std::string snd_dir = rootfs_path + "/dev/snd";
    system(("mkdir -p " + snd_dir).c_str());

    fifo_path = snd_dir + "/pcmC0D0p";
    unlink(fifo_path.c_str());
    mkfifo(fifo_path.c_str(), 0666);

    audio_fifo_fd = open(fifo_path.c_str(), O_RDWR | O_NONBLOCK);
    if (audio_fifo_fd < 0) {
        LOGE("Gagal membuka audio FIFO: %s", fifo_path.c_str());
        return false;
    }

#ifdef __ANDROID__
    init_aaudio();
#endif

    is_running = true;
    audio_thread = std::thread(&VirtualAudio::audio_loop, this);

    LOGI("Virtual Audio sink aktif di: %s", fifo_path.c_str());
    return true;
}

void VirtualAudio::write_pcm(const void* buffer, size_t size) {
#ifdef __ANDROID__
    if (aaudio_stream != nullptr && buffer != nullptr && size > 0) {
        int32_t num_frames = size / (channels * sizeof(int16_t));
        AAudioStream_write(aaudio_stream, buffer, num_frames, 0);
    }
#endif
}

void VirtualAudio::audio_loop() {
    std::vector<uint8_t> buffer(4096);

    while (is_running) {
        if (audio_fifo_fd >= 0) {
            ssize_t n = read(audio_fifo_fd, buffer.data(), buffer.size());
            if (n > 0) {
                write_pcm(buffer.data(), n);
            } else {
                usleep(10000); // 10ms
            }
        } else {
            break;
        }
    }
}

void VirtualAudio::stop() {
    is_running = false;
    if (audio_fifo_fd >= 0) {
        close(audio_fifo_fd);
        audio_fifo_fd = -1;
    }
    if (!fifo_path.empty()) {
        unlink(fifo_path.c_str());
    }
#ifdef __ANDROID__
    close_aaudio();
#endif
    if (audio_thread.joinable()) {
        audio_thread.join();
    }
}
