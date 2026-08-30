#pragma once

#include <string>
#include <linux/input.h>
#include <mutex>

class VirtualInput {
public:
    VirtualInput();
    ~VirtualInput();

    // Inisialisasi pipe/FIFO untuk virtual input device
    bool init_device(const std::string& fifo_path);

    // Kirim event sentuhan multi-touch (Linux Input Protocol Type B)
    void send_touch(int action, int slot, float x, float y, int screen_w = 1080, int screen_h = 2400);

    // Kirim event tombol fisik (Power, Volume, Back)
    void send_key(int key_code, bool is_down);

    void close_device();

private:
    int fifo_fd = -1;
    std::mutex input_mutex;

    void write_event(uint16_t type, uint16_t code, int32_t value);
    void sync();
};
