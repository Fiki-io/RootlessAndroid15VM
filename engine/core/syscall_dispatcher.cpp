#include "syscall_dispatcher.hpp"
#include "path_resolver.hpp"
#include "process_tree.hpp"
#include "memory_manager.hpp"
#include "../ipc/virtual_binder.hpp"
#include "../display/virtual_framebuffer.hpp"

#include <iostream>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <elf.h>
#include <asm/unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "VMEngine_Dispatcher"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

SyscallDispatcher::SyscallDispatcher(PathResolver* resolver, ProcessTree* tree,
                                     VirtualBinder* binder, VirtualFramebuffer* fb)
    : path_resolver(resolver), process_tree(tree), virtual_binder(binder), virtual_fb(fb) {}

SyscallDispatcher::~SyscallDispatcher() {}

long SyscallDispatcher::get_syscall_nr(sys_regs_t* regs) {
#if defined(__aarch64__)
    return regs->regs[8];
#elif defined(__x86_64__)
    return regs->orig_rax;
#else
    return -1;
#endif
}

void SyscallDispatcher::cancel_syscall(pid_t pid, sys_regs_t* regs, long return_value) {
#if defined(__aarch64__)
    regs->regs[8] = -1;
    regs->regs[0] = return_value;
#elif defined(__x86_64__)
    regs->orig_rax = -1;
    regs->rax = return_value;
#endif
    struct iovec iov = { .iov_base = regs, .iov_len = sizeof(*regs) };
    ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
}

void SyscallDispatcher::handle_openat(pid_t pid, sys_regs_t* regs, bool is_entry) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[1];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rsi;
#else
    unsigned long long addr = 0;
#endif
    std::string path = MemoryManager::read_string(pid, addr);
    if (!path.empty() && path[0] == '/') {
        std::string resolved = path_resolver->resolve(pid, path);
        MemoryManager::write_string(pid, addr, resolved);

        // Jika membuka node Binder atau Framebuffer, simpan trackingnya
        if (virtual_binder && (path.find("/dev/binder") != std::string::npos ||
                               path.find("/dev/vndbinder") != std::string::npos ||
                               path.find("/dev/hwbinder") != std::string::npos)) {
            // Register di fd nanti atau tandai
        }
    }
}

void SyscallDispatcher::handle_close(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    int fd = regs->regs[0];
#elif defined(__x86_64__)
    int fd = regs->rdi;
#else
    int fd = -1;
#endif
    if (virtual_binder) virtual_binder->unregister_binder_fd(pid, fd);
    if (virtual_fb) virtual_fb->unregister_fb_fd(pid, fd);
}

void SyscallDispatcher::handle_ioctl(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    int fd = regs->regs[0];
    unsigned long req = regs->regs[1];
    unsigned long long arg = regs->regs[2];
#elif defined(__x86_64__)
    int fd = regs->rdi;
    unsigned long req = regs->rsi;
    unsigned long long arg = regs->rdx;
#else
    int fd = -1;
    unsigned long req = 0;
    unsigned long long arg = 0;
#endif

    long ret = 0;
    // 1. Coba tangani via VirtualBinder
    if (virtual_binder && virtual_binder->handle_ioctl(pid, fd, req, arg, ret)) {
        cancel_syscall(pid, regs, ret);
        return;
    }

    // 2. Coba tangani via VirtualFramebuffer
    if (virtual_fb && virtual_fb->handle_ioctl(pid, fd, req, arg, ret)) {
        cancel_syscall(pid, regs, ret);
        return;
    }
}

void SyscallDispatcher::handle_faccessat(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[1];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rsi;
#else
    unsigned long long addr = 0;
#endif
    std::string path = MemoryManager::read_string(pid, addr);
    if (!path.empty() && path[0] == '/') {
        std::string resolved = path_resolver->resolve(pid, path);
        MemoryManager::write_string(pid, addr, resolved);
    }
}

void SyscallDispatcher::handle_statx(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[1];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rsi;
#else
    unsigned long long addr = 0;
#endif
    std::string path = MemoryManager::read_string(pid, addr);
    if (!path.empty() && path[0] == '/') {
        std::string resolved = path_resolver->resolve(pid, path);
        MemoryManager::write_string(pid, addr, resolved);
    }
}

