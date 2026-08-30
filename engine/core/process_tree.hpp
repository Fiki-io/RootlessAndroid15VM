#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <sys/types.h>

struct GuestProcess {
    pid_t pid;
    pid_t ppid;
    bool is_thread;
    bool is_alive;
    std::string name;
};

class ProcessTree {
public:
    ProcessTree();
    ~ProcessTree();

    void add_process(pid_t pid, pid_t ppid, bool is_thread = false);
    void set_process_name(pid_t pid, const std::string& name);
    void remove_process(pid_t pid);
    bool has_process(pid_t pid);

    std::vector<pid_t> get_all_pids();
    size_t active_process_count();
    void kill_all();

private:
    std::unordered_map<pid_t, GuestProcess> processes;
    std::mutex tree_mutex;
};
