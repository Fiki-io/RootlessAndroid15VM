#include "virtual_framebuffer.hpp"
#include "gralloc_server.hpp"
#include "../core/memory_manager.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "VMEngine_FB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

VirtualFramebuffer::VirtualFramebuffer(GrallocServer* gralloc_srv)
    : gralloc(gralloc_srv) {}

VirtualFramebuffer::~VirtualFramebuffer() {}

void VirtualFramebuffer::setup_screeninfo() {
    memset(&var_info, 0, sizeof(var_info));
    var_info.xres = fb_width;
    var_info.yres = fb_height;
    var_info.xres_virtual = fb_width;
    var_info.yres_virtual = fb_height * 2; // Double buffering
    var_info.bits_per_pixel = 32;          // RGBA_8888

    var_info.red.offset = 0;
    var_info.red.length = 8;
    var_info.green.offset = 8;
    var_info.green.length = 8;
    var_info.blue.offset = 16;
    var_info.blue.length = 8;
    var_info.transp.offset = 24;
    var_info.transp.length = 8;

    memset(&fix_info, 0, sizeof(fix_info));
    strncpy(fix_info.id, "vm_virtual_fb", sizeof(fix_info.id) - 1);
    fix_info.type = FB_TYPE_PACKED_PIXELS;
    fix_info.visual = FB_VISUAL_TRUECOLOR;
    fix_info.line_length = fb_width * 4;
    fix_info.smem_len = fb_width * fb_height * 4 * 2;
}

bool VirtualFramebuffer::init_fb_device(const std::string& rootfs_path, int width, int height) {
    fb_width = width;
    fb_height = height;
    setup_screeninfo();

    std::string gfx_dir = rootfs_path + "/dev/graphics";
    system(("mkdir -p " + gfx_dir).c_str());

    std::string fb_node = gfx_dir + "/fb0";
    int fd = open(fb_node.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd >= 0) close(fd);

    LOGI("Virtual Framebuffer /dev/graphics/fb0 aktif (%dx%d, 32bpp)", width, height);
    return true;
}

bool VirtualFramebuffer::is_fb_fd(pid_t pid, int fd) {
    std::lock_guard<std::mutex> lock(fb_mutex);
    auto it = tracked_fb_fds.find(pid);
    if (it != tracked_fb_fds.end()) {
        const auto& fds = it->second;
        return std::find(fds.begin(), fds.end(), fd) != fds.end();
    }
    return false;
}

void VirtualFramebuffer::register_fb_fd(pid_t pid, int fd) {
    std::lock_guard<std::mutex> lock(fb_mutex);
    tracked_fb_fds[pid].push_back(fd);
    LOGI("[FB] PID %d membuka Framebuffer device /dev/graphics/fb0 (FD: %d)", pid, fd);
}

void VirtualFramebuffer::unregister_fb_fd(pid_t pid, int fd) {
    std::lock_guard<std::mutex> lock(fb_mutex);
    auto it = tracked_fb_fds.find(pid);
    if (it != tracked_fb_fds.end()) {
        auto& fds = it->second;
        fds.erase(std::remove(fds.begin(), fds.end(), fd), fds.end());
    }
}

bool VirtualFramebuffer::handle_ioctl(pid_t pid, int fd, unsigned long request, unsigned long long arg_addr, long& out_ret) {
    if (!is_fb_fd(pid, fd)) {
        return false;
    }

    switch (request) {
        case FBIOGET_VSCREENINFO: {
            MemoryManager::write_bytes(pid, arg_addr, &var_info, sizeof(var_info));
            out_ret = 0;
            return true;
        }

        case FBIOPUT_VSCREENINFO: {
            MemoryManager::read_bytes(pid, arg_addr, &var_info, sizeof(var_info));
            out_ret = 0;
            return true;
        }

        case FBIOGET_FSCREENINFO: {
            MemoryManager::write_bytes(pid, arg_addr, &fix_info, sizeof(fix_info));
            out_ret = 0;
            return true;
        }

        case FBIOPAN_DISPLAY: {
            // SurfaceFlinger memicu pan/swap buffer layar
            out_ret = 0;
            return true;
        }

        default:
            out_ret = 0;
            return true;
    }
}
