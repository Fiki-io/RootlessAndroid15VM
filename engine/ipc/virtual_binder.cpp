#include "virtual_binder.hpp"
#include "../core/memory_manager.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>

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

// Versi protokol Binder Android 15 (Kernel 64-bit Binder Version = 8)
#define CURRENT_BINDER_PROTOCOL_VERSION 8

VirtualBinder::VirtualBinder() {}
VirtualBinder::~VirtualBinder() {}

int VirtualBinder::create_shared_memory(const std::string& name, size_t size) {
#ifdef __ANDROID__
    int fd = ASharedMemory_create(name.c_str(), size);
    if (fd >= 0) return fd;
#endif

#if defined(__NR_memfd_create)
    int fd = syscall(__NR_memfd_create, name.c_str(), 0);
    if (fd >= 0) {
        if (ftruncate(fd, size) == 0) return fd;
        close(fd);
    }
#endif

    LOGE("Gagal membuat anonymous shared memory!");
    return -1;
}

bool VirtualBinder::setup_binder_endpoints(const std::string& rootfs_path) {
    std::string dev_dir = rootfs_path + "/dev";
    system(("mkdir -p " + dev_dir).c_str());

    // Buat file stub /dev/binder, /dev/vndbinder, /dev/hwbinder agar open() berhasil
    const char* nodes[] = {"/dev/binder", "/dev/vndbinder", "/dev/hwbinder"};
    for (const char* node : nodes) {
        std::string full_node = rootfs_path + node;
        int fd = open(full_node.c_str(), O_RDWR | O_CREAT, 0666);
        if (fd >= 0) close(fd);
    }

    LOGI("Menyiapkan endpoint Binder virtual di: %s", dev_dir.c_str());
    return true;
}

bool VirtualBinder::is_binder_fd(pid_t pid, int fd) {
    std::lock_guard<std::mutex> lock(binder_mutex);
    auto it = tracked_fds.find(pid);
    if (it != tracked_fds.end()) {
        const auto& fds = it->second;
        return std::find(fds.begin(), fds.end(), fd) != fds.end();
    }
    return false;
}

void VirtualBinder::register_binder_fd(pid_t pid, int fd) {
    std::lock_guard<std::mutex> lock(binder_mutex);
    tracked_fds[pid].push_back(fd);
    LOGI("[BINDER] PID %d membuka Binder node (FD: %d)", pid, fd);
}

void VirtualBinder::unregister_binder_fd(pid_t pid, int fd) {
    std::lock_guard<std::mutex> lock(binder_mutex);
    auto it = tracked_fds.find(pid);
    if (it != tracked_fds.end()) {
        auto& fds = it->second;
        fds.erase(std::remove(fds.begin(), fds.end(), fd), fds.end());
    }
}

bool VirtualBinder::handle_ioctl(pid_t pid, int fd, unsigned long request, unsigned long long arg_addr, long& out_ret) {
    if (!is_binder_fd(pid, fd)) {
        return false; // Bukan file descriptor Binder
    }

    switch (request) {
        case BINDER_VERSION: {
            // Balas dengan versi protokol Binder yang diharapkan Android 15
            struct binder_version_info ver;
            ver.protocol_version = CURRENT_BINDER_PROTOCOL_VERSION;
            MemoryManager::write_bytes(pid, arg_addr, &ver, sizeof(ver));
            out_ret = 0;
            return true;
        }

        case BINDER_SET_MAX_THREADS: {
            // Set batas thread pool
            out_ret = 0;
            return true;
        }

        case BINDER_SET_CONTEXT_MGR: {
            // Service Manager mendaftarkan dirinya sebagai root context manager
            context_manager_pid = pid;
            LOGI("[BINDER] PID %d terdaftar sebagai ServiceManager (Context Manager)!", pid);
            out_ret = 0;
            return true;
        }

        case BINDER_WRITE_READ: {
            // Transaksi write/read Binder
            struct binder_write_read_data bwr;
            if (MemoryManager::read_bytes(pid, arg_addr, &bwr, sizeof(bwr))) {
                // Tandai bahwa seluruh write buffer telah dikonsumsi
                bwr.write_consumed = bwr.write_size;
                bwr.read_consumed = 0;
                MemoryManager::write_bytes(pid, arg_addr, &bwr, sizeof(bwr));
            }
            out_ret = 0;
            return true;
        }

        case BINDER_THREAD_EXIT: {
            out_ret = 0;
            return true;
        }

        default:
            out_ret = 0;
            return true;
    }
}
