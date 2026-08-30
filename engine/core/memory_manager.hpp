#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    // Membaca string null-terminated dari memori proses target
    static std::string read_string(pid_t pid, unsigned long long remote_addr, size_t max_len = 4096);

    // Menulis string ke memori proses target
    static bool write_string(pid_t pid, unsigned long long remote_addr, const std::string& str);

    // Membaca raw buffer memori
    static bool read_bytes(pid_t pid, unsigned long long remote_addr, void* local_buf, size_t len);

    // Menulis raw buffer memori
    static bool write_bytes(pid_t pid, unsigned long long remote_addr, const void* local_buf, size_t len);
};
