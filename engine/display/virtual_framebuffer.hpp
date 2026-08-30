#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <linux/fb.h>

class GrallocServer;

class VirtualFramebuffer {
public:
    VirtualFramebuffer(GrallocServer* gralloc_srv);
    ~VirtualFramebuffer();

    bool init_fb_device(const std::string& rootfs_path, int width = 1080, int height = 2400);

    bool is_fb_fd(pid_t pid, int fd);
    void register_fb_fd(pid_t pid, int fd);
    void unregister_fb_fd(pid_t pid, int fd);

    // Menangani ioctl framebuffer standar Linux (FBIOGET_VSCREENINFO, FBIOPAN_DISPLAY, dll)
    bool handle_ioctl(pid_t pid, int fd, unsigned long request, unsigned long long arg_addr, long& out_ret);

private:
    GrallocServer* gralloc;
    int fb_width = 1080;
    int fb_height = 2400;
    std::unordered_map<pid_t, std::vector<int>> tracked_fb_fds;
    std::mutex fb_mutex;

    struct fb_var_screeninfo var_info;
    struct fb_fix_screeninfo fix_info;

    void setup_screeninfo();
};
