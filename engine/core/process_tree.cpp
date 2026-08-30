#include "process_tree.hpp"
#include <csignal>
#include <unistd.h>

ProcessTree::ProcessTree() {}
ProcessTree::~ProcessTree() {
    kill_all();
}

void ProcessTree::add_process(pid_t pid, pid_t ppid, bool is_thread) {
    std::lock_guard<std::mutex> lock(tree_mutex);
    GuestProcess proc;
    proc.pid = pid;
    proc.ppid = ppid;
    proc.is_thread = is_thread;
    proc.is_alive = true;
    processes[pid] = proc;
}

void ProcessTree::set_process_name(pid_t pid, const std::string& name) {
    std::lock_guard<std::mutex> lock(tree_mutex);
    auto it = processes.find(pid);
    if (it != processes.end()) {
        it->second.name = name;
    }
}

void ProcessTree::remove_process(pid_t pid) {
    std::lock_guard<std::mutex> lock(tree_mutex);
    processes.erase(pid);
}

bool ProcessTree::has_process(pid_t pid) {
    std::lock_guard<std::mutex> lock(tree_mutex);
    return processes.find(pid) != processes.end();
}

std::vector<pid_t> ProcessTree::get_all_pids() {
    std::lock_guard<std::mutex> lock(tree_mutex);
    std::vector<pid_t> result;
    for (const auto& pair : processes) {
        result.push_back(pair.first);
    }
    return result;
}

size_t ProcessTree::active_process_count() {
    std::lock_guard<std::mutex> lock(tree_mutex);
    return processes.size();
}

void ProcessTree::kill_all() {
    std::lock_guard<std::mutex> lock(tree_mutex);
    for (const auto& pair : processes) {
        kill(pair.first, SIGKILL);
    }
    processes.clear();
}
