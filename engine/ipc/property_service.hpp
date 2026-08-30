#pragma once

#include <string>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

class PropertyService {
public:
    PropertyService();
    ~PropertyService();

    // Memulai server socket property_service
    bool start(const std::string& rootfs_path);
    void stop();

    // Manipulasi properties secara lokal di C++
    void set_property(const std::string& key, const std::string& value);
    std::string get_property(const std::string& key, const std::string& default_val = "");

    void load_properties_file(const std::string& filepath);

private:
    std::unordered_map<std::string, std::string> properties;
    std::mutex prop_mutex;
    std::atomic<bool> is_running{false};
    int server_fd = -1;
    std::string socket_path;
    std::thread worker_thread;

    void server_loop();
    void handle_client(int client_fd);
};