void SyscallDispatcher::handle_newfstatat(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[1];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rsi;
#else
    unsigned long long addr = 0;
#endif
    std::string path = MemoryManager::read_string(pid, addr);
    if (!path.empty() && path[0] == '/') {
        std::string resolved = path_resolver->resolve(pid, path);
        MemoryManager::write_string(pid, addr, resolved);
    }
}

void SyscallDispatcher::handle_readlinkat(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[1];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rsi;
#else
    unsigned long long addr = 0;
#endif
    std::string path = MemoryManager::read_string(pid, addr);
    if (!path.empty() && path[0] == '/') {
        std::string resolved = path_resolver->resolve(pid, path);
        MemoryManager::write_string(pid, addr, resolved);
    }
}

void SyscallDispatcher::handle_execve(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[0];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rdi;
#else
    unsigned long long addr = 0;
#endif
    std::string path = MemoryManager::read_string(pid, addr);
    if (!path.empty()) {
        std::string resolved = path_resolver->resolve(pid, path);
        MemoryManager::write_string(pid, addr, resolved);
        LOGI("[EXECVE] PID %d meluncurkan: %s", pid, resolved.c_str());
    }
}

void SyscallDispatcher::handle_chdir(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[0];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rdi;
#else
    unsigned long long addr = 0;
#endif
    std::string path = MemoryManager::read_string(pid, addr);
    if (!path.empty()) {
        path_resolver->set_cwd(pid, path);
        std::string resolved = path_resolver->resolve(pid, path);
        MemoryManager::write_string(pid, addr, resolved);
    }
}

void SyscallDispatcher::handle_getcwd(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long buf_addr = regs->regs[0];
    size_t size = regs->regs[1];
#elif defined(__x86_64__)
    unsigned long long buf_addr = regs->rdi;
    size_t size = regs->rsi;
#else
    unsigned long long buf_addr = 0;
    size_t size = 0;
#endif
    std::string cwd = path_resolver->get_cwd(pid);
    if (cwd.length() + 1 <= size && buf_addr != 0) {
        MemoryManager::write_string(pid, buf_addr, cwd);
        cancel_syscall(pid, regs, cwd.length() + 1);
    }
}

void SyscallDispatcher::handle_mount(pid_t pid, sys_regs_t* regs) {
    cancel_syscall(pid, regs, 0); // Fake Mount Success
}

void SyscallDispatcher::handle_fake_identity(pid_t pid, sys_regs_t* regs) {
    cancel_syscall(pid, regs, 0); // Fake UID/GID 0 (Root)
}

void SyscallDispatcher::handle_prctl(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    long option = regs->regs[0];
#elif defined(__x86_64__)
    long option = regs->rdi;
#else
    long option = 0;
#endif
    if (option == 4 || option == 38) { // PR_SET_DUMPABLE / PR_SET_NO_NEW_PRIVS
        cancel_syscall(pid, regs, 0);
    }
}

void SyscallDispatcher::handle_socket_connect(pid_t pid, sys_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long addr = regs->regs[1];
    socklen_t addrlen = regs->regs[2];
#elif defined(__x86_64__)
    unsigned long long addr = regs->rsi;
    socklen_t addrlen = regs->rdx;
#else
    unsigned long long addr = 0;
    socklen_t addrlen = 0;
#endif

    if (addr != 0 && addrlen >= sizeof(sa_family_t)) {
        struct sockaddr_un sun;
        if (MemoryManager::read_bytes(pid, addr, &sun, sizeof(sun))) {
            if (sun.sun_family == AF_UNIX && sun.sun_path[0] == '/') {
                std::string sock_path = sun.sun_path;
                std::string resolved = path_resolver->resolve(pid, sock_path);
                strncpy(sun.sun_path, resolved.c_str(), sizeof(sun.sun_path) - 1);
                MemoryManager::write_bytes(pid, addr, &sun, sizeof(sun));
            }
        }
    }
}

