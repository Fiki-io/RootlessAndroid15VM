#include "gralloc_server.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include "../ipc/virtual_binder.hpp"

#define VM_FRAME_MAGIC 0x564D4652 // "VMFR"

GrallocServer::GrallocServer() {}
GrallocServer::~GrallocServer() {
    release();
}

bool GrallocServer::init(int width, int height) {
    release();

    size_t frame_bytes = width * height * 4;
    total_shm_size = sizeof(FrameHeader) + frame_bytes;

    shm_fd = VirtualBinder::create_shared_memory("vm_gralloc_shm", total_shm_size);
    if (shm_fd < 0) {
        return false;
    }

    shm_ptr = mmap(nullptr, total_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        close(shm_fd);
        shm_fd = -1;
        shm_ptr = nullptr;
        return false;
    }

    FrameHeader* hdr = static_cast<FrameHeader*>(shm_ptr);
    hdr->magic = VM_FRAME_MAGIC;
    hdr->width = width;
    hdr->height = height;
    hdr->format = 1; // RGBA_8888
    hdr->frame_seq = 0;
    hdr->data_size = frame_bytes;

    return true;
}

void GrallocServer::post_frame(const void* pixel_data, size_t size) {
    std::lock_guard<std::mutex> lock(gralloc_mutex);
    if (shm_ptr == nullptr || pixel_data == nullptr) return;

    FrameHeader* hdr = static_cast<FrameHeader*>(shm_ptr);
    uint8_t* dst = static_cast<uint8_t*>(shm_ptr) + sizeof(FrameHeader);

    size_t copy_len = (size < hdr->data_size) ? size : hdr->data_size;
    memcpy(dst, pixel_data, copy_len);

    hdr->frame_seq = ++current_seq;
}

bool GrallocServer::read_latest_frame(void* out_buf, size_t max_size, uint32_t& out_w, uint32_t& out_h) {
    std::lock_guard<std::mutex> lock(gralloc_mutex);
    if (shm_ptr == nullptr || out_buf == nullptr) return false;

    FrameHeader* hdr = static_cast<FrameHeader*>(shm_ptr);
    if (hdr->magic != VM_FRAME_MAGIC) return false;

    out_w = hdr->width;
    out_h = hdr->height;

    const uint8_t* src = static_cast<const uint8_t*>(shm_ptr) + sizeof(FrameHeader);
    size_t copy_len = (hdr->data_size < max_size) ? hdr->data_size : max_size;
    memcpy(out_buf, src, copy_len);

    return true;
}

void GrallocServer::release() {
    std::lock_guard<std::mutex> lock(gralloc_mutex);
    if (shm_ptr != nullptr && shm_ptr != MAP_FAILED) {
        munmap(shm_ptr, total_shm_size);
        shm_ptr = nullptr;
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_fd = -1;
    }
}
