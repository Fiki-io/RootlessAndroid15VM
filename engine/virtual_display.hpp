#pragma once

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <mutex>

class VirtualDisplay {
public:
    VirtualDisplay();
    ~VirtualDisplay();

    void set_native_window(ANativeWindow* window);
    void update_frame(const void* pixel_data, int width, int height);
    void release();

private:
    ANativeWindow* native_window = nullptr;
    std::mutex display_mutex;
};
