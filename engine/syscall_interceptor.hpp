#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/user.h>

#if defined(__aarch64__)
#include <asm/ptrace.h>
typedef struct user_pt_regs vm_regs_t;
#else
typedef struct user_regs_struct vm_regs_t;
#endif

class SyscallInterceptor {
public:
    SyscallInterceptor(const std::string& guest_rootfs_path);
    ~SyscallInterceptor();

    pid_t launch_guest(const std::string& entrypoint_bin, const std::vector<std::string>& args);
    void run_event_loop(pid_t child_pid);

private:
    std::string rootfs_path;

    void handle_syscall(pid_t pid, vm_regs_t* regs);
    void handle_openat(pid_t pid, vm_regs_t* regs);
    void handle_mount(pid_t pid, vm_regs_t* regs);
    void handle_getuid(pid_t pid, vm_regs_t* regs);

    std::string read_string_from_child(pid_t pid, unsigned long long addr);
    void write_string_to_child(pid_t pid, unsigned long long addr, const std::string& str);
    std::string translate_path(const std::string& original_path);
};
