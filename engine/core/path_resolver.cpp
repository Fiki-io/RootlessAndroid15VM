#include "path_resolver.hpp"
#include <vector>
#include <sstream>

PathResolver::PathResolver(const std::string& rootfs_dir)
    : rootfs_path(rootfs_dir) {
    if (!rootfs_path.empty() && rootfs_path.back() == '/') {
        rootfs_path.pop_back();
    }
}

PathResolver::~PathResolver() {}

std::string PathResolver::normalize_path(const std::string& path) {
    if (path.empty()) return "/";

    std::vector<std::string> segments;
    std::stringstream ss(path);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;
        }
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            }
        } else {
            segments.push_back(segment);
        }
    }

    std::string result = "";
    for (const auto& s : segments) {
        result += "/" + s;
    }
    return result.empty() ? "/" : result;
}

std::string PathResolver::resolve(pid_t pid, const std::string& guest_path) {
    if (guest_path.empty()) return rootfs_path;

    std::string full_guest_path;
    if (guest_path[0] == '/') {
        full_guest_path = guest_path;
    } else {
        std::string cwd = get_cwd(pid);
        if (cwd.back() == '/') {
            full_guest_path = cwd + guest_path;
        } else {
            full_guest_path = cwd + "/" + guest_path;
        }
    }

    std::string normalized = normalize_path(full_guest_path);

    // 1. Shared Storage Binding: Bagikan folder Download HP Host langsung ke dalam Guest VM
    if (normalized.rfind("/sdcard/Download", 0) == 0 ||
        normalized.rfind("/storage/emulated/0/Download", 0) == 0) {
        return "/sdcard/Download";
    }

    // 2. Jangan dobel prefix jika sudah di dalam rootfs
    if (normalized.rfind(rootfs_path, 0) == 0) {
        return normalized;
    }

    return rootfs_path + normalized;
}

void PathResolver::set_cwd(pid_t pid, const std::string& cwd) {
    std::lock_guard<std::mutex> lock(resolver_mutex);
    process_cwds[pid] = normalize_path(cwd);
}

std::string PathResolver::get_cwd(pid_t pid) {
    std::lock_guard<std::mutex> lock(resolver_mutex);
    auto it = process_cwds.find(pid);
    if (it != process_cwds.end()) {
        return it->second;
    }
    return "/";
}

void PathResolver::remove_process(pid_t pid) {
    std::lock_guard<std::mutex> lock(resolver_mutex);
    process_cwds.erase(pid);
}
