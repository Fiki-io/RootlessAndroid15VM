#include "virtual_binder.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/stat.h>

#ifdef __ANDROID__
#include <android/sharedmem.h>
#include <android/log.h>
#define TAG "VMEngine_Binder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

VirtualBinder::VirtualBinder() {}

VirtualBinder::~VirtualBinder() {}

int VirtualBinder::create_shared_memory(const std::string& name, size_t size) {
#ifdef __ANDROID__
    int fd = ASharedMemory_create(name.c_str(), size);
    if (fd >= 0) {
        return fd;
    }
#endif

#if defined(__NR_memfd_create)
    int fd = syscall(__NR_memfd_create, name.c_str(), 0);
    if (fd >= 0) {
        if (ftruncate(fd, size) == 0) {
            return fd;
        }
        close(fd);
    }
#endif

    LOGE("Gagal membuat anonymous shared memory!");
    return -1;
}

bool VirtualBinder::setup_binder_endpoints(const std::string& rootfs_path) {
    std::string dev_dir = rootfs_path + "/dev";
    mkdir(dev_dir.c_str(), 0755);

    std::string binder_path = dev_dir + "/binder";
    std::string vndbinder_path = dev_dir + "/vndbinder";
    std::string hwbinder_path = dev_dir + "/hwbinder";

    LOGI("Menyiapkan endpoint Binder virtual di: %s", dev_dir.c_str());
    return true;
}
