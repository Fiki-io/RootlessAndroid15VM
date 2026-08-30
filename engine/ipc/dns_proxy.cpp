#include "dns_proxy.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fstream>
#include <cstring>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "VMEngine_DNS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

DNSProxy::DNSProxy() {}
DNSProxy::~DNSProxy() {
    stop();
}

bool DNSProxy::setup_resolv_conf(const std::string& rootfs_path) {
    std::string etc_dir = rootfs_path + "/system/etc";
    system(("mkdir -p " + etc_dir).c_str());

    std::string resolv_file = etc_dir + "/resolv.conf";
    std::ofstream out(resolv_file);
    if (out.is_open()) {
        out << "# Android 15 Virtual DNS Configuration\n";
        out << "nameserver 8.8.8.8\n";
        out << "nameserver 1.1.1.1\n";
        out << "nameserver 8.8.4.4\n";
        out.close();
        LOGI("DNS resolv.conf berhasil dibuat di: %s", resolv_file.c_str());
        return true;
    }
    return false;
}

bool DNSProxy::start(const std::string& rootfs_path) {
    stop();
    setup_resolv_conf(rootfs_path);

    std::string socket_dir = rootfs_path + "/dev/socket";
    system(("mkdir -p " + socket_dir).c_str());

    socket_path = socket_dir + "/dnsproxyd";
    unlink(socket_path.c_str());

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("Gagal membuat socket dnsproxyd!");
        return false;
    }

    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, socket_path.c_str(), sizeof(sun.sun_path) - 1);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&sun), sizeof(sun)) < 0) {
        LOGE("Gagal bind socket dnsproxyd!");
        close(server_fd);
        server_fd = -1;
        return false;
    }

    listen(server_fd, 16);
    is_running = true;

    worker_thread = std::thread(&DNSProxy::server_loop, this);
    LOGI("DNS Proxy aktif di: %s", socket_path.c_str());
    return true;
}

void DNSProxy::server_loop() {
    while (is_running) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            handle_client(client_fd);
            close(client_fd);
        }
    }
}

void DNSProxy::handle_client(int client_fd) {
    char buf[512];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        // Android netd query format: getaddrinfo/gethostbyname
        // Respons sukses sederhana dengan fallback
        uint32_t success_code = 0;
        write(client_fd, &success_code, sizeof(success_code));
    }
}

void DNSProxy::stop() {
    is_running = false;
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    if (!socket_path.empty()) {
        unlink(socket_path.c_str());
    }
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}
