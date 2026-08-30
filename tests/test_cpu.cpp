#include "cpu.hpp"
#include "bus.hpp"
#include <iostream>
#include <cassert>
#include <sstream>
#include <stdexcept>

template <typename Function>
void expect_guest_fault(Function&& function, GuestFaultCause expectedCause, uint32_t expectedMtval) {
    bool faulted = false;
    try {
        function();
    } catch (const GuestFault& fault) {
        faulted = true;
        assert(fault.cause() == static_cast<uint32_t>(expectedCause));
        assert(fault.mtval() == expectedMtval);
    }
    assert(faulted);
}

void run_pipeline_test() {
    CPU cpu;
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

void run_bus_ram_test() {
    std::ostringstream uartOutput;
    Bus bus(uartOutput);

    bus.store_word(0xA1B2C3D4, Memory::BASE_OFFSET);
    assert(bus.load_uw(Memory::BASE_OFFSET) == 0xA1B2C3D4);
    assert(bus.load_ubyte(Memory::BASE_OFFSET) == 0xD4);
    assert(bus.fetch_instruction(Memory::BASE_OFFSET) == 0xA1B2C3D4);
}

void run_mmio_test() {
    {
        std::ostringstream uartOutput;
        Bus bus(uartOutput);
        bus.store_byte('A', Bus::UART_TX);
        assert(uartOutput.str() == "A");
        assert(bus.load_ubyte(Bus::UART_LINE_STATUS) == 0x60);
    }

    {
        std::ostringstream uartOutput;
        Bus bus(uartOutput);
        bus.store_word(0, Bus::TEST_FINISHER);
        assert(bus.halted());
        assert(bus.exit_code() == 0);
    }

    {
        std::ostringstream uartOutput;
        Bus bus(uartOutput);
        bus.store_word(9, Bus::TEST_FINISHER);
        assert(bus.halted());
        assert(bus.exit_code() == 9);
    }
}

void run_bus_fault_test() {
    std::ostringstream uartOutput;
    Bus bus(uartOutput);
    constexpr uint32_t unmapped = 0x20000000;

    // MMIO and other low addresses are rejected before RAM address translation.
    expect_guest_fault([&] { static_cast<void>(bus.load_ubyte(Bus::UART_TX + 1)); },
                       GuestFaultCause::LoadAccessFault, Bus::UART_TX + 1);
    expect_guest_fault([&] { bus.store_byte(0, Bus::UART_TX + 1); },
                       GuestFaultCause::StoreAccessFault, Bus::UART_TX + 1);

    bool rejectedMmioProgram = false;
    try {
        bus.load_program(ProgramImage{Bus::UART_TX, {{Bus::UART_TX, {0x13, 0, 0, 0}, 4}}});
    } catch (const std::runtime_error&) {
        rejectedMmioProgram = true;
    }
    assert(rejectedMmioProgram);
    assert(uartOutput.str().empty());

    expect_guest_fault([&] { static_cast<void>(bus.fetch_instruction(unmapped)); },
                       GuestFaultCause::InstructionAccessFault, unmapped);
    expect_guest_fault([&] { static_cast<void>(bus.load_uw(unmapped)); },
                       GuestFaultCause::LoadAccessFault, unmapped);
    expect_guest_fault([&] { bus.store_word(0, unmapped); },
                       GuestFaultCause::StoreAccessFault, unmapped);

    expect_guest_fault([&] { static_cast<void>(bus.fetch_instruction(Memory::BASE_OFFSET + 2)); },
                       GuestFaultCause::InstructionAddressMisaligned, Memory::BASE_OFFSET + 2);
    expect_guest_fault([&] { static_cast<void>(bus.load_uhw(Memory::BASE_OFFSET + 1)); },
                       GuestFaultCause::LoadAddressMisaligned, Memory::BASE_OFFSET + 1);
    expect_guest_fault([&] { static_cast<void>(bus.load_uw(Memory::BASE_OFFSET + 2)); },
                       GuestFaultCause::LoadAddressMisaligned, Memory::BASE_OFFSET + 2);
    expect_guest_fault([&] { bus.store_hw(0, Memory::BASE_OFFSET + 1); },
                       GuestFaultCause::StoreAddressMisaligned, Memory::BASE_OFFSET + 1);
    expect_guest_fault([&] { bus.store_word(0, Memory::BASE_OFFSET + 2); },
                       GuestFaultCause::StoreAddressMisaligned, Memory::BASE_OFFSET + 2);

    expect_guest_fault([&] { static_cast<void>(bus.load_ubyte(Bus::UART_TX)); },
                       GuestFaultCause::LoadAccessFault, Bus::UART_TX);
    expect_guest_fault([&] { bus.store_byte(0, Bus::UART_LINE_STATUS); },
                       GuestFaultCause::StoreAccessFault, Bus::UART_LINE_STATUS);
    expect_guest_fault([&] { static_cast<void>(bus.load_uhw(Bus::UART_TX)); },
                       GuestFaultCause::LoadAccessFault, Bus::UART_TX);
    expect_guest_fault([&] { bus.store_word(0, Bus::UART_TX); },
                       GuestFaultCause::StoreAccessFault, Bus::UART_TX);
    expect_guest_fault([&] { static_cast<void>(bus.load_uw(Bus::TEST_FINISHER)); },
                       GuestFaultCause::LoadAccessFault, Bus::TEST_FINISHER);
    expect_guest_fault([&] { bus.store_byte(0, Bus::TEST_FINISHER); },
                       GuestFaultCause::StoreAccessFault, Bus::TEST_FINISHER);
}

void run_system_fault_test() {
    std::ostringstream uartOutput;
    CPU cpu(uartOutput);
    DecodedInstruction systemInstruction;
    systemInstruction.opcode = 0x73;
    systemInstruction.funct3 = 0;

    systemInstruction.imm = 0;
    expect_guest_fault([&] { static_cast<void>(cpu.execute(systemInstruction, Memory::BASE_OFFSET)); },
                       GuestFaultCause::MachineModeEcall, 0);

    systemInstruction.imm = 1;
    expect_guest_fault([&] { static_cast<void>(cpu.execute(systemInstruction, Memory::BASE_OFFSET)); },
                       GuestFaultCause::Breakpoint, 0);
}

void run_uart_guest_test() {
    std::ostringstream uartOutput;
    CPU cpu(uartOutput);

    // LUI x1, 0x10000; ADDI x2, x0, 'A'; SB x2, 0(x1)
    cpu.write_memory_word(0x80000000, 0x100000B7);
    cpu.write_memory_word(0x80000004, 0x04100113);
    cpu.write_memory_word(0x80000008, 0x00208023);
    // LUI x3, 0x00100; SW x0, 0(x3)
    cpu.write_memory_word(0x8000000C, 0x001001B7);
    cpu.write_memory_word(0x80000010, 0x0001A023);

    for (int instructionCount = 0; instructionCount < 5 && !cpu.halted(); ++instructionCount) {
        cpu.clk();
    }

    assert(uartOutput.str() == "A");
    assert(cpu.halted());
    assert(cpu.exit_code() == 0);
}

int main() {
    try {
        run_pipeline_test();
        run_bus_ram_test();
        run_mmio_test();
        run_bus_fault_test();
        run_system_fault_test();
        run_uart_guest_test();
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] Exception caught during simulation: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
