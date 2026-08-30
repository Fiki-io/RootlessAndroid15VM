#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sys/types.h>

// Android Binder IOCTL Commands
#define BINDER_WRITE_READ         0xc0306201
#define BINDER_SET_MAX_THREADS    0x40046205
#define BINDER_SET_CONTEXT_MGR    0x40046207
#define BINDER_THREAD_EXIT        0x40046208
#define BINDER_VERSION            0xc0046209
#define BINDER_GET_NODE_DEBUG_INFO 0xc018620b

struct binder_version_info {
    int32_t protocol_version;
};

struct binder_write_read_data {
    uint64_t write_size;
    uint64_t write_consumed;
    uint64_t write_buffer;
    uint64_t read_size;
    uint64_t read_consumed;
    uint64_t read_buffer;
};

class VirtualBinder {
public:
    VirtualBinder();
    ~VirtualBinder();

    static int create_shared_memory(const std::string& name, size_t size);
    bool setup_binder_endpoints(const std::string& rootfs_path);

    // Menangani syscall ioctl yang ditujukan ke file descriptor Binder
    bool handle_ioctl(pid_t pid, int fd, unsigned long request, unsigned long long arg_addr, long& out_ret);

    // Cek apakah file descriptor adalah node Binder
    bool is_binder_fd(pid_t pid, int fd);
    void register_binder_fd(pid_t pid, int fd);
    void unregister_binder_fd(pid_t pid, int fd);

private:
    std::unordered_map<pid_t, std::vector<int>> tracked_fds;
    std::mutex binder_mutex;

    // Service Manager state (Context Manager PID)
    pid_t context_manager_pid = -1;
};
