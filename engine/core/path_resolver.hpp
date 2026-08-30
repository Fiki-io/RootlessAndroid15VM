#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

class PathResolver {
public:
    PathResolver(const std::string& rootfs_dir);
    ~PathResolver();

    // Resolusi path virtual guest ke path absolut di storage host
    std::string resolve(pid_t pid, const std::string& guest_path);

    // Set & Get Current Working Directory (CWD) per PID/Thread
    void set_cwd(pid_t pid, const std::string& cwd);
    std::string get_cwd(pid_t pid);
    void remove_process(pid_t pid);

    // Normalisasi path (menghapus '.' dan '..')
    static std::string normalize_path(const std::string& path);

    const std::string& get_rootfs() const { return rootfs_path; }

private:
    std::string rootfs_path;
    std::unordered_map<pid_t, std::string> process_cwds;
    std::mutex resolver_mutex;
};