void SyscallDispatcher::dispatch(pid_t pid, sys_regs_t* regs, bool is_entry) {
    long nr = get_syscall_nr(regs);

    if (is_entry) {
        switch (nr) {
            case __NR_mount:
#if defined(__NR_umount2)
            case __NR_umount2:
#endif
                handle_mount(pid, regs);
                break;

            case __NR_ioctl:
                handle_ioctl(pid, regs);
                break;

            case __NR_close:
                handle_close(pid, regs);
                break;

            case __NR_getuid:
            case __NR_geteuid:
            case __NR_getgid:
            case __NR_getegid:
            case __NR_setuid:
            case __NR_setgid:
#if defined(__NR_setresuid)
            case __NR_setresuid:
            case __NR_setresgid:
#endif
                handle_fake_identity(pid, regs);
                break;

            case __NR_openat:
                handle_openat(pid, regs, true);
                break;

#if defined(__NR_faccessat)
            case __NR_faccessat:
                handle_faccessat(pid, regs);
                break;
#endif
#if defined(__NR_faccessat2)
            case __NR_faccessat2:
                handle_faccessat(pid, regs);
                break;
#endif

#if defined(__NR_statx)
            case __NR_statx:
                handle_statx(pid, regs);
                break;
#endif

#if defined(__NR_newfstatat)
            case __NR_newfstatat:
                handle_newfstatat(pid, regs);
                break;
#endif

            case __NR_readlinkat:
                handle_readlinkat(pid, regs);
                break;

            case __NR_execve:
#if defined(__NR_execveat)
            case __NR_execveat:
#endif
                handle_execve(pid, regs);
                break;

            case __NR_chdir:
                handle_chdir(pid, regs);
                break;

            case __NR_getcwd:
                handle_getcwd(pid, regs);
                break;

            case __NR_prctl:
                handle_prctl(pid, regs);
                break;

            case __NR_connect:
                handle_socket_connect(pid, regs);
                break;

            default:
                break;
        }
    }
}

pid_t SyscallDispatcher::spawn_guest(const std::string& entrypoint, const std::vector<std::string>& args) {
    pid_t pid = fork();

    if (pid < 0) {
        LOGE("Gagal fork proses guest!");
        return -1;
    }

    if (pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            _exit(1);
        }
        raise(SIGSTOP);

        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(entrypoint.c_str()));
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        std::string full_bin = path_resolver->resolve(getpid(), entrypoint);
        execv(full_bin.c_str(), c_args.data());
        _exit(1);
    }

    int status;
    waitpid(pid, &status, 0);

    ptrace(PTRACE_SETOPTIONS, pid, nullptr,
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
           PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT);

    process_tree->add_process(pid, getpid(), false);
    return pid;
}

void SyscallDispatcher::run_loop(pid_t initial_pid) {
    int status;

    while (process_tree->active_process_count() > 0) {
        pid_t current_pid = waitpid(-1, &status, __WALL);

        if (current_pid < 0) {
            if (errno == ECHILD) break;
            continue;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            process_tree->remove_process(current_pid);
            path_resolver->remove_process(current_pid);
            continue;
        }

        if (WIFSTOPPED(status)) {
            int event = (status >> 16) & 0xff;

            if (event == PTRACE_EVENT_FORK || event == PTRACE_EVENT_VFORK || event == PTRACE_EVENT_CLONE) {
                unsigned long new_pid = 0;
                ptrace(PTRACE_GETEVENTMSG, current_pid, nullptr, &new_pid);
                if (new_pid > 0) {
                    process_tree->add_process(new_pid, current_pid, (event == PTRACE_EVENT_CLONE));
                    path_resolver->set_cwd(new_pid, path_resolver->get_cwd(current_pid));
                }
            } else if (WSTOPSIG(status) == (SIGTRAP | 0x80)) {
                sys_regs_t regs;
                struct iovec iov = { .iov_base = &regs, .iov_len = sizeof(regs) };
                if (ptrace(PTRACE_GETREGSET, current_pid, NT_PRSTATUS, &iov) == 0) {
                    dispatch(current_pid, &regs, true);
                }
            }
        }

        ptrace(PTRACE_SYSCALL, current_pid, nullptr, nullptr);
    }
}
