#include "memory_manager.hpp"
#include <sys/uio.h>
#include <sys/ptrace.h>
#include <cstring>
#include <cerrno>
#include <cstdint>

MemoryManager::MemoryManager() {}
MemoryManager::~MemoryManager() {}

bool MemoryManager::read_bytes(pid_t pid, unsigned long long remote_addr, void* local_buf, size_t len) {
    if (len == 0 || local_buf == nullptr || remote_addr == 0) return false;

    struct iovec local_iov = { .iov_base = local_buf, .iov_len = len };
    struct iovec remote_iov = { .iov_base = reinterpret_cast<void*>(remote_addr), .iov_len = len };

    // 1. Coba process_vm_readv (Cepat, 1 syscall)
    ssize_t nread = process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);
    if (nread == static_cast<ssize_t>(len)) {
        return true;
    }

    // 2. Fallback ke PTRACE_PEEKDATA jika process_vm_readv gagal
    uint8_t* dst = static_cast<uint8_t*>(local_buf);
    for (size_t i = 0; i < len; i += sizeof(long)) {
        errno = 0;
        long data = ptrace(PTRACE_PEEKDATA, pid, remote_addr + i, nullptr);
        if (errno != 0) return false;

        size_t chunk = (len - i < sizeof(long)) ? (len - i) : sizeof(long);
        memcpy(dst + i, &data, chunk);
    }
    return true;
}

bool MemoryManager::write_bytes(pid_t pid, unsigned long long remote_addr, const void* local_buf, size_t len) {
    if (len == 0 || local_buf == nullptr || remote_addr == 0) return false;

    struct iovec local_iov = { .iov_base = const_cast<void*>(local_buf), .iov_len = len };
    struct iovec remote_iov = { .iov_base = reinterpret_cast<void*>(remote_addr), .iov_len = len };

    // 1. Coba process_vm_writev
    ssize_t nwritten = process_vm_writev(pid, &local_iov, 1, &remote_iov, 1, 0);
    if (nwritten == static_cast<ssize_t>(len)) {
        return true;
    }

    // 2. Fallback ke PTRACE_POKEDATA
    const uint8_t* src = static_cast<const uint8_t*>(local_buf);
    for (size_t i = 0; i < len; i += sizeof(long)) {
        long data = 0;
        size_t chunk = (len - i < sizeof(long)) ? (len - i) : sizeof(long);

        if (chunk < sizeof(long)) {
            data = ptrace(PTRACE_PEEKDATA, pid, remote_addr + i, nullptr);
        }
        memcpy(&data, src + i, chunk);
        if (ptrace(PTRACE_POKEDATA, pid, remote_addr + i, data) < 0) {
            return false;
        }
    }
    return true;
}

std::string MemoryManager::read_string(pid_t pid, unsigned long long remote_addr, size_t max_len) {
    if (remote_addr == 0) return "";

    std::vector<char> buffer(256);
    std::string result;
    size_t offset = 0;

    while (offset < max_len) {
        size_t chunk_size = buffer.size();
        if (!read_bytes(pid, remote_addr + offset, buffer.data(), chunk_size)) {
            break;
        }

        for (size_t i = 0; i < chunk_size; ++i) {
            if (buffer[i] == '\0') {
                return result;
            }
            result.push_back(buffer[i]);
            offset++;
            if (offset >= max_len) return result;
        }
    }
    return result;
}

bool MemoryManager::write_string(pid_t pid, unsigned long long remote_addr, const std::string& str) {
    return write_bytes(pid, remote_addr, str.c_str(), str.length() + 1);
}
