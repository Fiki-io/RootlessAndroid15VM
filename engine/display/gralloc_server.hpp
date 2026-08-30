#pragma once

#include <string>
#include <mutex>
#include <atomic>

struct FrameHeader {
    uint32_t magic;      // 0x564D4652 ("VMFR")
    uint32_t width;
    uint32_t height;
    uint32_t format;     // HAL_PIXEL_FORMAT_RGBA_8888
    uint32_t frame_seq;
    uint32_t data_size;
};

class GrallocServer {
public:
    GrallocServer();
    ~GrallocServer();

    bool init(int width = 1080, int height = 2400);
    void post_frame(const void* pixel_data, size_t size);
    bool read_latest_frame(void* out_buf, size_t max_size, uint32_t& out_w, uint32_t& out_h);
    void release();

    int get_shm_fd() const { return shm_fd; }

private:
    int shm_fd = -1;
    void* shm_ptr = nullptr;
    size_t total_shm_size = 0;
    std::mutex gralloc_mutex;
    std::atomic<uint32_t> current_seq{0};
};
