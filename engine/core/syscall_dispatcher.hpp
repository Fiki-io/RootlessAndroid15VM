#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/user.h>

#if defined(__aarch64__)
#include <asm/ptrace.h>
typedef struct user_pt_regs sys_regs_t;
#else
typedef struct user_regs_struct sys_regs_t;
#endif

class PathResolver;
class ProcessTree;
class VirtualBinder;
class VirtualFramebuffer;

class SyscallDispatcher {
public:
    SyscallDispatcher(PathResolver* resolver, ProcessTree* tree,
                      VirtualBinder* binder = nullptr,
                      VirtualFramebuffer* fb = nullptr);
    ~SyscallDispatcher();

    pid_t spawn_guest(const std::string& entrypoint, const std::vector<std::string>& args);
    void run_loop(pid_t initial_pid);
    void dispatch(pid_t pid, sys_regs_t* regs, bool is_entry);

private:
    PathResolver* path_resolver;
    ProcessTree* process_tree;
    VirtualBinder* virtual_binder;
    VirtualFramebuffer* virtual_fb;

    void handle_openat(pid_t pid, sys_regs_t* regs, bool is_entry);
    void handle_close(pid_t pid, sys_regs_t* regs);
    void handle_ioctl(pid_t pid, sys_regs_t* regs);
    void handle_faccessat(pid_t pid, sys_regs_t* regs);
    void handle_statx(pid_t pid, sys_regs_t* regs);
    void handle_newfstatat(pid_t pid, sys_regs_t* regs);
    void handle_readlinkat(pid_t pid, sys_regs_t* regs);
    void handle_execve(pid_t pid, sys_regs_t* regs);
    void handle_chdir(pid_t pid, sys_regs_t* regs);
    void handle_getcwd(pid_t pid, sys_regs_t* regs);
    void handle_mount(pid_t pid, sys_regs_t* regs);
    void handle_fake_identity(pid_t pid, sys_regs_t* regs);
    void handle_prctl(pid_t pid, sys_regs_t* regs);
    void handle_socket_connect(pid_t pid, sys_regs_t* regs);

    void cancel_syscall(pid_t pid, sys_regs_t* regs, long return_value);
    long get_syscall_nr(sys_regs_t* regs);
};
