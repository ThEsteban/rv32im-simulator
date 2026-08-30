#include "cpu.hpp"
#include <iostream>
#include <cassert>
#include <stdexcept>

void run_pipeline_test() {
    CPU<> cpu;
    std::cout << "Starting CPU basic pipeline tests...\n";

    // INSTRUCTION 1: LUI x1, 0x80000
    cpu.write_memory_word(0x80000000, 0x800000B7);

    // INSTRUCTION 2: ADDI x2, x0, 42
    cpu.write_memory_word(0x80000004, 0x02A00113);

    // INSTRUCTION 3: SW x2, 256(x1)  <-- CHANGED TO OFFSET 256
    // Stores 42 into memory address (0x80000000 + 256) = 0x80000100
    cpu.write_memory_word(0x80000008, 0x1020A023);

    // INSTRUCTION 4: LW x3, 256(x1)  <-- CHANGED TO OFFSET 256
    // Loads the word at memory address 0x80000100 into register x3
    cpu.write_memory_word(0x8000000C, 0x1000A183);

    // INSTRUCTION 5: ADD x4, x2, x3
    cpu.write_memory_word(0x80000010, 0x00310233);

    // --- EXECUTION & ASSERTIONS ---

    cpu.clk();
    assert(cpu.get_register(1) == 0x80000000 && "FAIL: LUI");
    std::cout << "[PASS] LUI executed correctly.\n";

    cpu.clk();
    assert(cpu.get_register(2) == 42 && "FAIL: ADDI");
    std::cout << "[PASS] ADDI executed correctly.\n";

    cpu.clk();
    // Check the new memory address 0x80000100!
    assert(cpu.read_memory_word(0x80000100) == 42 && "FAIL: SW");
    std::cout << "[PASS] SW executed correctly.\n";

    cpu.clk();
    assert(cpu.get_register(3) == 42 && "FAIL: LW");
    std::cout << "[PASS] LW executed correctly.\n";

    cpu.clk();
    assert(cpu.get_register(4) == 84 && "FAIL: ADD");
    std::cout << "[PASS] ADD executed correctly.\n";

    std::cout << "\n>>> ALL BASIC PIPELINE TESTS PASSED! <<<\n";
}

int main() {
    try {
        run_pipeline_test();
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] Exception caught during simulation: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
