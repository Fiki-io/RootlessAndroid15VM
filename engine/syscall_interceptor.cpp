#include "syscall_interceptor.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <elf.h>
#include <asm/unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "VMEngine_Syscall"
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

SyscallInterceptor::SyscallInterceptor(const std::string& guest_rootfs_path)
    : rootfs_path(guest_rootfs_path) {
    if (!rootfs_path.empty() && rootfs_path.back() == '/') {
        rootfs_path.pop_back();
    }
}

SyscallInterceptor::~SyscallInterceptor() {}

std::string SyscallInterceptor::translate_path(const std::string& original_path) {
    if (original_path.empty()) return original_path;

    if (original_path[0] == '/') {
        if (original_path.rfind(rootfs_path, 0) == 0) {
            return original_path;
        }
        return rootfs_path + original_path;
    }
    return original_path;
}

std::string SyscallInterceptor::read_string_from_child(pid_t pid, unsigned long long addr) {
    std::string result;
    if (addr == 0) return result;

    while (true) {
        errno = 0;
        long data = ptrace(PTRACE_PEEKDATA, pid, addr + result.size(), nullptr);
        if (errno != 0) break;

        char* bytes = reinterpret_cast<char*>(&data);
        for (size_t i = 0; i < sizeof(long); ++i) {
            if (bytes[i] == '\0') return result;
            result.push_back(bytes[i]);
        }
    }
    return result;
}

void SyscallInterceptor::write_string_to_child(pid_t pid, unsigned long long addr, const std::string& str) {
    size_t len = str.length() + 1;
    const char* buf = str.c_str();

    for (size_t i = 0; i < len; i += sizeof(long)) {
        long data = 0;
        size_t chunk = (len - i < sizeof(long)) ? (len - i) : sizeof(long);
        memcpy(&data, buf + i, chunk);
        ptrace(PTRACE_POKEDATA, pid, addr + i, data);
    }
}

pid_t SyscallInterceptor::launch_guest(const std::string& entrypoint_bin, const std::vector<std::string>& args) {
    pid_t pid = fork();

    if (pid < 0) {
        LOGE("Gagal melakukan fork proses guest!");
        return -1;
    }

    if (pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            LOGE("PTRACE_TRACEME gagal!");
            _exit(1);
        }

        raise(SIGSTOP);

        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(entrypoint_bin.c_str()));
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        std::string full_bin = translate_path(entrypoint_bin);
        execv(full_bin.c_str(), c_args.data());

        LOGE("Gagal mengeksekusi entrypoint: %s", full_bin.c_str());
        _exit(1);
    }

    int status;
    waitpid(pid, &status, 0);

    ptrace(PTRACE_SETOPTIONS, pid, nullptr,
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE);

    return pid;
}

void SyscallInterceptor::handle_mount(pid_t pid, vm_regs_t* regs) {
#if defined(__aarch64__)
    regs->regs[8] = -1;
    regs->regs[0] = 0;
#elif defined(__x86_64__)
    regs->orig_rax = -1;
    regs->rax = 0;
#endif
    struct iovec iov = { .iov_base = regs, .iov_len = sizeof(*regs) };
    ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
}

void SyscallInterceptor::handle_getuid(pid_t pid, vm_regs_t* regs) {
#if defined(__aarch64__)
    regs->regs[8] = -1;
    regs->regs[0] = 0;
#elif defined(__x86_64__)
    regs->orig_rax = -1;
    regs->rax = 0;
#endif
    struct iovec iov = { .iov_base = regs, .iov_len = sizeof(*regs) };
    ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
}

void SyscallInterceptor::handle_openat(pid_t pid, vm_regs_t* regs) {
#if defined(__aarch64__)
    unsigned long long path_addr = regs->regs[1];
#elif defined(__x86_64__)
    unsigned long long path_addr = regs->rsi;
#else
    unsigned long long path_addr = 0;
#endif

    if (path_addr != 0) {
        std::string original_path = read_string_from_child(pid, path_addr);
        if (!original_path.empty() && original_path[0] == '/') {
            std::string new_path = translate_path(original_path);
            write_string_to_child(pid, path_addr, new_path);
        }
    }
}

void SyscallInterceptor::handle_syscall(pid_t pid, vm_regs_t* regs) {
#if defined(__aarch64__)
    long syscall_no = regs->regs[8];
    switch (syscall_no) {
        case __NR_mount:
            handle_mount(pid, regs);
            break;
        case __NR_getuid:
        case __NR_geteuid:
        case __NR_getgid:
        case __NR_getegid:
            handle_getuid(pid, regs);
            break;
        case __NR_openat:
            handle_openat(pid, regs);
            break;
        default:
            break;
    }
#elif defined(__x86_64__)
    long syscall_no = regs->orig_rax;
    switch (syscall_no) {
        case __NR_mount:
            handle_mount(pid, regs);
            break;
        case __NR_getuid:
        case __NR_geteuid:
        case __NR_getgid:
        case __NR_getegid:
            handle_getuid(pid, regs);
            break;
        case __NR_openat:
            handle_openat(pid, regs);
            break;
        default:
            break;
    }
#endif
}

void SyscallInterceptor::run_event_loop(pid_t child_pid) {
    int status;

    while (true) {
        if (ptrace(PTRACE_SYSCALL, child_pid, nullptr, nullptr) < 0) {
            break;
        }

        if (waitpid(child_pid, &status, 0) < 0) {
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            LOGI("Proses guest (PID: %d) telah berhenti.", child_pid);
            break;
        }

        if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            vm_regs_t regs;
            struct iovec iov = { .iov_base = &regs, .iov_len = sizeof(regs) };
            if (ptrace(PTRACE_GETREGSET, child_pid, NT_PRSTATUS, &iov) == 0) {
                handle_syscall(child_pid, &regs);
            }
        }
    }
}
