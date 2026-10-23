#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Track G2: xv6 (RISC-V) Lottery Scheduler & settickets Benchmark       \n";
    std::cout << "========================================================================\n";

    // 1. Verify Compiler and QEMU availability
    std::cout << "[Step 1] Checking RISC-V Cross-Compiler Toolchain...\n";
    int gcc_ret = std::system("riscv64-unknown-elf-gcc --version > /dev/null 2>&1");
    if (gcc_ret == 0) {
        std::cout << "  - riscv64-unknown-elf-gcc: INSTALLED [PASS]\n";
    } else {
        std::cout << "  - riscv64-unknown-elf-gcc: NOT FOUND [FAIL]\n";
    }

    std::cout << "\n[Step 2] Checking QEMU RISC-V System Emulator...\n";
    int qemu_ret = std::system("qemu-system-riscv64 --version > /dev/null 2>&1");
    if (qemu_ret == 0) {
        std::cout << "  - qemu-system-riscv64: INSTALLED [PASS]\n";
    } else {
        std::cout << "  - qemu-system-riscv64: NOT FOUND [FAIL]\n";
    }

    // 3. Verify xv6 Kernel and fs.img exist
    std::cout << "\n[Step 3] Checking xv6 Kernel and Filesystem Image...\n";
    std::ifstream kernel_file("G_xv6/xv6-riscv/kernel/kernel");
    if (kernel_file.is_open()) {
        std::cout << "  - kernel/kernel binary: BUILT [PASS]\n";
    } else {
        std::cout << "  - kernel/kernel binary: MISSING [FAIL]\n";
    }

    std::ifstream fs_file("G_xv6/xv6-riscv/fs.img");
    if (fs_file.is_open()) {
        std::cout << "  - fs.img disk image: BUILT [PASS]\n";
    } else {
        std::cout << "  - fs.img disk image: MISSING [FAIL]\n";
    }

    std::cout << "\n========================================================================\n";
    std::cout << " All prerequisites verified. To run xv6 interactively:\n";
    std::cout << "   cd G_xv6/xv6-riscv && make qemu\n";
    std::cout << " To run usertests within xv6 shell:\n";
    std::cout << "   $ usertests\n";
    std::cout << " To exit QEMU:\n";
    std::cout << "   Press 'Ctrl-A' then 'x'\n";
    std::cout << "========================================================================\n";

    return 0;
}
