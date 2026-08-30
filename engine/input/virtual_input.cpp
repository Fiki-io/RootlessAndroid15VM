#include "virtual_input.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <cstring>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "VMEngine_Input"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

#define ACTION_DOWN   0
#define ACTION_UP     1
#define ACTION_MOVE   2
#define ACTION_CANCEL 3

VirtualInput::VirtualInput() {}

VirtualInput::~VirtualInput() {
    close_device();
}

bool VirtualInput::init_device(const std::string& fifo_path) {
    std::lock_guard<std::mutex> lock(input_mutex);
    close_device();

    mkfifo(fifo_path.c_str(), 0666);
    fifo_fd = open(fifo_path.c_str(), O_RDWR | O_NONBLOCK);

    if (fifo_fd < 0) {
        LOGE("Gagal membuka input FIFO di: %s", fifo_path.c_str());
        return false;
    }

    LOGI("Virtual input device siap di: %s", fifo_path.c_str());
    return true;
}

void VirtualInput::write_event(uint16_t type, uint16_t code, int32_t value) {
    if (fifo_fd < 0) return;

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, nullptr);
    ev.type = type;
    ev.code = code;
    ev.value = value;

    write(fifo_fd, &ev, sizeof(ev));
}

void VirtualInput::sync() {
    write_event(EV_SYN, SYN_REPORT, 0);
}

void VirtualInput::send_touch(int action, int slot, float x, float y, int screen_w, int screen_h) {
    std::lock_guard<std::mutex> lock(input_mutex);
    if (fifo_fd < 0) return;

    int32_t abs_x = static_cast<int32_t>(x);
    int32_t abs_y = static_cast<int32_t>(y);

    write_event(EV_ABS, ABS_MT_SLOT, slot);

    switch (action) {
        case ACTION_DOWN:
            write_event(EV_ABS, ABS_MT_TRACKING_ID, slot + 1);
            write_event(EV_ABS, ABS_MT_POSITION_X, abs_x);
            write_event(EV_ABS, ABS_MT_POSITION_Y, abs_y);
            write_event(EV_ABS, ABS_MT_TOUCH_MAJOR, 10);
            write_event(EV_KEY, BTN_TOUCH, 1);
            break;

        case ACTION_MOVE:
            write_event(EV_ABS, ABS_MT_POSITION_X, abs_x);
            write_event(EV_ABS, ABS_MT_POSITION_Y, abs_y);
            break;

        case ACTION_UP:
        case ACTION_CANCEL:
            write_event(EV_ABS, ABS_MT_TRACKING_ID, -1);
            write_event(EV_KEY, BTN_TOUCH, 0);
            break;
    }

    sync();
}

void VirtualInput::send_key(int key_code, bool is_down) {
    std::lock_guard<std::mutex> lock(input_mutex);
    if (fifo_fd < 0) return;

    write_event(EV_KEY, key_code, is_down ? 1 : 0);
    sync();
}

void VirtualInput::close_device() {
    if (fifo_fd >= 0) {
        close(fifo_fd);
        fifo_fd = -1;
    }
}
