#pragma once

#include <string>
#include <sys/types.h>

class VirtualBinder {
public:
    VirtualBinder();
    ~VirtualBinder();

    // Buat anonymous shared memory (pengganti /dev/ashmem yang diblokir SELinux di non-root)
    static int create_shared_memory(const std::string& name, size_t size);

    // Inisialisasi virtual binder endpoint
    bool setup_binder_endpoints(const std::string& rootfs_path);
};
