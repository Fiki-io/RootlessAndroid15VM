#pragma once

#include <string>
#include <thread>
#include <atomic>

class DNSProxy {
public:
    DNSProxy();
    ~DNSProxy();

    // Memulai server socket dnsproxyd di rootfs
    bool start(const std::string& rootfs_path);
    void stop();

    // Membuat file /system/etc/resolv.conf dengan nameserver standar
    static bool setup_resolv_conf(const std::string& rootfs_path);

private:
    std::atomic<bool> is_running{false};
    int server_fd = -1;
    std::string socket_path;
    std::thread worker_thread;

    void server_loop();
    void handle_client(int client_fd);
};
