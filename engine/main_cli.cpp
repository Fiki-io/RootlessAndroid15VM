#include <iostream>
#include <vector>
#include <string>

#include "core/path_resolver.hpp"
#include "core/process_tree.hpp"
#include "core/syscall_dispatcher.hpp"
#include "ipc/property_service.hpp"
#include "ipc/dns_proxy.hpp"
#include "ipc/virtual_binder.hpp"
#include "audio/virtual_audio.hpp"
#include "input/virtual_input.hpp"
#include "display/gralloc_server.hpp"
#include "display/virtual_framebuffer.hpp"

int main(int argc, char* argv[]) {
    std::cout << "\033[1;32m====================================================\033[0m\n";
    std::cout << "\033[1;32m=== Rootless Android 15 VM Engine (Production)  ===\033[0m\n";
    std::cout << "\033[1;32m====================================================\033[0m\n";

    if (argc < 3) {
        std::cout << "Penggunaan: " << argv[0] << " <path_to_rootfs> <entrypoint_bin> [args...]\n";
        std::cout << "Contoh: " << argv[0] << " ./rootfs /init_guest.sh\n";
        return 1;
    }

    std::string rootfs_dir = argv[1];
    std::string entrypoint = argv[2];

    std::vector<std::string> args;
    for (int i = 3; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    std::cout << "[*] Menginisialisasi PathResolver di: " << rootfs_dir << "\n";
    PathResolver resolver(rootfs_dir);
    ProcessTree process_tree;

    std::cout << "[*] Menginisialisasi Property Service & DNS Proxy...\n";
    PropertyService prop_service;
    prop_service.load_properties_file(rootfs_dir + "/system/build.prop");
    prop_service.start(rootfs_dir);

    DNSProxy dns_proxy;
    dns_proxy.start(rootfs_dir);

    std::cout << "[*] Menyiapkan Virtual Binder & Shared Memory...\n";
    VirtualBinder binder;
    binder.setup_binder_endpoints(rootfs_dir);

    std::cout << "[*] Menyiapkan Virtual Audio, Input & Gralloc Ring-Buffer...\n";
    VirtualAudio audio;
    audio.init_audio(rootfs_dir, 48000, 2);

    VirtualInput input;
    input.init_device(rootfs_dir + "/dev/input_event0");

    GrallocServer gralloc;
    gralloc.init(1080, 2400);

    VirtualFramebuffer fb(&gralloc);
    fb.init_fb_device(rootfs_dir, 1080, 2400);

    SyscallDispatcher dispatcher(&resolver, &process_tree, &binder, &fb);

    std::cout << "\033[1;32m[+] Meluncurkan proses guest: " << entrypoint << "\033[0m\n";
    pid_t guest_pid = dispatcher.spawn_guest(entrypoint, args);

    if (guest_pid <= 0) {
        std::cerr << "\033[1;31m[-] Gagal meluncurkan proses guest!\033[0m\n";
        return 1;
    }

    std::cout << "\033[1;32m[+] Guest aktif (PID: " << guest_pid << "). Memasuki Dispatcher Event Loop...\033[0m\n";
    dispatcher.run_loop(guest_pid);

    std::cout << "[*] Virtual Machine selesai dijalankan.\n";
    prop_service.stop();
    dns_proxy.stop();
    audio.stop();
    return 0;
}
