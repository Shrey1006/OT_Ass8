#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdlib>

void test_toolchain_availability() {
    std::cout << "[Test 1] Toolchain Verification (GCC & QEMU)..." << std::flush;
    int ret_gcc = std::system("which riscv64-unknown-elf-gcc > /dev/null 2>&1");
    assert(ret_gcc == 0);

    int ret_qemu = std::system("which qemu-system-riscv64 > /dev/null 2>&1");
    assert(ret_qemu == 0);
    std::cout << " PASSED\n";
}

void test_xv6_binaries() {
    std::cout << "[Test 2] xv6 Kernel & Filesystem Existence..." << std::flush;
    std::ifstream kfile("G_xv6/xv6-riscv/kernel/kernel", std::ios::binary);
    assert(kfile.is_open());

    // Verify ELF Magic: 0x7F 'E' 'L' 'F'
    char magic[4];
    kfile.read(magic, 4);
    assert(magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F');

    std::ifstream fsfile("G_xv6/xv6-riscv/fs.img", std::ios::binary);
    assert(fsfile.is_open());
    std::cout << " PASSED\n";
}

void test_lottery_scheduler_artifacts() {
    std::cout << "[Test 3] Lottery Scheduler Syscall & User Binary..." << std::flush;
    std::ifstream lottery_bin("G_xv6/xv6-riscv/user/_lotterytest", std::ios::binary);
    assert(lottery_bin.is_open());

    // Check SYS_settickets in syscall.h
    std::ifstream sys_hdr("G_xv6/xv6-riscv/kernel/syscall.h");
    assert(sys_hdr.is_open());
    std::string line;
    bool found_sys = false;
    while (std::getline(sys_hdr, line)) {
        if (line.find("SYS_settickets") != std::string::npos) {
            found_sys = true;
            break;
        }
    }
    assert(found_sys);
    std::cout << " PASSED\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Running Track G2: xv6 Lottery Scheduler Unit Tests       \n";
    std::cout << "=========================================================\n";

    test_toolchain_availability();
    test_xv6_binaries();
    test_lottery_scheduler_artifacts();

    std::cout << "\n>>> All xv6 RISC-V Environment & Lottery Tests PASSED! <<<\n";
    return 0;
}
