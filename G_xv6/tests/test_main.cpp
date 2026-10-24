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

void test_benchmark_binaries() {
    std::cout << "[Test 4] Benchmark Binaries (_bench_cpu, _bench_io, _bench_mixed)..." << std::flush;
    std::ifstream cpu_bin("G_xv6/xv6-riscv/user/_bench_cpu", std::ios::binary);
    assert(cpu_bin.is_open());

    std::ifstream io_bin("G_xv6/xv6-riscv/user/_bench_io", std::ios::binary);
    assert(io_bin.is_open());

    std::ifstream mixed_bin("G_xv6/xv6-riscv/user/_bench_mixed", std::ios::binary);
    assert(mixed_bin.is_open());

    std::cout << " PASSED\n";
}

void test_cow_artifacts() {
    std::cout << "[Test 5] Copy-on-Write (COW) Kernel & User Artifacts..." << std::flush;
    std::ifstream cow_bin("G_xv6/xv6-riscv/user/_cowtest", std::ios::binary);
    assert(cow_bin.is_open());

    // Check PTE_COW in riscv.h
    std::ifstream riscv_hdr("G_xv6/xv6-riscv/kernel/riscv.h");
    assert(riscv_hdr.is_open());
    std::string line;
    bool found_cow = false;
    while (std::getline(riscv_hdr, line)) {
        if (line.find("PTE_COW") != std::string::npos) {
            found_cow = true;
            break;
        }
    }
    assert(found_cow);
    std::cout << " PASSED\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Running Track G4: xv6 Copy-on-Write (COW) Unit Tests    \n";
    std::cout << "=========================================================\n";

    test_toolchain_availability();
    test_xv6_binaries();
    test_lottery_scheduler_artifacts();
    test_benchmark_binaries();
    test_cow_artifacts();

    std::cout << "\n>>> All xv6 RISC-V Environment & COW Tests PASSED! <<<\n";
    return 0;
}
