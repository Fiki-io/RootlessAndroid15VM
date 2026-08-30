#include "property_service.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "VMEngine_Prop"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

// Protokol header Android Property Service
#define PROP_MSG_SETPROP 1

struct prop_msg {
    unsigned int cmd;
    char name[32];
    char value[92];
};

PropertyService::PropertyService() {
    // Default properties dasar Android 15
    set_property("ro.build.version.release", "15");
    set_property("ro.build.version.sdk", "35");
    set_property("ro.debuggable", "1");
    set_property("ro.secure", "0");
    set_property("ro.sf.lcd_density", "420");
    set_property("persist.sys.timezone", "Asia/Jakarta");
}

PropertyService::~PropertyService() {
    stop();
}

void PropertyService::set_property(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(prop_mutex);
    properties[key] = value;
}

std::string PropertyService::get_property(const std::string& key, const std::string& default_val) {
    std::lock_guard<std::mutex> lock(prop_mutex);
    auto it = properties.find(key);
    if (it != properties.end()) {
        return it->second;
    }
    return default_val;
}

void PropertyService::load_properties_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string k = line.substr(0, eq_pos);
            std::string v = line.substr(eq_pos + 1);
            set_property(k, v);
        }
    }
}

bool PropertyService::start(const std::string& rootfs_path) {
    stop();

    std::string socket_dir = rootfs_path + "/dev/socket";
    system(("mkdir -p " + socket_dir).c_str());

    socket_path = socket_dir + "/property_service";
    unlink(socket_path.c_str());

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("Gagal membuat socket property_service!");
        return false;
    }

    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, socket_path.c_str(), sizeof(sun.sun_path) - 1);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&sun), sizeof(sun)) < 0) {
        LOGE("Gagal bind socket property_service!");
        close(server_fd);
        server_fd = -1;
        return false;
    }

    listen(server_fd, 16);
    is_running = true;

    worker_thread = std::thread(&PropertyService::server_loop, this);
    LOGI("Property Service aktif di: %s", socket_path.c_str());
    return true;
}

void PropertyService::server_loop() {
    while (is_running) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            handle_client(client_fd);
            close(client_fd);
        }
    }
}

void PropertyService::handle_client(int client_fd) {
    struct prop_msg msg;
    ssize_t n = read(client_fd, &msg, sizeof(msg));
    if (n >= static_cast<ssize_t>(sizeof(msg))) {
        if (msg.cmd == PROP_MSG_SETPROP) {
            msg.name[sizeof(msg.name) - 1] = '\0';
            msg.value[sizeof(msg.value) - 1] = '\0';
            set_property(msg.name, msg.value);
            LOGI("[PROP_SET] %s = %s", msg.name, msg.value);
            int result = 0;
            write(client_fd, &result, sizeof(result));
        }
    }
}

void PropertyService::stop() {
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
