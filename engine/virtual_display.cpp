#include "virtual_display.hpp"
#include <cstring>
#include <android/log.h>

#define TAG "VMEngine_Display"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

VirtualDisplay::VirtualDisplay() {}

VirtualDisplay::~VirtualDisplay() {
    release();
}

void VirtualDisplay::set_native_window(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(display_mutex);
    if (native_window != nullptr) {
        ANativeWindow_release(native_window);
    }
    native_window = window;
    if (native_window != nullptr) {
        // Atur format buffer RGBA_8888
        ANativeWindow_setBuffersGeometry(native_window, 0, 0, WINDOW_FORMAT_RGBA_8888);
        LOGI("ANativeWindow berhasil dikonfigurasi.");
    }
}

void VirtualDisplay::update_frame(const void* pixel_data, int width, int height) {
    std::lock_guard<std::mutex> lock(display_mutex);
    if (native_window == nullptr || pixel_data == nullptr) return;

    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(native_window, &buffer, nullptr) < 0) {
        LOGE("Gagal me-lock ANativeWindow buffer!");
        return;
    }

    // Salin baris demi baris ke window buffer
    uint8_t* dst = static_cast<uint8_t*>(buffer.bits);
    const uint8_t* src = static_cast<const uint8_t*>(pixel_data);
    int copy_bytes_per_row = (width < buffer.width ? width : buffer.width) * 4;

    for (int y = 0; y < buffer.height && y < height; ++y) {
        memcpy(dst + (y * buffer.stride * 4), src + (y * width * 4), copy_bytes_per_row);
    }

    ANativeWindow_unlockAndPost(native_window);
}

void VirtualDisplay::release() {
    std::lock_guard<std::mutex> lock(display_mutex);
    if (native_window != nullptr) {
        ANativeWindow_release(native_window);
        native_window = nullptr;
    }
}
